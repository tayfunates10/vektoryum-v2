#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "vektoryum/vector/reconstruction.hpp"
#include "vektoryum/vector/svg_path.hpp"

namespace vektoryum::vector {

struct SourceFillRgbResult {
    bool valid{false};
    std::array<std::uint8_t, 3U> rgb{0U, 0U, 0U};
};

// Derives a deterministic source-space fill from pixels that belong to the
// canonical foreground. Coverage is used as a weight so soft-alpha edge color
// cannot dominate the interior paint estimate. This helper does not change the
// canonical >=128 foreground definition used by reconstruction/certification.
[[nodiscard]] inline SourceFillRgbResult derive_source_fill_rgb(
    std::span<const std::uint8_t> rgba8,
    std::span<const std::uint8_t> coverage) noexcept {
    SourceFillRgbResult result{};
    if (coverage.empty() || rgba8.size() != coverage.size() * 4U) {
        return result;
    }

    std::uint64_t weighted_r = 0U;
    std::uint64_t weighted_g = 0U;
    std::uint64_t weighted_b = 0U;
    std::uint64_t total_weight = 0U;
    for (std::size_t pixel = 0U; pixel < coverage.size(); ++pixel) {
        const std::uint8_t value = coverage[pixel];
        if (!coverage_is_foreground(value)) {
            continue;
        }
        const std::uint64_t weight = value;
        const std::size_t base = pixel * 4U;
        weighted_r += static_cast<std::uint64_t>(rgba8[base]) * weight;
        weighted_g += static_cast<std::uint64_t>(rgba8[base + 1U]) * weight;
        weighted_b += static_cast<std::uint64_t>(rgba8[base + 2U]) * weight;
        total_weight += weight;
    }

    if (total_weight == 0U) {
        return result;
    }

    const auto rounded_channel = [total_weight](std::uint64_t weighted) noexcept {
        return static_cast<std::uint8_t>((weighted + (total_weight / 2U)) / total_weight);
    };
    result.rgb = {
        rounded_channel(weighted_r),
        rounded_channel(weighted_g),
        rounded_channel(weighted_b),
    };
    result.valid = true;
    return result;
}

// Attaches deterministic source paint metadata without changing certified
// geometry. The compatibility fill is always populated; a scene that does not
// yet have explicit paint layers receives one compound layer containing the
// already-fitted paths. Existing multi-layer paint is never collapsed.
[[nodiscard]] inline bool attach_source_fill_rgb(
    SvgScene& scene,
    std::span<const std::uint8_t> rgba8,
    std::span<const std::uint8_t> coverage) {
    const auto source_fill = derive_source_fill_rgb(rgba8, coverage);
    if (!source_fill.valid) {
        return false;
    }

    scene.fill_rgb = source_fill.rgb;
    if (scene.paint_layers.empty() && !scene.paths.empty()) {
        SvgPaintLayer layer{};
        layer.paths = scene.paths;
        layer.fill_rgb = source_fill.rgb;
        scene.paint_layers.push_back(std::move(layer));
    }
    return true;
}

}  // namespace vektoryum::vector
