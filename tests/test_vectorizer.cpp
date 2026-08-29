#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "vektoryum/vector/vectorizer.hpp"

namespace {
int failures = 0;

void expect_true(const bool condition, const std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    } else {
        std::cout << "[PASS] " << name << '\n';
    }
}
}

int run_vectorizer_tests() {
    using namespace vektoryum::vector;

    std::vector<std::uint8_t> rect(6U * 5U, 0U);
    for (std::uint32_t y = 1U; y < 4U; ++y) {
        for (std::uint32_t x = 2U; x < 5U; ++x) {
            rect[static_cast<std::size_t>(y) * 6U + x] = 1U;
        }
    }
    const auto rectangle = trace_binary_mask(rect, 6U, 5U);
    expect_true(rectangle.ok(), "rectangle vectorization succeeds");
    expect_true(rectangle.contours.size() == 1U, "rectangle has exactly one contour");
    expect_true(rectangle.contours.front().points.size() == 4U,
                "collinear simplification reduces rectangle to four corners");
    const auto rectangle_fidelity = rasterize_back_fidelity(rect, 6U, 5U, rectangle.contours);
    expect_true(rectangle_fidelity.iou == 1.0, "rectangle rasterize-back IoU is exact");

    std::vector<std::uint8_t> donut(7U * 7U, 0U);
    for (std::uint32_t y = 1U; y < 6U; ++y) {
        for (std::uint32_t x = 1U; x < 6U; ++x) {
            donut[static_cast<std::size_t>(y) * 7U + x] = 1U;
        }
    }
    for (std::uint32_t y = 2U; y < 5U; ++y) {
        for (std::uint32_t x = 2U; x < 5U; ++x) {
            donut[static_cast<std::size_t>(y) * 7U + x] = 0U;
        }
    }
    const auto ring = trace_binary_mask(donut, 7U, 7U);
    expect_true(ring.ok(), "donut vectorization succeeds");
    expect_true(ring.contours.size() == 2U, "donut preserves outer loop and hole loop");
    const auto donut_fidelity = rasterize_back_fidelity(donut, 7U, 7U, ring.contours);
    expect_true(donut_fidelity.iou == 1.0, "donut rasterize-back IoU is exact");

    const std::vector<std::uint8_t> diagonal{1U, 0U, 0U, 1U};
    const auto ambiguous = trace_binary_mask(diagonal, 2U, 2U);
    expect_true(ambiguous.error == VectorizeError::TopologyAmbiguity,
                "diagonal-only junction fails closed as topology ambiguity");

    const std::vector<std::uint8_t> invalid_value{0U, 2U, 0U, 0U};
    const auto invalid = trace_binary_mask(invalid_value, 2U, 2U);
    expect_true(invalid.error == VectorizeError::InvalidMaskValue,
                "non-binary mask value is rejected");

    const std::vector<std::uint8_t> empty(4U, 0U);
    const auto no_foreground = trace_binary_mask(empty, 2U, 2U);
    expect_true(no_foreground.error == VectorizeError::NoForeground,
                "empty mask is rejected as no foreground");

    const auto wrong_shape = trace_binary_mask(rect, 7U, 5U);
    expect_true(wrong_shape.error == VectorizeError::MaskSizeMismatch,
                "mask shape mismatch is rejected");

    const auto repeat = trace_binary_mask(rect, 6U, 5U);
    expect_true(repeat.ok() && repeat.contours == rectangle.contours,
                "vectorization is deterministic for identical input");

    return failures;
}
