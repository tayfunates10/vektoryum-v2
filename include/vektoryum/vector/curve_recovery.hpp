#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "vektoryum/vector/svg_path.hpp"

namespace vektoryum::vector {

struct CurveRecoveryOptions {
    double simplify_tolerance{0.75};
    double corner_radius{0.5};
    SvgCertificationOptions certification{};
};

struct CurveRecoveryResult {
    SvgFitError error{SvgFitError::None};
    SvgScene scene{};
    SvgCertificationReport certification{};
    std::size_t source_nodes{};
    std::size_t recovered_nodes{};
    bool used_curves{false};

    [[nodiscard]] bool ok() const noexcept {
        return error == SvgFitError::None && certification.passed();
    }

    [[nodiscard]] bool reduced_nodes() const noexcept {
        return recovered_nodes < source_nodes;
    }
};

namespace detail {

[[nodiscard]] inline double point_segment_distance(
    const IntPoint& point,
    const IntPoint& a,
    const IntPoint& b) noexcept {
    const double px = static_cast<double>(point.x);
    const double py = static_cast<double>(point.y);
    const double ax = static_cast<double>(a.x);
    const double ay = static_cast<double>(a.y);
    const double bx = static_cast<double>(b.x);
    const double by = static_cast<double>(b.y);
    const double dx = bx - ax;
    const double dy = by - ay;
    const double length_sq = (dx * dx) + (dy * dy);
    if (length_sq == 0.0) {
        return std::hypot(px - ax, py - ay);
    }

    double t = (((px - ax) * dx) + ((py - ay) * dy)) / length_sq;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }
    const double nearest_x = ax + (t * dx);
    const double nearest_y = ay + (t * dy);
    return std::hypot(px - nearest_x, py - nearest_y);
}

[[nodiscard]] inline std::vector<IntPoint> simplify_closed_curve_path(
    const std::vector<IntPoint>& input,
    double tolerance) {
    std::vector<IntPoint> points = input;
    if (points.size() <= 3U || tolerance == 0.0) {
        return points;
    }

    bool changed = true;
    while (changed && points.size() > 3U) {
        changed = false;
        for (std::size_t i = 0U; i < points.size(); ++i) {
            const std::size_t previous = (i + points.size() - 1U) % points.size();
            const std::size_t next = (i + 1U) % points.size();
            if (point_segment_distance(points[i], points[previous], points[next]) <= tolerance) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
    return points;
}

[[nodiscard]] inline std::size_t curve_node_count(const VectorScene& scene) noexcept {
    std::size_t count = 0U;
    for (const Path& path : scene.paths) {
        count += path.points.size();
    }
    return count;
}

[[nodiscard]] inline bool contains_cubic_commands(const SvgScene& scene) noexcept {
    for (const SvgPath& path : scene.paths) {
        for (const SvgCommand& command : path.commands) {
            if (command.type == SvgCommandType::CubicTo) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace detail

// Recover a lower-node cubic representation only when it independently passes
// the existing rasterize-back fidelity gate. Failed candidates fall back to the
// exact polygon; acceptance thresholds are never relaxed to obtain reduction.
[[nodiscard]] inline CurveRecoveryResult recover_curves_certified(
    const VectorScene& scene,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    CurveRecoveryOptions options = {}) {
    CurveRecoveryResult result{};
    result.source_nodes = detail::curve_node_count(scene);

    if (!std::isfinite(options.simplify_tolerance) || options.simplify_tolerance < 0.0 ||
        !std::isfinite(options.corner_radius) || options.corner_radius < 0.0) {
        result.error = SvgFitError::InvalidRadius;
        return result;
    }

    VectorScene simplified = scene;
    for (Path& path : simplified.paths) {
        path.points = detail::simplify_closed_curve_path(path.points, options.simplify_tolerance);
    }

    const std::size_t simplified_nodes = detail::curve_node_count(simplified);
    const auto curved = fit_svg_paths(simplified, {.corner_radius = options.corner_radius});
    if (curved.ok()) {
        const auto quality = certify_svg_scene(
            curved.scene, reference_mask, width, height, options.certification);
        if (quality.passed()) {
            result.scene = curved.scene;
            result.certification = quality;
            result.recovered_nodes = simplified_nodes;
            result.used_curves = detail::contains_cubic_commands(curved.scene);
            return result;
        }
    }

    const auto exact = fit_svg_paths(scene, {.corner_radius = 0.0});
    if (!exact.ok()) {
        result.error = exact.error;
        return result;
    }

    result.scene = exact.scene;
    result.recovered_nodes = result.source_nodes;
    result.used_curves = false;
    result.certification = certify_svg_scene(
        exact.scene, reference_mask, width, height, options.certification);
    if (!result.certification.passed()) {
        result.error = SvgFitError::InvalidScene;
    }
    return result;
}

}  // namespace vektoryum::vector
