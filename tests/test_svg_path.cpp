#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "vektoryum/vector/curve_recovery.hpp"
#include "vektoryum/vector/svg_path.hpp"
#include "vektoryum/vector/upscale_reconstruction.hpp"

namespace {
int failures = 0;
void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    } else {
        std::cout << "[PASS] " << name << '\n';
    }
}

void fill_rectangle(std::vector<std::uint8_t>& pixels, std::uint32_t width,
                    std::uint32_t x0, std::uint32_t y0,
                    std::uint32_t x1, std::uint32_t y1,
                    std::uint8_t value) {
    for (std::uint32_t y = y0; y < y1; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            pixels[static_cast<std::size_t>(y) * width + x] = value;
        }
    }
}

vektoryum::resample::FloatImage make_upscale_fixture(
    std::uint32_t x0,
    std::uint32_t x1) {
    vektoryum::resample::FloatImage image{};
    image.width = 8U;
    image.height = 8U;
    image.channels = 4U;
    image.pixels.assign(8U * 8U * 4U, 0.0F);
    for (std::uint32_t y = 2U; y < 6U; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            const auto base = (static_cast<std::size_t>(y) * image.width + x) * 4U;
            image.pixels[base] = 1.0F;
            image.pixels[base + 3U] = 1.0F;
        }
    }
    return image;
}
}

int run_svg_path_tests() {
    using namespace vektoryum::vector;

    VectorScene rectangle{};
    rectangle.width = 16U;
    rectangle.height = 16U;
    rectangle.paths.push_back({{{2, 2}, {12, 2}, {12, 10}, {2, 10}}, true, false});

    const auto exact = fit_svg_paths(rectangle, {.corner_radius = 0.0});
    expect_true(exact.ok(), "polygon converts to SVG path");
    expect_true(exact.scene.paths[0].commands.size() == 5U,
                "exact polygon emits move lines and close");

    const auto rounded = fit_svg_paths(rectangle, {.corner_radius = 1.0});
    expect_true(rounded.ok(), "controlled corner fitting succeeds");
    bool has_cubic = false;
    for (const auto& command : rounded.scene.paths[0].commands) {
        has_cubic = has_cubic || command.type == SvgCommandType::CubicTo;
    }
    expect_true(has_cubic, "rounded path emits cubic segments");

    const auto rounded_repeat = fit_svg_paths(rectangle, {.corner_radius = 1.0});
    expect_true(serialize_svg_path_data(rounded.scene.paths[0]) ==
                    serialize_svg_path_data(rounded_repeat.scene.paths[0]),
                "curve fitting is deterministic");

    std::vector<std::uint8_t> rectangle_pixels(16U * 16U, 0U);
    fill_rectangle(rectangle_pixels, 16U, 2U, 2U, 12U, 10U, 1U);
    const auto exact_quality = certify_svg_scene(exact.scene, rectangle_pixels, 16U, 16U);
    expect_true(exact_quality.passed() && exact_quality.raster_iou == 1.0,
                "exact SVG polygon passes raster certification");
    const auto rounded_quality = certify_svg_scene(rounded.scene, rectangle_pixels, 16U, 16U);
    expect_true(rounded_quality.passed(),
                "small rounded SVG candidate passes raster certification");

    const auto large_rounding = fit_svg_paths(rectangle, {.corner_radius = 4.0});
    const auto strict_quality = certify_svg_scene(
        large_rounding.scene, rectangle_pixels, 16U, 16U,
        {.min_iou = 1.0, .max_disagreement_ratio = 0.0});
    expect_true(strict_quality.error == SvgCertificationError::FidelityRejected,
                "geometry-changing curve candidate is rejected by exact gate");

    VectorScene donut = rectangle;
    donut.paths.push_back({{{5, 4}, {5, 8}, {9, 8}, {9, 4}}, true, true});
    const auto donut_svg = fit_svg_paths(donut);
    expect_true(donut_svg.ok() && donut_svg.scene.paths.size() == 2U &&
                    donut_svg.scene.paths[1].hole,
                "SVG scene preserves hole semantics");
    std::vector<std::uint8_t> donut_pixels = rectangle_pixels;
    fill_rectangle(donut_pixels, 16U, 5U, 4U, 9U, 8U, 0U);
    const auto donut_quality = certify_svg_scene(donut_svg.scene, donut_pixels, 16U, 16U);
    expect_true(donut_quality.passed() && donut_quality.raster_iou == 1.0,
                "SVG raster certification preserves holes exactly");

    VectorScene components{};
    components.width = 16U;
    components.height = 16U;
    components.paths.push_back({{{1, 1}, {5, 1}, {5, 5}, {1, 5}}, true, false});
    components.paths.push_back({{{10, 10}, {15, 10}, {15, 15}, {10, 15}}, true, false});
    const auto component_svg = fit_svg_paths(components);
    std::vector<std::uint8_t> component_pixels(16U * 16U, 0U);
    fill_rectangle(component_pixels, 16U, 1U, 1U, 5U, 5U, 1U);
    fill_rectangle(component_pixels, 16U, 10U, 10U, 15U, 15U, 1U);
    const auto component_quality = certify_svg_scene(component_svg.scene, component_pixels, 16U, 16U);
    expect_true(component_quality.passed() && component_quality.raster_iou == 1.0,
                "disconnected SVG components certify exactly");

    const auto budget_quality = certify_svg_scene(
        exact.scene, rectangle_pixels, 16U, 16U,
        {.max_certification_pixels = 64U});
    expect_true(budget_quality.error == SvgCertificationError::CertificationBudgetExceeded,
                "SVG certification obeys pixel budget");

    VectorScene open_scene = rectangle;
    open_scene.paths[0].closed = false;
    expect_true(fit_svg_paths(open_scene).error == SvgFitError::DegeneratePath,
                "open path is rejected");
    expect_true(fit_svg_paths(rectangle, {.corner_radius = -1.0}).error == SvgFitError::InvalidRadius,
                "negative radius is rejected");

    constexpr std::uint32_t circle_size = 128U;
    constexpr std::int32_t circle_center = 64;
    constexpr std::int32_t circle_radius = 40;
    std::vector<std::uint8_t> circle_coverage(circle_size * circle_size, 0U);
    std::vector<std::uint8_t> circle_reference(circle_size * circle_size, 0U);
    for (std::uint32_t y = 0U; y < circle_size; ++y) {
        for (std::uint32_t x = 0U; x < circle_size; ++x) {
            const auto dx = static_cast<std::int32_t>(x) - circle_center;
            const auto dy = static_cast<std::int32_t>(y) - circle_center;
            if ((dx * dx) + (dy * dy) <= circle_radius * circle_radius) {
                const auto index = static_cast<std::size_t>(y) * circle_size + x;
                circle_coverage[index] = 255U;
                circle_reference[index] = 1U;
            }
        }
    }

    const auto circle_vector = reconstruct_binary_mask(
        circle_coverage, circle_size, circle_size);
    expect_true(circle_vector.ok() && !circle_vector.scene.paths.empty(),
                "dense raster circle reconstructs before curve recovery");

    const auto recovered_circle = recover_curves_certified(
        circle_vector.scene, circle_reference, circle_size, circle_size);
    expect_true(recovered_circle.ok(),
                "curve recovery remains behind raster fidelity certification");
    expect_true(recovered_circle.used_curves,
                "certified smooth contour emits cubic Bezier geometry");
    expect_true(recovered_circle.reduced_nodes(),
                "certified smooth contour reduces source node count");
    expect_true(recovered_circle.certification.raster_iou >= 0.995 &&
                    recovered_circle.certification.disagreement_ratio <= 0.005,
                "curve recovery preserves committed fidelity thresholds");

    const auto recovered_repeat = recover_curves_certified(
        circle_vector.scene, circle_reference, circle_size, circle_size);
    expect_true(recovered_repeat.ok() &&
                    recovered_repeat.recovered_nodes == recovered_circle.recovered_nodes &&
                    serialize_svg_path_data(recovered_repeat.scene.paths[0]) ==
                        serialize_svg_path_data(recovered_circle.scene.paths[0]),
                "certified curve recovery is deterministic");

    const auto exact_gated_recovery = recover_curves_certified(
        rectangle, rectangle_pixels, 16U, 16U,
        {.simplify_tolerance = 8.0,
         .corner_radius = 8.0,
         .certification = {.min_iou = 1.0, .max_disagreement_ratio = 0.0}});
    expect_true(exact_gated_recovery.ok() &&
                    exact_gated_recovery.certification.raster_iou == 1.0 &&
                    exact_gated_recovery.certification.disagreement_ratio == 0.0,
                "exact-gated curve recovery never weakens fidelity gates");

    const auto upscale_left = make_upscale_fixture(1U, 4U);
    const auto upscale_right = make_upscale_fixture(4U, 7U);
    const auto reconstructed_left = reconstruct_from_upscaled_rgba(
        upscale_left, true, 4U, 4U);
    const auto reconstructed_right = reconstruct_from_upscaled_rgba(
        upscale_right, true, 4U, 4U);
    expect_true(reconstructed_left.valid && reconstructed_right.valid,
                "U6 reconstruction accepts actual upscale surfaces");
    expect_true(reconstructed_left.coverage.size() == 16U &&
                    reconstructed_right.coverage.size() == 16U,
                "U6 reconstruction derives source-grid coverage from upscale output");
    expect_true(!reconstructed_left.scene.paths.empty() &&
                    !reconstructed_right.scene.paths.empty() &&
                    serialize_svg_path_data(reconstructed_left.scene.paths.front()) !=
                        serialize_svg_path_data(reconstructed_right.scene.paths.front()),
                "changing only upscale output changes reconstructed production geometry");

    std::vector<std::uint8_t> upscale_reference(16U, 0U);
    for (std::size_t index = 0U; index < reconstructed_left.coverage.size(); ++index) {
        upscale_reference[index] = static_cast<std::uint8_t>(
            coverage_is_foreground(reconstructed_left.coverage[index]));
    }
    const auto upscale_quality = certify_svg_scene(
        reconstructed_left.scene, upscale_reference, 4U, 4U);
    expect_true(upscale_quality.passed() && upscale_quality.raster_iou >= 0.995 &&
                    upscale_quality.disagreement_ratio <= 0.005,
                "U6 upscale-derived reconstruction preserves committed fidelity gates");

    auto invalid_upscale = upscale_left;
    invalid_upscale.channels = 3U;
    expect_true(!reconstruct_from_upscaled_rgba(invalid_upscale, true, 4U, 4U).valid,
                "U6 upscale reconstruction fails closed on invalid surface contract");

    return failures;
}
