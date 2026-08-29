#include "vektoryum/vector/vectorizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace vektoryum::vector {
namespace {

struct Edge {
    PointI from{};
    PointI to{};
};

[[nodiscard]] std::int64_t point_key(const PointI p) noexcept {
    const auto ux = static_cast<std::uint32_t>(p.x);
    const auto uy = static_cast<std::uint32_t>(p.y);
    return (static_cast<std::int64_t>(uy) << 32) | static_cast<std::int64_t>(ux);
}

[[nodiscard]] bool is_collinear(const PointI a, const PointI b, const PointI c) noexcept {
    const std::int64_t abx = static_cast<std::int64_t>(b.x) - a.x;
    const std::int64_t aby = static_cast<std::int64_t>(b.y) - a.y;
    const std::int64_t bcx = static_cast<std::int64_t>(c.x) - b.x;
    const std::int64_t bcy = static_cast<std::int64_t>(c.y) - b.y;
    return abx * bcy == aby * bcx;
}

[[nodiscard]] bool point_in_contour(const Contour& contour, const double x, const double y) noexcept {
    const std::size_t count = contour.points.size();
    if (count < 3U) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0U, j = count - 1U; i < count; j = i++) {
        const auto& pi = contour.points[i];
        const auto& pj = contour.points[j];
        const double yi = static_cast<double>(pi.y);
        const double yj = static_cast<double>(pj.y);
        const bool crosses = (yi > y) != (yj > y);
        if (!crosses) {
            continue;
        }
        const double xi = static_cast<double>(pi.x);
        const double xj = static_cast<double>(pj.x);
        const double hit_x = (xj - xi) * (y - yi) / (yj - yi) + xi;
        if (x < hit_x) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

Contour simplify_collinear(const Contour& contour) {
    if (contour.points.size() < 3U) {
        return contour;
    }

    Contour simplified{};
    simplified.points.reserve(contour.points.size());
    const std::size_t count = contour.points.size();
    for (std::size_t i = 0U; i < count; ++i) {
        const PointI prev = contour.points[(i + count - 1U) % count];
        const PointI curr = contour.points[i];
        const PointI next = contour.points[(i + 1U) % count];
        if (!is_collinear(prev, curr, next)) {
            simplified.points.push_back(curr);
        }
    }
    return simplified;
}

VectorizationResult trace_binary_mask(
    const std::span<const std::uint8_t> mask,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    VectorizationResult result{};
    if (width == 0U || height == 0U) {
        result.error = VectorizeError::ZeroDimension;
        return result;
    }
    if (width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() - 1) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() - 1)) {
        result.error = VectorizeError::CoordinateOverflow;
        return result;
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    if (pixel_count != mask.size()) {
        result.error = VectorizeError::MaskSizeMismatch;
        return result;
    }

    const auto is_on = [&](const std::uint32_t x, const std::uint32_t y) noexcept {
        return mask[static_cast<std::size_t>(y) * width + x] == 1U;
    };

    std::vector<Edge> edges{};
    edges.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(pixel_count * 2U, 1'000'000U)));

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::uint8_t value = mask[static_cast<std::size_t>(y) * width + x];
            if (value > 1U) {
                result.error = VectorizeError::InvalidMaskValue;
                return result;
            }
            if (value == 0U) {
                continue;
            }
            ++result.foreground_pixels;

            const auto xi = static_cast<std::int32_t>(x);
            const auto yi = static_cast<std::int32_t>(y);
            if (y == 0U || !is_on(x, y - 1U)) {
                edges.push_back({{xi, yi}, {xi + 1, yi}});
            }
            if (x + 1U == width || !is_on(x + 1U, y)) {
                edges.push_back({{xi + 1, yi}, {xi + 1, yi + 1}});
            }
            if (y + 1U == height || !is_on(x, y + 1U)) {
                edges.push_back({{xi + 1, yi + 1}, {xi, yi + 1}});
            }
            if (x == 0U || !is_on(x - 1U, y)) {
                edges.push_back({{xi, yi + 1}, {xi, yi}});
            }
        }
    }

    if (result.foreground_pixels == 0U) {
        result.error = VectorizeError::NoForeground;
        return result;
    }
    result.boundary_edges = edges.size();

    std::map<std::int64_t, std::vector<std::size_t>> outgoing{};
    for (std::size_t i = 0U; i < edges.size(); ++i) {
        outgoing[point_key(edges[i].from)].push_back(i);
    }
    for (const auto& [key, indices] : outgoing) {
        static_cast<void>(key);
        if (indices.size() != 1U) {
            result.error = VectorizeError::TopologyAmbiguity;
            result.contours.clear();
            return result;
        }
    }

    std::vector<bool> used(edges.size(), false);
    for (std::size_t start_edge = 0U; start_edge < edges.size(); ++start_edge) {
        if (used[start_edge]) {
            continue;
        }

        Contour contour{};
        std::size_t current = start_edge;
        const PointI start = edges[current].from;
        for (std::size_t guard = 0U; guard <= edges.size(); ++guard) {
            if (used[current]) {
                if (edges[current].from == start) {
                    break;
                }
                result.error = VectorizeError::TopologyAmbiguity;
                result.contours.clear();
                return result;
            }
            used[current] = true;
            contour.points.push_back(edges[current].from);
            const PointI next_point = edges[current].to;
            if (next_point == start) {
                break;
            }
            const auto found = outgoing.find(point_key(next_point));
            if (found == outgoing.end() || found->second.size() != 1U) {
                result.error = VectorizeError::TopologyAmbiguity;
                result.contours.clear();
                return result;
            }
            current = found->second.front();
            if (guard == edges.size()) {
                result.error = VectorizeError::TopologyAmbiguity;
                result.contours.clear();
                return result;
            }
        }

        contour = simplify_collinear(contour);
        if (contour.points.size() < 3U) {
            result.error = VectorizeError::TopologyAmbiguity;
            result.contours.clear();
            return result;
        }
        result.contours.push_back(std::move(contour));
    }

    return result;
}

std::vector<std::uint8_t> rasterize_even_odd(
    const std::span<const Contour> contours,
    const std::uint32_t width,
    const std::uint32_t height) {
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> output(pixel_count, 0U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            bool inside = false;
            const double px = static_cast<double>(x) + 0.5;
            const double py = static_cast<double>(y) + 0.5;
            for (const auto& contour : contours) {
                if (point_in_contour(contour, px, py)) {
                    inside = !inside;
                }
            }
            output[static_cast<std::size_t>(y) * width + x] = inside ? 1U : 0U;
        }
    }
    return output;
}

FidelityMetrics rasterize_back_fidelity(
    const std::span<const std::uint8_t> source_mask,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::span<const Contour> contours) {
    FidelityMetrics metrics{};
    const auto reconstructed = rasterize_even_odd(contours, width, height);
    if (source_mask.size() != reconstructed.size()) {
        return metrics;
    }

    for (std::size_t i = 0U; i < reconstructed.size(); ++i) {
        const bool source = source_mask[i] != 0U;
        const bool rebuilt = reconstructed[i] != 0U;
        metrics.source_foreground += source ? 1U : 0U;
        metrics.reconstructed_foreground += rebuilt ? 1U : 0U;
        metrics.intersection += source && rebuilt ? 1U : 0U;
        metrics.union_pixels += source || rebuilt ? 1U : 0U;
    }
    metrics.iou = metrics.union_pixels == 0U
        ? 1.0
        : static_cast<double>(metrics.intersection) / static_cast<double>(metrics.union_pixels);
    return metrics;
}

}  // namespace vektoryum::vector
