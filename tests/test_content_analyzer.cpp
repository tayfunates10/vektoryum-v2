#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

#include "vektoryum/analysis/content_analyzer.hpp"

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
}

int run_content_analyzer_tests() {
    using namespace vektoryum::analysis;

    const auto line = classify_features({0.30, 0.82, 0.04, 0.05, 0.0});
    expect_true(line.valid && line.kind == ContentKind::LineArt,
                "line-art feature vector routes to line-art");
    expect_true(line.route == ProcessingRoute::VectorReconstruction,
                "line-art uses vector reconstruction");

    const auto logo = classify_features({0.10, 0.74, 0.12, 0.08, 0.02});
    expect_true(logo.kind == ContentKind::Logo && logo.route == ProcessingRoute::VectorReconstruction,
                "logo feature vector uses vector route");

    const auto photo = classify_features({0.18, 0.24, 0.58, 0.31, 0.01});
    expect_true(photo.kind == ContentKind::Photo && photo.route == ProcessingRoute::PhotoRestoration,
                "photo feature vector uses photo restoration");

    const auto mixed = classify_features({0.18, 0.46, 0.36, 0.12, 0.03});
    expect_true(mixed.kind == ContentKind::Mixed && mixed.route == ProcessingRoute::Hybrid,
                "mixed feature vector uses hybrid route");

    const auto alpha_defined = classify_features({0.0, 0.90, 0.06, 0.0, 0.18});
    expect_true(alpha_defined.route == ProcessingRoute::VectorReconstruction,
                "alpha-defined silhouette uses vector reconstruction");

    const auto uncertain = classify_features({0.01, 0.40, 0.05, 0.01, 0.0});
    expect_true(uncertain.kind == ContentKind::Uncertain &&
                    uncertain.route == ProcessingRoute::ConservativeRaster,
                "uncertain input uses conservative fallback");

    ContentFeatures bad{};
    bad.edge_density = std::numeric_limits<double>::quiet_NaN();
    const auto invalid_features = classify_features(bad);
    expect_true(!invalid_features.valid && invalid_features.route == ProcessingRoute::ConservativeRaster,
                "non-finite feature vector is rejected safely");

    std::vector<float> flat_rgb(4U * 4U * 3U, 0.5F);
    const auto first = analyze_rgb_f32(flat_rgb, 4U, 4U, 3U);
    const auto second = analyze_rgb_f32(flat_rgb, 4U, 4U, 3U);
    expect_true(first.valid && second.valid && first.kind == second.kind &&
                    first.route == second.route && first.confidence == second.confidence,
                "analysis is deterministic for identical input");
    expect_true(first.route == ProcessingRoute::ConservativeRaster,
                "ambiguous flat image remains conservative");

    std::vector<float> alpha_silhouette(4U * 4U * 4U, 0.5F);
    for (std::uint32_t y = 0U; y < 4U; ++y) {
        for (std::uint32_t x = 0U; x < 4U; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * 4U + x) * 4U;
            alpha_silhouette[index + 3U] = x < 2U ? 0.0F : 1.0F;
        }
    }
    const auto rgba_shape = analyze_rgb_f32(alpha_silhouette, 4U, 4U, 4U);
    expect_true(rgba_shape.valid && rgba_shape.features.edge_density == 0.0 &&
                    rgba_shape.features.alpha_transition_ratio > 0.0,
                "RGBA silhouette exposes alpha-only structural edges");
    expect_true(rgba_shape.route == ProcessingRoute::VectorReconstruction,
                "RGBA alpha-only silhouette routes to vector reconstruction");

    std::vector<float> hard_edge_logo(8U * 8U * 4U, 1.0F);
    for (std::uint32_t y = 0U; y < 8U; ++y) {
        for (std::uint32_t x = 0U; x < 8U; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * 8U + x) * 4U;
            const float value = x < 4U ? 0.0F : 1.0F;
            hard_edge_logo[index] = value;
            hard_edge_logo[index + 1U] = value;
            hard_edge_logo[index + 2U] = value;
            hard_edge_logo[index + 3U] = 1.0F;
        }
    }
    const auto hard_edge = analyze_rgb_f32(hard_edge_logo, 8U, 8U, 4U);
    expect_true(hard_edge.valid && hard_edge.features.edge_density > 0.0,
                "hard-edged logo exposes structural edge density");
    expect_true(hard_edge.features.texture_energy == 0.0,
                "hard structural edges are excluded from texture energy");
    expect_true(hard_edge.route == ProcessingRoute::VectorReconstruction,
                "hard-edged logo remains on vector reconstruction route");

    const auto bad_shape = analyze_rgb_f32(flat_rgb, 4U, 4U, 4U);
    expect_true(!bad_shape.valid, "mismatched buffer shape is rejected");

    flat_rgb[0] = 2.0F;
    const auto out_of_range = analyze_rgb_f32(flat_rgb, 4U, 4U, 3U);
    expect_true(!out_of_range.valid, "out-of-range normalized sample is rejected");

    return failures;
}
