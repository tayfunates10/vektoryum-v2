#pragma once

#include <cstdint>
#include <span>

namespace vektoryum::analysis {

enum class ContentKind : std::uint8_t {
    Photo,
    Logo,
    LineArt,
    Mixed,
    Uncertain,
};

enum class ProcessingRoute : std::uint8_t {
    PhotoRestoration,
    VectorReconstruction,
    Hybrid,
    ConservativeRaster,
};

struct ContentFeatures {
    double edge_density{};
    double flat_region_ratio{};
    double color_complexity{};
    double texture_energy{};
    double alpha_transition_ratio{};
};

struct ContentAnalysis {
    ContentKind kind{ContentKind::Uncertain};
    ProcessingRoute route{ProcessingRoute::ConservativeRaster};
    double confidence{};
    ContentFeatures features{};
    bool valid{};
};

[[nodiscard]] ContentAnalysis classify_features(const ContentFeatures& features) noexcept;

[[nodiscard]] ContentAnalysis analyze_rgb_f32(
    std::span<const float> pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t channels) noexcept;

}  // namespace vektoryum::analysis
