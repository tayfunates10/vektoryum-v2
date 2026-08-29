#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

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
    expect_true(reconstruct_binary_mask(rectangle, 0U, 5U).error == ReconstructionError::ZeroDimension,
                "zero dimensions are rejected");
    expect_true(reconstruct_binary_mask(std::span<const std::uint8_t>(rectangle.data(), rectangle.size() - 1U),
                                        6U, 5U).error == ReconstructionError::SizeMismatch,
                "mask size mismatch is rejected");

    const std::vector<std::uint8_t> empty(4U, 0U);
    expect_true(reconstruct_binary_mask(empty, 2U, 2U).error == ReconstructionError::NoForeground,
                "empty foreground fails closed");

    const std::vector<std::uint8_t> diagonal{255U, 0U, 0U, 255U};
    expect_true(reconstruct_binary_mask(diagonal, 2U, 2U).error == ReconstructionError::TopologyAmbiguity,
                "diagonal-only touching components fail closed as topology ambiguity");

    const std::span<const std::uint8_t> none{};
    expect_true(reconstruct_binary_mask(
                    none, std::numeric_limits<std::uint32_t>::max(), 1U).error ==
                    ReconstructionError::CoordinateOverflow,
                "coordinates beyond signed geometry range are rejected before tracing");

    return failures;
}
