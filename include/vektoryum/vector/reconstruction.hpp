#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vektoryum::vector {

inline constexpr std::uint8_t canonical_coverage_threshold{128U};

[[nodiscard]] constexpr bool coverage_is_foreground(
    std::uint8_t value,
    std::uint8_t threshold = canonical_coverage_threshold) noexcept {
    return value >= threshold;
}

static_assert(!coverage_is_foreground(0U));
static_assert(!coverage_is_foreground(127U));
static_assert(coverage_is_foreground(128U));
static_assert(coverage_is_foreground(255U));

struct IntPoint {
    std::int32_t x{};
    std::int32_t y{};

    [[nodiscard]] friend constexpr bool operator==(const IntPoint&, const IntPoint&) noexcept = default;
};

struct Path {
    std::vector<IntPoint> points{};
    bool closed{true};
    bool hole{false};
};

struct VectorScene {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<Path> paths{};
};

enum class ReconstructionError : std::uint8_t {
    None,
    ZeroDimension,
    SizeMismatch,
    SizeOverflow,
    CoordinateOverflow,
    NoForeground,
    NodeBudgetExceeded,
    TopologyAmbiguity,
    BrokenContour,
};

struct ReconstructionOptions {
    std::uint8_t threshold{canonical_coverage_threshold};
    std::size_t max_nodes{1'000'000U};
};

struct ReconstructionResult {
    ReconstructionError error{ReconstructionError::None};
    VectorScene scene{};

    [[nodiscard]] bool ok() const noexcept { return error == ReconstructionError::None; }
};

[[nodiscard]] ReconstructionResult reconstruct_binary_mask(
    std::span<const std::uint8_t> coverage,
    std::uint32_t width,
    std::uint32_t height,
    ReconstructionOptions options = {}) noexcept;

[[nodiscard]] std::vector<std::uint8_t> rasterize_even_odd(
    const VectorScene& scene,
    std::uint32_t width,
    std::uint32_t height);

[[nodiscard]] double binary_iou(
    std::span<const std::uint8_t> lhs,
    std::span<const std::uint8_t> rhs) noexcept;

}  // namespace vektoryum::vector
