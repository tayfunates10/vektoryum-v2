#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "vektoryum/vector/svg_path.hpp"

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

    return failures;
}
