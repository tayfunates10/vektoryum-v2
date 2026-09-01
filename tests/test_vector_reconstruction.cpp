#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

#include "vektoryum/vector/path_quality.hpp"
#include "vektoryum/vector/reconstruction.hpp"

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
}  // namespace

int run_vector_reconstruction_tests() {
    using namespace vektoryum::vector;

    std::vector<std::uint8_t> rectangle(6U * 5U, 0U);
    for (std::uint32_t y = 1U; y < 4U; ++y) {
        for (std::uint32_t x = 2U; x < 5U; ++x) {
            rectangle[static_cast<std::size_t>(y) * 6U + x] = 255U;
        }
    }
    const auto rect = reconstruct_binary_mask(rectangle, 6U, 5U);
    expect_true(rect.ok(), "rectangle contour reconstruction succeeds");
    expect_true(rect.scene.paths.size() == 1U, "rectangle produces one closed path");
    expect_true(rect.scene.paths[0].points.size() == 4U, "collinear rectangle nodes simplify to four corners");
    const auto rect_raster = rasterize_even_odd(rect.scene, 6U, 5U);
    expect_true(binary_iou(rectangle, rect_raster) == 1.0, "rectangle rasterize-back IoU is exact");

    const auto rect_quality = certify_scene(rect.scene, rectangle, 6U, 5U);
    expect_true(rect_quality.passed() && rect_quality.raster_iou == 1.0 &&
                    rect_quality.disagreement_ratio == 0.0 && rect_quality.self_intersections == 0U,
                "rectangle passes vector fidelity certification");

    std::vector<std::uint8_t> soft_alpha(5U * 5U, 0U);
    soft_alpha[1U * 5U + 1U] = 127U;
    soft_alpha[1U * 5U + 2U] = 128U;
    soft_alpha[1U * 5U + 3U] = 127U;
    soft_alpha[2U * 5U + 1U] = 127U;
    soft_alpha[2U * 5U + 2U] = 255U;
    soft_alpha[2U * 5U + 3U] = 128U;
    soft_alpha[3U * 5U + 1U] = 127U;
    soft_alpha[3U * 5U + 2U] = 128U;
    soft_alpha[3U * 5U + 3U] = 127U;
    std::vector<std::uint8_t> canonical_soft_alpha(soft_alpha.size(), 0U);
    for (std::size_t i = 0U; i < soft_alpha.size(); ++i) {
        canonical_soft_alpha[i] = coverage_is_foreground(soft_alpha[i]) ? 255U : 0U;
    }
    const auto soft_alpha_result = reconstruct_binary_mask(soft_alpha, 5U, 5U);
    expect_true(soft_alpha_result.ok(), "soft-alpha coverage reconstructs with canonical threshold semantics");
    const auto soft_alpha_raster = rasterize_even_odd(soft_alpha_result.scene, 5U, 5U);
    expect_true(binary_iou(canonical_soft_alpha, soft_alpha_raster) == 1.0,
                "soft-alpha rasterize-back matches the same canonical >=128 coverage definition");

    std::vector<std::uint8_t> donut(7U * 7U, 0U);
    for (std::uint32_t y = 1U; y < 6U; ++y) {
        for (std::uint32_t x = 1U; x < 6U; ++x) {
            donut[static_cast<std::size_t>(y) * 7U + x] = 255U;
        }
    }
    donut[3U * 7U + 3U] = 0U;
    const auto ring = reconstruct_binary_mask(donut, 7U, 7U);
    expect_true(ring.ok(), "donut contour reconstruction succeeds");
    expect_true(ring.scene.paths.size() == 2U, "donut preserves outer and hole contours");
    const auto donut_raster = rasterize_even_odd(ring.scene, 7U, 7U);
    expect_true(binary_iou(donut, donut_raster) == 1.0, "donut rasterize-back IoU is exact");
    expect_true(certify_scene(ring.scene, donut, 7U, 7U).passed(),
                "hole topology passes fidelity and self-intersection certification");

    std::vector<std::uint8_t> nested(11U * 11U, 0U);
    for (std::uint32_t y = 1U; y < 10U; ++y) {
        for (std::uint32_t x = 1U; x < 10U; ++x) {
            nested[static_cast<std::size_t>(y) * 11U + x] = 255U;
        }
    }
    for (std::uint32_t y = 3U; y < 8U; ++y) {
        for (std::uint32_t x = 3U; x < 8U; ++x) {
            nested[static_cast<std::size_t>(y) * 11U + x] = 0U;
        }
    }
    for (std::uint32_t y = 5U; y < 7U; ++y) {
        for (std::uint32_t x = 5U; x < 7U; ++x) {
            nested[static_cast<std::size_t>(y) * 11U + x] = 255U;
        }
    }
    const auto nested_result = reconstruct_binary_mask(nested, 11U, 11U);
    expect_true(nested_result.ok() && nested_result.scene.paths.size() == 3U,
                "nested outer-hole-island topology reconstructs three contours");
    const auto nested_quality = certify_scene(nested_result.scene, nested, 11U, 11U);
    expect_true(nested_quality.passed() && nested_quality.raster_iou == 1.0 &&
                    nested_quality.disagreement_ratio == 0.0,
                "nested outer-hole-island topology round-trips exactly");

    std::vector<std::uint8_t> thin_components(14U * 9U, 0U);
    for (std::uint32_t x = 1U; x < 10U; ++x) {
        thin_components[2U * 14U + x] = 255U;
    }
    for (std::uint32_t y = 2U; y < 7U; ++y) {
        thin_components[static_cast<std::size_t>(y) * 14U + 9U] = 255U;
    }
    for (std::uint32_t y = 5U; y < 8U; ++y) {
        for (std::uint32_t x = 11U; x < 13U; ++x) {
            thin_components[static_cast<std::size_t>(y) * 14U + x] = 255U;
        }
    }
    const auto thin_result = reconstruct_binary_mask(thin_components, 14U, 9U);
    expect_true(thin_result.ok() && thin_result.scene.paths.size() == 2U,
                "thin one-pixel corridor plus disconnected component reconstructs deterministically");
    const auto thin_quality = certify_scene(thin_result.scene, thin_components, 14U, 9U);
    expect_true(thin_quality.passed() && thin_quality.raster_iou == 1.0 &&
                    thin_quality.disagreement_ratio == 0.0,
                "thin and disconnected adversarial components round-trip exactly");

    const auto deterministic_a = reconstruct_binary_mask(donut, 7U, 7U);
    const auto deterministic_b = reconstruct_binary_mask(donut, 7U, 7U);
    expect_true(deterministic_a.ok() && deterministic_b.ok() &&
                    deterministic_a.scene.paths.size() == deterministic_b.scene.paths.size() &&
                    deterministic_a.scene.paths[0].points == deterministic_b.scene.paths[0].points,
                "vector reconstruction is deterministic");

    ReconstructionOptions tiny_budget{};
    tiny_budget.max_nodes = 2U;
    expect_true(reconstruct_binary_mask(rectangle, 6U, 5U, tiny_budget).error ==
                    ReconstructionError::NodeBudgetExceeded,
                "node budget fails closed");

    std::vector<std::uint8_t> checkerboard(64U * 64U, 0U);
    for (std::uint32_t y = 0U; y < 64U; ++y) {
        for (std::uint32_t x = 0U; x < 64U; ++x) {
            if (((x + y) & 1U) == 0U) {
                checkerboard[static_cast<std::size_t>(y) * 64U + x] = 255U;
            }
        }
    }
    ReconstructionOptions adversarial_budget{};
    adversarial_budget.max_nodes = 8U;
    expect_true(reconstruct_binary_mask(checkerboard, 64U, 64U, adversarial_budget).error ==
                    ReconstructionError::NodeBudgetExceeded,
                "adversarial boundary extraction respects node budget immediately");

    VectorScene bow_tie{};
    bow_tie.width = 4U;
    bow_tie.height = 4U;
    Path crossing{};
    crossing.points = {{0, 0}, {4, 4}, {0, 4}, {4, 0}};
    bow_tie.paths.push_back(crossing);
    expect_true(path_has_self_intersection(bow_tie.paths[0]),
                "crossing polygon is detected as self-intersecting");
    const std::vector<std::uint8_t> bow_reference(16U, 0U);
    expect_true(certify_scene(bow_tie, bow_reference, 4U, 4U).error == PathQualityError::SelfIntersection,
                "self-intersecting scene fails certification before fidelity acceptance");

    VectorScene stepped{};
    stepped.width = 6U;
    stepped.height = 5U;
    Path stepped_path{};
    stepped_path.points = {{2, 1}, {5, 1}, {5, 4}, {4, 4}, {4, 3}, {2, 3}};
    stepped.paths.push_back(stepped_path);
    const auto stepped_mask = rasterize_even_odd(stepped, 6U, 5U);
    PathQualityOptions permissive{};
    permissive.min_iou = 0.80;
    permissive.max_disagreement_ratio = 0.20;
    const auto simplified = simplify_scene_fidelity_gated(stepped, stepped_mask, 6U, 5U, permissive);
    expect_true(simplified.ok() && simplified.removed_nodes > 0U &&
                    simplified.quality.raster_iou >= permissive.min_iou &&
                    simplified.quality.disagreement_ratio <= permissive.max_disagreement_ratio,
                "path simplification removes nodes only while fidelity gates remain satisfied");
    expect_true(!path_has_self_intersection(simplified.scene.paths[0]),
                "fidelity-gated simplification cannot introduce self intersections");

    PathQualityOptions exact_gate{};
    exact_gate.min_iou = 1.0;
    exact_gate.max_disagreement_ratio = 0.0;
    const auto exact_simplified = simplify_scene_fidelity_gated(rect.scene, rectangle, 6U, 5U, exact_gate);
    expect_true(exact_simplified.ok() && exact_simplified.removed_nodes == 0U &&
                    exact_simplified.scene.paths[0].points.size() == 4U,
                "exact fidelity gate preserves irreducible rectangle corners");

    PathQualityOptions cert_budget{};
    cert_budget.max_certification_pixels = 8U;
    expect_true(certify_scene(rect.scene, rectangle, 6U, 5U, cert_budget).error ==
                    PathQualityError::CertificationBudgetExceeded,
                "rasterize-back certification obeys explicit pixel budget");

    PathQualityOptions node_quality_budget{};
    node_quality_budget.max_nodes = 3U;
    expect_true(certify_scene(rect.scene, rectangle, 6U, 5U, node_quality_budget).error ==
                    PathQualityError::InvalidScene,
                "quality certification enforces total node complexity budget");

    expect_true(reconstruct_binary_mask(rectangle, 0U, 5U).error == ReconstructionError::ZeroDimension,
                "zero dimensions are rejected");
    expect_true(reconstruct_binary_mask(std::span<const std::uint8_t>(rectangle.data(), rectangle.size() - 1U),
                                        6U, 5U).error == ReconstructionError::SizeMismatch,
                "mask size mismatch is rejected");

    const std::vector<std::uint8_t> empty(4U, 0U);
    expect_true(reconstruct_binary_mask(empty, 2U, 2U).error == ReconstructionError::NoForeground,
                "empty foreground fails closed");

    const std::vector<std::uint8_t> diagonal{255U, 0U, 0U, 255U};
    const auto diagonal_a = reconstruct_binary_mask(diagonal, 2U, 2U);
    const auto diagonal_b = reconstruct_binary_mask(diagonal, 2U, 2U);
    expect_true(diagonal_a.ok() && diagonal_b.ok() && diagonal_a.scene.paths.size() == 2U &&
                    diagonal_b.scene.paths.size() == diagonal_a.scene.paths.size() &&
                    diagonal_b.scene.paths[0].points == diagonal_a.scene.paths[0].points &&
                    diagonal_b.scene.paths[1].points == diagonal_a.scene.paths[1].points,
                "diagonal-only touching components resolve into deterministic contours");
    const auto diagonal_raster = rasterize_even_odd(diagonal_a.scene, 2U, 2U);
    const auto diagonal_quality = certify_scene(diagonal_a.scene, diagonal, 2U, 2U);
    expect_true(binary_iou(diagonal, diagonal_raster) == 1.0 && diagonal_quality.passed() &&
                    diagonal_quality.raster_iou == 1.0 && diagonal_quality.disagreement_ratio == 0.0,
                "diagonal saddle topology round-trips with exact fidelity");

    const std::vector<std::uint8_t> jpeg_like_saddle{201U, 74U, 91U, 188U};
    const std::vector<std::uint8_t> jpeg_like_reference{255U, 0U, 0U, 255U};
    const auto jpeg_like_a = reconstruct_binary_mask(jpeg_like_saddle, 2U, 2U);
    const auto jpeg_like_b = reconstruct_binary_mask(jpeg_like_saddle, 2U, 2U);
    expect_true(jpeg_like_a.ok() && jpeg_like_b.ok() && jpeg_like_a.scene.paths.size() == 2U &&
                    jpeg_like_b.scene.paths.size() == jpeg_like_a.scene.paths.size() &&
                    jpeg_like_b.scene.paths[0].points == jpeg_like_a.scene.paths[0].points &&
                    jpeg_like_b.scene.paths[1].points == jpeg_like_a.scene.paths[1].points,
                "JPEG-like saddle coverage resolves deterministically at the canonical threshold");
    const auto jpeg_like_raster = rasterize_even_odd(jpeg_like_a.scene, 2U, 2U);
    const auto jpeg_like_quality = certify_scene(jpeg_like_a.scene, jpeg_like_reference, 2U, 2U);
    expect_true(binary_iou(jpeg_like_reference, jpeg_like_raster) == 1.0 && jpeg_like_quality.passed() &&
                    jpeg_like_quality.raster_iou == 1.0 && jpeg_like_quality.disagreement_ratio == 0.0,
                "JPEG-like saddle topology round-trips with exact canonical fidelity");

    const std::span<const std::uint8_t> none{};
    expect_true(reconstruct_binary_mask(
                    none, std::numeric_limits<std::uint32_t>::max(), 1U).error ==
                    ReconstructionError::CoordinateOverflow,
                "coordinates beyond signed geometry range are rejected before tracing");

    return failures;
}
