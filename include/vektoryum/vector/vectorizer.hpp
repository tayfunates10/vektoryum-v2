#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace vektoryum::vector {

struct PointI {
    std::int32_t x{0};
    std::int32_t y{0};

    friend bool operator==(const PointI&, const PointI&) = default;
};

struct Contour {
    std::vector<PointI> points{};
};

enum class VectorizeError : std::uint8_t {
    None = 0,
    ZeroDimension,
    MaskSizeMismatch,
    InvalidMaskValue,
    NoForeground,
    TopologyAmbiguity,
    CoordinateOverflow,
};

struct VectorizationResult {
    VectorizeError error{VectorizeError::None};
    std::vector<Contour> contours{};
    std::uint64_t foreground_pixels{0};
    std::uint64_t boundary_edges{0};

    [[nodiscard]] bool ok() const noexcept { return error == VectorizeError::None; }
};

struct FidelityMetrics {
    double iou{0.0};
    std::uint64_t source_foreground{0};
    std::uint64_t reconstructed_foreground{0};
    std::uint64_t intersection{0};
    std::uint64_t union_pixels{0};
};

// Traces a binary mask into deterministic grid-aligned closed contours.
// Mask values must be exactly 0 or 1. Diagonal-only junctions that create
// ambiguous contour continuation fail closed instead of inventing topology.
[[nodiscard]] VectorizationResult trace_binary_mask(
    std::span<const std::uint8_t> mask,
    std::uint32_t width,
    std::uint32_t height) noexcept;

// Removes redundant collinear vertices while preserving every corner and loop.
[[nodiscard]] Contour simplify_collinear(const Contour& contour);

// Rasterizes contours with an even-odd fill rule at pixel centers. This makes
// nested contours represent holes without requiring winding-orientation policy.
[[nodiscard]] std::vector<std::uint8_t> rasterize_even_odd(
    std::span<const Contour> contours,
    std::uint32_t width,
    std::uint32_t height);

[[nodiscard]] FidelityMetrics rasterize_back_fidelity(
    std::span<const std::uint8_t> source_mask,
    std::uint32_t width,
    std::uint32_t height,
    std::span<const Contour> contours);

}  // namespace vektoryum::vector
