#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "vektoryum/certification/final_svg_rasterizer.hpp"
#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/vector/reconstruction.hpp"

namespace vektoryum::certification {

struct FinalOutputEvidence {
    bool valid{false};
    std::string output_sha256;
    CanonicalQualityMeasurement canonical_quality{};
    double color_mae{1.0};
    std::uint64_t reference_components{0U};
    std::uint64_t candidate_components{0U};
    std::uint64_t reference_holes{0U};
    std::uint64_t candidate_holes{0U};
    double boundary_p95_pixels{std::numeric_limits<double>::infinity()};
    double visible_residual_ratio{1.0};
};

namespace final_output_detail {

struct BinaryTopology {
    std::uint64_t components{0U};
    std::uint64_t holes{0U};
};

[[nodiscard]] inline BinaryTopology measure_topology(
    std::span<const std::uint8_t> mask,
    std::uint32_t width,
    std::uint32_t height) {
    BinaryTopology result{};
    const std::size_t count = static_cast<std::size_t>(width) * height;
    if (width == 0U || height == 0U || mask.size() != count) {
        return result;
    }

    const auto count_regions = [mask, width, height, count](bool foreground, bool exclude_border) {
        std::vector<std::uint8_t> visited(count, 0U);
        std::vector<std::size_t> queue;
        queue.reserve(count);
        std::uint64_t regions = 0U;
        for (std::size_t start = 0U; start < count; ++start) {
            const bool value = mask[start] != 0U;
            if (value != foreground || visited[start] != 0U) {
                continue;
            }
            bool touches_border = false;
            queue.clear();
            queue.push_back(start);
            visited[start] = 1U;
            for (std::size_t head = 0U; head < queue.size(); ++head) {
                const std::size_t index = queue[head];
                const std::uint32_t x = static_cast<std::uint32_t>(index % width);
                const std::uint32_t y = static_cast<std::uint32_t>(index / width);
                touches_border = touches_border || x == 0U || y == 0U || x + 1U == width || y + 1U == height;
                const auto visit = [&](std::uint32_t nx, std::uint32_t ny) {
                    const std::size_t next = static_cast<std::size_t>(ny) * width + nx;
                    if (visited[next] == 0U && ((mask[next] != 0U) == foreground)) {
                        visited[next] = 1U;
                        queue.push_back(next);
                    }
                };
                if (x > 0U) visit(x - 1U, y);
                if (x + 1U < width) visit(x + 1U, y);
                if (y > 0U) visit(x, y - 1U);
                if (y + 1U < height) visit(x, y + 1U);
            }
            if (!exclude_border || !touches_border) {
                ++regions;
            }
        }
        return regions;
    };

    result.components = count_regions(true, false);
    result.holes = count_regions(false, true);
    return result;
}

[[nodiscard]] inline std::vector<std::pair<std::uint32_t, std::uint32_t>> boundary_points(
    std::span<const std::uint8_t> mask,
    std::uint32_t width,
    std::uint32_t height) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> points;
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const bool value = mask[index] != 0U;
            bool boundary = x == 0U || y == 0U || x + 1U == width || y + 1U == height;
            if (x > 0U) boundary = boundary || ((mask[index - 1U] != 0U) != value);
            if (x + 1U < width) boundary = boundary || ((mask[index + 1U] != 0U) != value);
            if (y > 0U) boundary = boundary || ((mask[index - width] != 0U) != value);
            if (y + 1U < height) boundary = boundary || ((mask[index + width] != 0U) != value);
            if (boundary) {
                points.emplace_back(x, y);
            }
        }
    }
    return points;
}

[[nodiscard]] inline double directed_boundary_p95(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& from,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& to) {
    if (from.empty() || to.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    std::vector<double> distances;
    distances.reserve(from.size());
    for (const auto& [fx, fy] : from) {
        double best_squared = std::numeric_limits<double>::infinity();
        for (const auto& [tx, ty] : to) {
            const double dx = static_cast<double>(fx) - static_cast<double>(tx);
            const double dy = static_cast<double>(fy) - static_cast<double>(ty);
            best_squared = std::min(best_squared, dx * dx + dy * dy);
        }
        distances.push_back(std::sqrt(best_squared));
    }
    const std::size_t percentile_index = (distances.size() - 1U) * 95U / 100U;
    std::nth_element(distances.begin(), distances.begin() + static_cast<std::ptrdiff_t>(percentile_index), distances.end());
    return distances[percentile_index];
}

}  // namespace final_output_detail

// Measures quality exclusively from the independently rasterized final SVG bytes.
// The final artifact digest and every candidate metric below are therefore bound to
// the serialized output rather than to SvgScene/reconstruction state.
[[nodiscard]] inline FinalOutputEvidence measure_final_serialized_svg_evidence(
    std::span<const std::uint8_t> final_svg_bytes,
    std::span<const std::uint8_t> reference_rgba8,
    std::span<const std::uint8_t> reference_alpha,
    std::span<const std::uint8_t> reference_vector_mask,
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t max_pixels = 1'000'000U) {
    FinalOutputEvidence result{};
    const std::uint64_t pixel_count64 = static_cast<std::uint64_t>(width) * height;
    if (width == 0U || height == 0U || pixel_count64 == 0U || pixel_count64 > max_pixels ||
        pixel_count64 > std::numeric_limits<std::size_t>::max()) {
        return result;
    }
    const std::size_t pixel_count = static_cast<std::size_t>(pixel_count64);
    if (reference_rgba8.size() != pixel_count * 4U || reference_alpha.size() != pixel_count ||
        reference_vector_mask.size() != pixel_count) {
        return result;
    }

    const FinalSvgRaster candidate = rasterize_final_serialized_svg(final_svg_bytes, max_pixels);
    if (!candidate.valid || candidate.width != width || candidate.height != height ||
        candidate.rgba8.size() != reference_rgba8.size()) {
        return result;
    }

    std::vector<std::uint8_t> candidate_alpha(pixel_count, 0U);
    std::vector<std::uint8_t> candidate_mask(pixel_count, 0U);
    std::uint64_t color_error = 0U;
    std::uint64_t color_samples = 0U;
    std::uint64_t visible_union = 0U;
    std::uint64_t visible_residual = 0U;
    for (std::size_t pixel = 0U; pixel < pixel_count; ++pixel) {
        const std::size_t base = pixel * 4U;
        const std::uint8_t alpha = candidate.rgba8[base + 3U];
        candidate_alpha[pixel] = alpha;
        candidate_mask[pixel] = static_cast<std::uint8_t>(vektoryum::vector::coverage_is_foreground(alpha));

        const bool reference_visible = vektoryum::vector::coverage_is_foreground(reference_alpha[pixel]);
        const bool candidate_visible = vektoryum::vector::coverage_is_foreground(alpha);
        if (reference_visible || candidate_visible) {
            ++visible_union;
            bool residual = reference_visible != candidate_visible;
            if (reference_visible && candidate_visible) {
                for (std::size_t channel = 0U; channel < 3U; ++channel) {
                    const auto lhs = static_cast<int>(reference_rgba8[base + channel]);
                    const auto rhs = static_cast<int>(candidate.rgba8[base + channel]);
                    const auto delta = static_cast<std::uint64_t>(std::abs(lhs - rhs));
                    color_error += delta;
                    ++color_samples;
                    residual = residual || delta != 0U;
                }
            }
            if (residual) {
                ++visible_residual;
            }
        }
    }

    CanonicalQualityFixture fixture;
    fixture.reference_alpha.assign(reference_alpha.begin(), reference_alpha.end());
    fixture.candidate_alpha = candidate_alpha;
    fixture.reference_vector_mask.assign(reference_vector_mask.begin(), reference_vector_mask.end());
    fixture.candidate_vector_mask = candidate_mask;
    result.canonical_quality = measure_canonical_quality_metrics(fixture, max_pixels);
    if (!result.canonical_quality.ok()) {
        return result;
    }

    result.output_sha256 = vektoryum::ml::sha256_hex(final_svg_bytes);
    result.color_mae = color_samples == 0U
        ? 0.0
        : static_cast<double>(color_error) / (255.0 * static_cast<double>(color_samples));
    result.visible_residual_ratio = visible_union == 0U
        ? 0.0
        : static_cast<double>(visible_residual) / static_cast<double>(visible_union);

    const auto reference_topology = final_output_detail::measure_topology(reference_vector_mask, width, height);
    const auto candidate_topology = final_output_detail::measure_topology(candidate_mask, width, height);
    result.reference_components = reference_topology.components;
    result.candidate_components = candidate_topology.components;
    result.reference_holes = reference_topology.holes;
    result.candidate_holes = candidate_topology.holes;

    const auto reference_boundary = final_output_detail::boundary_points(reference_vector_mask, width, height);
    const auto candidate_boundary = final_output_detail::boundary_points(candidate_mask, width, height);
    result.boundary_p95_pixels = std::max(
        final_output_detail::directed_boundary_p95(reference_boundary, candidate_boundary),
        final_output_detail::directed_boundary_p95(candidate_boundary, reference_boundary));
    result.valid = std::isfinite(result.boundary_p95_pixels);
    return result;
}

}  // namespace vektoryum::certification
