#include "vektoryum/analysis/content_analyzer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace vektoryum::analysis {
namespace {

[[nodiscard]] double clamp01(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] bool finite_features(const ContentFeatures& f) noexcept {
    return std::isfinite(f.edge_density) && std::isfinite(f.flat_region_ratio) &&
           std::isfinite(f.color_complexity) && std::isfinite(f.texture_energy) &&
           std::isfinite(f.alpha_transition_ratio);
}

[[nodiscard]] double luminance(const float* pixel) noexcept {
    return 0.2126 * static_cast<double>(pixel[0]) +
           0.7152 * static_cast<double>(pixel[1]) +
           0.0722 * static_cast<double>(pixel[2]);
}

}  // namespace

ContentAnalysis classify_features(const ContentFeatures& features) noexcept {
    ContentAnalysis result{};
    result.features = features;

    if (!finite_features(features)) {
        return result;
    }

    const ContentFeatures f{
        clamp01(features.edge_density),
        clamp01(features.flat_region_ratio),
        clamp01(features.color_complexity),
        clamp01(features.texture_energy),
        clamp01(features.alpha_transition_ratio),
    };
    result.features = f;
    result.valid = true;

    if (f.color_complexity <= 0.10 && f.flat_region_ratio >= 0.68 && f.edge_density >= 0.14) {
        result.kind = ContentKind::LineArt;
        result.route = ProcessingRoute::VectorReconstruction;
        result.confidence = clamp01(0.55 + 0.25 * f.flat_region_ratio + 0.20 * f.edge_density);
        return result;
    }

    if (f.color_complexity <= 0.20 && f.flat_region_ratio >= 0.58 && f.edge_density >= 0.04 &&
        f.texture_energy <= 0.18) {
        result.kind = ContentKind::Logo;
        result.route = ProcessingRoute::VectorReconstruction;
        result.confidence = clamp01(0.50 + 0.30 * f.flat_region_ratio + 0.20 * (1.0 - f.color_complexity));
        return result;
    }

    if (f.color_complexity >= 0.30 && f.texture_energy >= 0.08 && f.flat_region_ratio <= 0.52) {
        result.kind = ContentKind::Photo;
        result.route = ProcessingRoute::PhotoRestoration;
        result.confidence = clamp01(0.48 + 0.26 * f.color_complexity + 0.26 * f.texture_energy);
        return result;
    }

    if (f.color_complexity >= 0.18 && f.edge_density >= 0.08 && f.flat_region_ratio >= 0.30) {
        result.kind = ContentKind::Mixed;
        result.route = ProcessingRoute::Hybrid;
        result.confidence = clamp01(0.46 + 0.18 * f.edge_density + 0.18 * f.color_complexity +
                                    0.18 * f.flat_region_ratio);
        return result;
    }

    result.kind = ContentKind::Uncertain;
    result.route = ProcessingRoute::ConservativeRaster;
    result.confidence = 0.35;
    return result;
}

ContentAnalysis analyze_rgb_f32(
    std::span<const float> pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t channels) noexcept {
    ContentAnalysis invalid{};
    if (width == 0U || height == 0U || (channels != 3U && channels != 4U)) {
        return invalid;
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    if (pixel_count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / channels)) {
        return invalid;
    }
    const std::size_t required = static_cast<std::size_t>(pixel_count) * channels;
    if (pixels.size() != required) {
        return invalid;
    }

    std::array<bool, 512> occupied{};
    std::size_t occupied_count = 0U;
    std::uint64_t comparisons = 0U;
    std::uint64_t edges = 0U;
    std::uint64_t flats = 0U;
    std::uint64_t alpha_transitions = 0U;
    double texture_sum = 0.0;

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * channels;
            const float* p = pixels.data() + index;
            for (std::uint8_t c = 0U; c < channels; ++c) {
                if (!std::isfinite(p[c]) || p[c] < 0.0F || p[c] > 1.0F) {
                    return invalid;
                }
            }

            const auto q = [](float v) noexcept -> std::size_t {
                return static_cast<std::size_t>(std::min(7, static_cast<int>(v * 8.0F)));
            };
            const std::size_t bin = q(p[0]) * 64U + q(p[1]) * 8U + q(p[2]);
            if (!occupied[bin]) {
                occupied[bin] = true;
                ++occupied_count;
            }

            const double lum = luminance(p);
            if (x > 0U) {
                const float* left = p - channels;
                const double delta = std::abs(lum - luminance(left));
                ++comparisons;
                edges += delta >= 0.12 ? 1U : 0U;
                flats += delta <= 0.025 ? 1U : 0U;
                texture_sum += delta * delta;
                if (channels == 4U && std::abs(static_cast<double>(p[3] - left[3])) >= 0.10) {
                    ++alpha_transitions;
                }
            }
            if (y > 0U) {
                const float* up = pixels.data() +
                    ((static_cast<std::size_t>(y - 1U) * width + x) * channels);
                const double delta = std::abs(lum - luminance(up));
                ++comparisons;
                edges += delta >= 0.12 ? 1U : 0U;
                flats += delta <= 0.025 ? 1U : 0U;
                texture_sum += delta * delta;
                if (channels == 4U && std::abs(static_cast<double>(p[3] - up[3])) >= 0.10) {
                    ++alpha_transitions;
                }
            }
        }
    }

    const double denom = comparisons == 0U ? 1.0 : static_cast<double>(comparisons);
    const double palette_scale = std::min<double>(512.0, static_cast<double>(pixel_count));
    const ContentFeatures features{
        static_cast<double>(edges) / denom,
        static_cast<double>(flats) / denom,
        static_cast<double>(occupied_count) / palette_scale,
        clamp01(std::sqrt(texture_sum / denom) * 2.0),
        static_cast<double>(alpha_transitions) / denom,
    };
    return classify_features(features);
}

}  // namespace vektoryum::analysis
