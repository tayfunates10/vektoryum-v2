#include "vektoryum/vector/reconstruction.hpp"

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
    IntPoint a{};
    IntPoint b{};
    bool used{false};
};

[[nodiscard]] bool less_point(const IntPoint& lhs, const IntPoint& rhs) noexcept {
    return lhs.y < rhs.y || (lhs.y == rhs.y && lhs.x < rhs.x);
}

[[nodiscard]] double signed_area(const std::vector<IntPoint>& points) noexcept {
    if (points.size() < 3U) {
        return 0.0;
    }
    double area = 0.0;
    for (std::size_t i = 0U; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1U) % points.size()];
        area += static_cast<double>(a.x) * static_cast<double>(b.y) -
                static_cast<double>(b.x) * static_cast<double>(a.y);
    }
    return area * 0.5;
}

void remove_collinear(std::vector<IntPoint>& points) {
    if (points.size() < 3U) {
        return;
    }
    std::vector<IntPoint> out;
    out.reserve(points.size());
    for (std::size_t i = 0U; i < points.size(); ++i) {
        const auto& prev = points[(i + points.size() - 1U) % points.size()];
        const auto& cur = points[i];
        const auto& next = points[(i + 1U) % points.size()];
        const std::int64_t dx1 = static_cast<std::int64_t>(cur.x) - prev.x;
        const std::int64_t dy1 = static_cast<std::int64_t>(cur.y) - prev.y;
        const std::int64_t dx2 = static_cast<std::int64_t>(next.x) - cur.x;
        const std::int64_t dy2 = static_cast<std::int64_t>(next.y) - cur.y;
        if (dx1 * dy2 != dy1 * dx2) {
            out.push_back(cur);
        }
    }
    points = std::move(out);
}

[[nodiscard]] bool point_in_path(double x, double y, const Path& path) noexcept {
    bool inside = false;
    const auto n = path.points.size();
    if (n < 3U) {
        return false;
    }
    for (std::size_t i = 0U, j = n - 1U; i < n; j = i++) {
        const auto& pi = path.points[i];
        const auto& pj = path.points[j];
        const double yi = static_cast<double>(pi.y);
        const double yj = static_cast<double>(pj.y);
        const bool crosses = ((yi > y) != (yj > y));
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

ReconstructionResult reconstruct_binary_mask(
    std::span<const std::uint8_t> coverage,
    std::uint32_t width,
    std::uint32_t height,
    ReconstructionOptions options) noexcept {
    ReconstructionResult result{};
    if (width == 0U || height == 0U) {
        result.error = ReconstructionError::ZeroDimension;
        return result;
    }
    if (width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() - 1) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() - 1)) {
        result.error = ReconstructionError::CoordinateOverflow;
        return result;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
    if (pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error = ReconstructionError::SizeOverflow;
        return result;
    }
    if (coverage.size() != static_cast<std::size_t>(pixels)) {
        result.error = ReconstructionError::SizeMismatch;
        return result;
    }

    result.scene.width = width;
    result.scene.height = height;
    std::vector<Edge> edges;
    const std::size_t reserve_count = std::min(static_cast<std::size_t>(pixels), options.max_nodes);
    edges.reserve(reserve_count);
    const auto fg = [&](std::int64_t x, std::int64_t y) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(width) || y >= static_cast<std::int64_t>(height)) {
            return false;
        }
        const auto idx = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
        return coverage[idx] >= options.threshold;
    };
    const auto add_edge = [&](IntPoint a, IntPoint b) noexcept {
        if (edges.size() >= options.max_nodes) {
            return false;
        }
        edges.push_back({a, b, false});
        return true;
    };

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            if (!fg(x, y)) {
                continue;
            }
            const auto ix = static_cast<std::int32_t>(x);
            const auto iy = static_cast<std::int32_t>(y);
            if (!fg(static_cast<std::int64_t>(x), static_cast<std::int64_t>(y) - 1) &&
                !add_edge({ix, iy}, {ix + 1, iy})) {
                result.error = ReconstructionError::NodeBudgetExceeded;
                return result;
            }
            if (!fg(static_cast<std::int64_t>(x) + 1, y) &&
                !add_edge({ix + 1, iy}, {ix + 1, iy + 1})) {
                result.error = ReconstructionError::NodeBudgetExceeded;
                return result;
            }
            if (!fg(x, static_cast<std::int64_t>(y) + 1) &&
                !add_edge({ix + 1, iy + 1}, {ix, iy + 1})) {
                result.error = ReconstructionError::NodeBudgetExceeded;
                return result;
            }
            if (!fg(static_cast<std::int64_t>(x) - 1, y) &&
                !add_edge({ix, iy + 1}, {ix, iy})) {
                result.error = ReconstructionError::NodeBudgetExceeded;
                return result;
            }
        }
    }

    if (edges.empty()) {
        result.error = ReconstructionError::NoForeground;
        return result;
    }

    std::map<std::pair<std::int32_t, std::int32_t>, std::vector<std::size_t>> outgoing;
    for (std::size_t i = 0U; i < edges.size(); ++i) {
        outgoing[{edges[i].a.x, edges[i].a.y}].push_back(i);
    }
    for (auto& [key, indices] : outgoing) {
        (void)key;
        std::sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
            return less_point(edges[lhs].b, edges[rhs].b);
        });
        if (indices.size() != 1U) {
            result.error = ReconstructionError::TopologyAmbiguity;
            return result;
        }
    }

    for (std::size_t seed = 0U; seed < edges.size(); ++seed) {
        if (edges[seed].used) {
            continue;
        }
        Path path{};
        const IntPoint start = edges[seed].a;
        std::size_t current = seed;
        while (true) {
            if (edges[current].used) {
                result.error = ReconstructionError::BrokenContour;
                return result;
            }
            edges[current].used = true;
            path.points.push_back(edges[current].a);
            const IntPoint next = edges[current].b;
            if (next == start) {
                break;
            }
            const auto it = outgoing.find({next.x, next.y});
            if (it == outgoing.end() || it->second.size() != 1U) {
                result.error = ReconstructionError::BrokenContour;
                return result;
            }
            current = it->second.front();
            if (edges[current].used) {
                result.error = ReconstructionError::BrokenContour;
                return result;
            }
            if (path.points.size() > options.max_nodes) {
                result.error = ReconstructionError::NodeBudgetExceeded;
                return result;
            }
        }
        remove_collinear(path.points);
        if (path.points.size() >= 3U) {
            path.hole = signed_area(path.points) < 0.0;
            result.scene.paths.push_back(std::move(path));
        }
    }

    std::sort(result.scene.paths.begin(), result.scene.paths.end(), [](const Path& lhs, const Path& rhs) {
        const IntPoint la = *std::min_element(lhs.points.begin(), lhs.points.end(), less_point);
        const IntPoint ra = *std::min_element(rhs.points.begin(), rhs.points.end(), less_point);
        return less_point(la, ra);
    });
    return result;
}

std::vector<std::uint8_t> rasterize_even_odd(const VectorScene& scene, std::uint32_t width, std::uint32_t height) {
    const std::size_t count = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> out(count, 0U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            bool inside = false;
            const double px = static_cast<double>(x) + 0.5;
            const double py = static_cast<double>(y) + 0.5;
            for (const auto& path : scene.paths) {
                if (point_in_path(px, py, path)) {
                    inside = !inside;
                }
            }
            if (inside) {
                out[static_cast<std::size_t>(y) * width + x] = 255U;
            }
        }
    }
    return out;
}

double binary_iou(std::span<const std::uint8_t> lhs, std::span<const std::uint8_t> rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return 0.0;
    }
    std::uint64_t intersection = 0U;
    std::uint64_t union_count = 0U;
    for (std::size_t i = 0U; i < lhs.size(); ++i) {
        const bool a = lhs[i] != 0U;
        const bool b = rhs[i] != 0U;
        intersection += (a && b) ? 1U : 0U;
        union_count += (a || b) ? 1U : 0U;
    }
    if (union_count == 0U) {
        return 1.0;
    }
    return static_cast<double>(intersection) / static_cast<double>(union_count);
}

}  // namespace vektoryum::vector
