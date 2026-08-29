#include "vektoryum/vector/path_quality.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace vektoryum::vector {
namespace {

[[nodiscard]] std::int64_t sign64(std::int64_t value) noexcept {
    return (value > 0) - (value < 0);
}

[[nodiscard]] std::int64_t orientation(const IntPoint& a, const IntPoint& b, const IntPoint& c) noexcept {
    const std::int64_t abx = static_cast<std::int64_t>(b.x) - a.x;
    const std::int64_t aby = static_cast<std::int64_t>(b.y) - a.y;
    const std::int64_t acx = static_cast<std::int64_t>(c.x) - a.x;
    const std::int64_t acy = static_cast<std::int64_t>(c.y) - a.y;

    constexpr std::int64_t safe = 2'000'000'000LL;
    if (std::abs(abx) > safe || std::abs(aby) > safe || std::abs(acx) > safe || std::abs(acy) > safe) {
        const long double cross = static_cast<long double>(abx) * static_cast<long double>(acy) -
                                  static_cast<long double>(aby) * static_cast<long double>(acx);
        return (cross > 0.0L) - (cross < 0.0L);
    }
    return sign64(abx * acy - aby * acx);
}

[[nodiscard]] bool on_segment(const IntPoint& a, const IntPoint& b, const IntPoint& p) noexcept {
    return p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
           p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}

[[nodiscard]] bool segments_intersect(
    const IntPoint& a,
    const IntPoint& b,
    const IntPoint& c,
    const IntPoint& d) noexcept {
    const auto o1 = orientation(a, b, c);
    const auto o2 = orientation(a, b, d);
    const auto o3 = orientation(c, d, a);
    const auto o4 = orientation(c, d, b);
    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && on_segment(a, b, c)) {
        return true;
    }
    if (o2 == 0 && on_segment(a, b, d)) {
        return true;
    }
    if (o3 == 0 && on_segment(c, d, a)) {
        return true;
    }
    return o4 == 0 && on_segment(c, d, b);
}

[[nodiscard]] bool adjacent_edges(std::size_t i, std::size_t j, std::size_t n) noexcept {
    if (i == j) {
        return true;
    }
    if ((i + 1U) % n == j || (j + 1U) % n == i) {
        return true;
    }
    return false;
}

[[nodiscard]] std::size_t total_nodes(const VectorScene& scene) noexcept {
    std::size_t total = 0U;
    for (const auto& path : scene.paths) {
        if (path.points.size() > std::numeric_limits<std::size_t>::max() - total) {
            return std::numeric_limits<std::size_t>::max();
        }
        total += path.points.size();
    }
    return total;
}

[[nodiscard]] bool valid_scene_shape(const VectorScene& scene) noexcept {
    if (scene.width == 0U || scene.height == 0U || scene.paths.empty()) {
        return false;
    }
    for (const auto& path : scene.paths) {
        if (!path.closed || path.points.size() < 3U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] double disagreement_ratio(
    std::span<const std::uint8_t> lhs,
    std::span<const std::uint8_t> rhs) noexcept {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 1.0;
    }
    std::size_t disagreements = 0U;
    for (std::size_t i = 0U; i < lhs.size(); ++i) {
        disagreements += ((lhs[i] != 0U) != (rhs[i] != 0U)) ? 1U : 0U;
    }
    return static_cast<double>(disagreements) / static_cast<double>(lhs.size());
}

}  // namespace

bool path_has_self_intersection(const Path& path) noexcept {
    const std::size_t n = path.points.size();
    if (!path.closed || n < 3U) {
        return true;
    }
    for (std::size_t i = 0U; i < n; ++i) {
        const auto& a = path.points[i];
        const auto& b = path.points[(i + 1U) % n];
        if (a == b) {
            return true;
        }
        for (std::size_t j = i + 1U; j < n; ++j) {
            if (adjacent_edges(i, j, n)) {
                continue;
            }
            const auto& c = path.points[j];
            const auto& d = path.points[(j + 1U) % n];
            if (segments_intersect(a, b, c, d)) {
                return true;
            }
        }
    }
    return false;
}

PathQualityReport certify_scene(
    const VectorScene& scene,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    PathQualityOptions options) {
    PathQualityReport report{};
    if (!valid_scene_shape(scene) || scene.width != width || scene.height != height) {
        report.error = PathQualityError::InvalidScene;
        return report;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
    if (pixels > options.max_certification_pixels ||
        pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        reference_mask.size() != static_cast<std::size_t>(pixels)) {
        report.error = PathQualityError::CertificationBudgetExceeded;
        return report;
    }

    report.total_nodes = total_nodes(scene);
    if (report.total_nodes > options.max_nodes) {
        report.error = PathQualityError::InvalidScene;
        return report;
    }
    for (const auto& path : scene.paths) {
        report.self_intersections += path_has_self_intersection(path) ? 1U : 0U;
    }
    if (report.self_intersections != 0U) {
        report.error = PathQualityError::SelfIntersection;
        return report;
    }

    const auto raster = rasterize_even_odd(scene, width, height);
    report.raster_iou = binary_iou(reference_mask, raster);
    report.disagreement_ratio = disagreement_ratio(reference_mask, raster);
    if (report.raster_iou < options.min_iou ||
        report.disagreement_ratio > options.max_disagreement_ratio) {
        report.error = PathQualityError::FidelityRejected;
    }
    return report;
}

SimplificationResult simplify_scene_fidelity_gated(
    const VectorScene& input,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    PathQualityOptions options) {
    SimplificationResult result{};
    result.scene = input;
    const auto initial = certify_scene(input, reference_mask, width, height, options);
    if (!initial.passed()) {
        result.error = initial.error;
        result.quality = initial;
        return result;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t path_index = 0U; path_index < result.scene.paths.size(); ++path_index) {
            auto& path = result.scene.paths[path_index];
            if (path.points.size() <= 3U) {
                continue;
            }
            for (std::size_t point_index = 0U; point_index < path.points.size(); ++point_index) {
                VectorScene candidate = result.scene;
                auto& candidate_path = candidate.paths[path_index];
                candidate_path.points.erase(candidate_path.points.begin() + static_cast<std::ptrdiff_t>(point_index));
                if (path_has_self_intersection(candidate_path)) {
                    continue;
                }
                const auto quality = certify_scene(candidate, reference_mask, width, height, options);
                if (!quality.passed()) {
                    continue;
                }
                result.scene = std::move(candidate);
                ++result.removed_nodes;
                changed = true;
                break;
            }
            if (changed) {
                break;
            }
        }
    }

    result.quality = certify_scene(result.scene, reference_mask, width, height, options);
    result.error = result.quality.error;
    return result;
}

}  // namespace vektoryum::vector
