#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <utility>
#include <vector>

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

namespace detail {

struct SourceColorBucket {
    std::uint32_t key{};
    std::uint64_t weight{};
    std::uint64_t weighted_r{};
    std::uint64_t weighted_g{};
    std::uint64_t weighted_b{};

    [[nodiscard]] std::array<std::uint8_t, 3U> rgb() const noexcept {
        if (weight == 0U) {
            return {0U, 0U, 0U};
        }
        const auto rounded = [this](std::uint64_t value) noexcept {
            return static_cast<std::uint8_t>((value + (weight / 2U)) / weight);
        };
        return {rounded(weighted_r), rounded(weighted_g), rounded(weighted_b)};
    }
};

[[nodiscard]] inline std::uint32_t source_color_key(
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b) noexcept {
    // Five bits per channel retain source fill identity while grouping only the
    // small interpolation variations normally introduced at antialiased edges.
    return (static_cast<std::uint32_t>(r >> 3U) << 10U) |
           (static_cast<std::uint32_t>(g >> 3U) << 5U) |
           static_cast<std::uint32_t>(b >> 3U);
}

[[nodiscard]] inline std::uint32_t color_distance_squared(
    const std::array<std::uint8_t, 3U>& lhs,
    const std::array<std::uint8_t, 3U>& rhs) noexcept {
    const auto dr = static_cast<std::int32_t>(lhs[0]) - static_cast<std::int32_t>(rhs[0]);
    const auto dg = static_cast<std::int32_t>(lhs[1]) - static_cast<std::int32_t>(rhs[1]);
    const auto db = static_cast<std::int32_t>(lhs[2]) - static_cast<std::int32_t>(rhs[2]);
    return static_cast<std::uint32_t>(dr * dr + dg * dg + db * db);
}

}  // namespace detail

// Attaches deterministic source paint metadata without changing the canonical
// scene geometry. Multiple source color regions are partitioned into bounded,
// deterministic paint masks, reconstructed independently, and serialized as
// separate compound layers. The original scene.paths remain the authority for
// alpha fidelity certification; paint-layer construction is fail-closed.
[[nodiscard]] inline bool attach_source_fill_rgb(
    SvgScene& scene,
    std::span<const std::uint8_t> rgba8,
    std::span<const std::uint8_t> coverage) {
    const auto source_fill = derive_source_fill_rgb(rgba8, coverage);
    if (!source_fill.valid || scene.width == 0U || scene.height == 0U || scene.paths.empty()) {
        return false;
    }

    scene.fill_rgb = source_fill.rgb;

    std::map<std::uint32_t, detail::SourceColorBucket> histogram;
    for (std::size_t pixel = 0U; pixel < coverage.size(); ++pixel) {
        const std::uint8_t alpha = coverage[pixel];
        if (!coverage_is_foreground(alpha)) {
            continue;
        }
        const std::size_t base = pixel * 4U;
        const std::uint8_t r = rgba8[base];
        const std::uint8_t g = rgba8[base + 1U];
        const std::uint8_t b = rgba8[base + 2U];
        const std::uint32_t key = detail::source_color_key(r, g, b);
        auto& bucket = histogram[key];
        bucket.key = key;
        const std::uint64_t weight = alpha;
        bucket.weight += weight;
        bucket.weighted_r += static_cast<std::uint64_t>(r) * weight;
        bucket.weighted_g += static_cast<std::uint64_t>(g) * weight;
        bucket.weighted_b += static_cast<std::uint64_t>(b) * weight;
    }
    if (histogram.empty()) {
        return false;
    }

    std::vector<detail::SourceColorBucket> palette;
    palette.reserve(histogram.size());
    for (const auto& [key, bucket] : histogram) {
        static_cast<void>(key);
        palette.push_back(bucket);
    }
    std::sort(
        palette.begin(),
        palette.end(),
        [](const detail::SourceColorBucket& lhs, const detail::SourceColorBucket& rhs) {
            if (lhs.weight != rhs.weight) {
                return lhs.weight > rhs.weight;
            }
            return lhs.key < rhs.key;
        });

    constexpr std::size_t max_paint_layers{32U};
    if (palette.size() > max_paint_layers) {
        palette.resize(max_paint_layers);
    }

    if (palette.size() == 1U) {
        scene.paint_layers.clear();
        SvgPaintLayer layer{};
        layer.paths = scene.paths;
        layer.fill_rgb = palette.front().rgb();
        scene.fill_rgb = layer.fill_rgb;
        scene.paint_layers.push_back(std::move(layer));
        return true;
    }

    std::vector<std::array<std::uint8_t, 3U>> palette_rgb;
    palette_rgb.reserve(palette.size());
    for (const auto& bucket : palette) {
        palette_rgb.push_back(bucket.rgb());
    }

    std::vector<std::vector<std::uint8_t>> region_masks(
        palette.size(),
        std::vector<std::uint8_t>(coverage.size(), 0U));
    for (std::size_t pixel = 0U; pixel < coverage.size(); ++pixel) {
        const std::uint8_t alpha = coverage[pixel];
        if (!coverage_is_foreground(alpha)) {
            continue;
        }
        const std::size_t base = pixel * 4U;
        const std::array<std::uint8_t, 3U> source_rgb{
            rgba8[base],
            rgba8[base + 1U],
            rgba8[base + 2U],
        };
        std::size_t best = 0U;
        std::uint32_t best_distance = detail::color_distance_squared(source_rgb, palette_rgb.front());
        for (std::size_t candidate = 1U; candidate < palette_rgb.size(); ++candidate) {
            const std::uint32_t distance = detail::color_distance_squared(source_rgb, palette_rgb[candidate]);
            if (distance < best_distance) {
                best = candidate;
                best_distance = distance;
            }
        }
        region_masks[best][pixel] = alpha;
    }

    std::vector<SvgPaintLayer> layers;
    layers.reserve(palette.size());
    for (std::size_t index = 0U; index < region_masks.size(); ++index) {
        const auto reconstructed = reconstruct_binary_mask(
            region_masks[index],
            scene.width,
            scene.height);
        if (!reconstructed.ok()) {
            return false;
        }
        const auto fitted = fit_svg_paths(reconstructed.scene);
        if (!fitted.ok() || fitted.scene.paths.empty()) {
            return false;
        }
        SvgPaintLayer layer{};
        layer.paths = fitted.scene.paths;
        layer.fill_rgb = palette_rgb[index];
        layers.push_back(std::move(layer));
    }

    scene.paint_layers = std::move(layers);
    return !scene.paint_layers.empty();
}

}  // namespace vektoryum::vector
