#include "vektoryum/vector/svg_path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <locale>
#include <sstream>

namespace vektoryum::vector {
namespace {

constexpr double kKappa = 0.5522847498307936;

[[nodiscard]] double distance(const DoublePoint& a, const DoublePoint& b) noexcept {
    return std::hypot(b.x - a.x, b.y - a.y);
}

[[nodiscard]] DoublePoint to_double(IntPoint p) noexcept {
    return {static_cast<double>(p.x), static_cast<double>(p.y)};
}

[[nodiscard]] DoublePoint interpolate(const DoublePoint& a, const DoublePoint& b, double t) noexcept {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

[[nodiscard]] DoublePoint cubic_point(
    const DoublePoint& p0,
    const DoublePoint& p1,
    const DoublePoint& p2,
    const DoublePoint& p3,
    double t) noexcept {
    const double u = 1.0 - t;
    const double uu = u * u;
    const double tt = t * t;
    const double uuu = uu * u;
    const double ttt = tt * t;
    return {
        (uuu * p0.x) + (3.0 * uu * t * p1.x) + (3.0 * u * tt * p2.x) + (ttt * p3.x),
        (uuu * p0.y) + (3.0 * uu * t * p1.y) + (3.0 * u * tt * p2.y) + (ttt * p3.y),
    };
}

[[nodiscard]] bool finite_point(const DoublePoint& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

[[nodiscard]] bool valid_source_path(const Path& path) noexcept {
    if (!path.closed || path.points.size() < 3U) {
        return false;
    }
    for (std::size_t i = 0U; i < path.points.size(); ++i) {
        if (path.points[i] == path.points[(i + 1U) % path.points.size()]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] SvgCommand move_to(DoublePoint p) noexcept {
    SvgCommand command{};
    command.type = SvgCommandType::MoveTo;
    command.end = p;
    return command;
}

[[nodiscard]] SvgCommand line_to(DoublePoint p) noexcept {
    SvgCommand command{};
    command.type = SvgCommandType::LineTo;
    command.end = p;
    return command;
}

[[nodiscard]] SvgCommand cubic_to(DoublePoint c1, DoublePoint c2, DoublePoint p) noexcept {
    SvgCommand command{};
    command.type = SvgCommandType::CubicTo;
    command.control1 = c1;
    command.control2 = c2;
    command.end = p;
    return command;
}

[[nodiscard]] SvgCommand close_path() noexcept {
    SvgCommand command{};
    command.type = SvgCommandType::Close;
    return command;
}

[[nodiscard]] bool flatten_svg_path(
    const SvgPath& path,
    std::uint32_t subdivisions,
    std::vector<DoublePoint>& flattened) {
    if (subdivisions == 0U || path.commands.size() < 3U ||
        path.commands.front().type != SvgCommandType::MoveTo ||
        path.commands.back().type != SvgCommandType::Close) {
        return false;
    }

    flattened.clear();
    DoublePoint current = path.commands.front().end;
    if (!finite_point(current)) {
        return false;
    }
    flattened.push_back(current);

    for (std::size_t i = 1U; i + 1U < path.commands.size(); ++i) {
        const SvgCommand& command = path.commands[i];
        if (!finite_point(command.end)) {
            return false;
        }
        if (command.type == SvgCommandType::LineTo) {
            flattened.push_back(command.end);
            current = command.end;
            continue;
        }
        if (command.type == SvgCommandType::CubicTo) {
            if (!finite_point(command.control1) || !finite_point(command.control2)) {
                return false;
            }
            for (std::uint32_t step = 1U; step <= subdivisions; ++step) {
                const double t = static_cast<double>(step) / static_cast<double>(subdivisions);
                flattened.push_back(cubic_point(current, command.control1, command.control2, command.end, t));
            }
            current = command.end;
            continue;
        }
        return false;
    }
    return flattened.size() >= 3U;
}

[[nodiscard]] bool point_in_polygon(
    const std::vector<DoublePoint>& polygon,
    double x,
    double y) noexcept {
    bool inside = false;
    for (std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
        const DoublePoint& a = polygon[i];
        const DoublePoint& b = polygon[j];
        const bool crosses = ((a.y > y) != (b.y > y));
        if (!crosses) {
            continue;
        }
        const double intersection_x = ((b.x - a.x) * (y - a.y) / (b.y - a.y)) + a.x;
        if (x < intersection_x) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

SvgFitResult fit_svg_paths(const VectorScene& scene, SvgFitOptions options) {
    SvgFitResult result{};
    if (scene.width == 0U || scene.height == 0U || scene.paths.empty()) {
        result.error = SvgFitError::InvalidScene;
        return result;
    }
    if (!std::isfinite(options.corner_radius) || options.corner_radius < 0.0) {
        result.error = SvgFitError::InvalidRadius;
        return result;
    }

    result.scene.width = scene.width;
    result.scene.height = scene.height;
    result.scene.paths.reserve(scene.paths.size());

    for (const Path& source : scene.paths) {
        if (!valid_source_path(source)) {
            result.error = SvgFitError::DegeneratePath;
            result.scene = {};
            return result;
        }

        SvgPath output{};
        output.hole = source.hole;
        const std::size_t count = source.points.size();

        if (options.corner_radius == 0.0) {
            output.commands.reserve(count + 1U);
            output.commands.push_back(move_to(to_double(source.points[0])));
            for (std::size_t i = 1U; i < count; ++i) {
                output.commands.push_back(line_to(to_double(source.points[i])));
            }
            output.commands.push_back(close_path());
            result.scene.paths.push_back(std::move(output));
            continue;
        }

        struct Corner {
            DoublePoint entry{};
            DoublePoint exit{};
            DoublePoint control1{};
            DoublePoint control2{};
        };
        std::vector<Corner> corners(count);

        for (std::size_t i = 0U; i < count; ++i) {
            const DoublePoint previous = to_double(source.points[(i + count - 1U) % count]);
            const DoublePoint current = to_double(source.points[i]);
            const DoublePoint next = to_double(source.points[(i + 1U) % count]);
            const double incoming_length = distance(previous, current);
            const double outgoing_length = distance(current, next);
            if (incoming_length == 0.0 || outgoing_length == 0.0) {
                result.error = SvgFitError::DegeneratePath;
                result.scene = {};
                return result;
            }

            const double trim = std::min({options.corner_radius,
                                          incoming_length * 0.5,
                                          outgoing_length * 0.5});
            const double incoming_fraction = trim / incoming_length;
            const double outgoing_fraction = trim / outgoing_length;
            const DoublePoint entry = interpolate(current, previous, incoming_fraction);
            const DoublePoint exit = interpolate(current, next, outgoing_fraction);
            const DoublePoint c1 = interpolate(entry, current, kKappa);
            const DoublePoint c2 = interpolate(exit, current, kKappa);
            corners[i] = {entry, exit, c1, c2};
        }

        output.commands.reserve((count * 2U) + 2U);
        output.commands.push_back(move_to(corners[0].exit));
        for (std::size_t step = 1U; step <= count; ++step) {
            const std::size_t i = step % count;
            output.commands.push_back(line_to(corners[i].entry));
            output.commands.push_back(cubic_to(corners[i].control1, corners[i].control2, corners[i].exit));
        }
        output.commands.push_back(close_path());
        result.scene.paths.push_back(std::move(output));
    }

    return result;
}

SvgCertificationReport certify_svg_scene(
    const SvgScene& scene,
    std::span<const std::uint8_t> reference_mask,
    std::uint32_t width,
    std::uint32_t height,
    SvgCertificationOptions options) {
    SvgCertificationReport report{};
    if (scene.width != width || scene.height != height || width == 0U || height == 0U ||
        scene.paths.empty() || options.cubic_subdivisions == 0U ||
        !std::isfinite(options.min_iou) || !std::isfinite(options.max_disagreement_ratio) ||
        options.min_iou < 0.0 || options.min_iou > 1.0 ||
        options.max_disagreement_ratio < 0.0 || options.max_disagreement_ratio > 1.0) {
        report.error = SvgCertificationError::InvalidScene;
        return report;
    }

    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    if (pixel_count > options.max_certification_pixels) {
        report.error = SvgCertificationError::CertificationBudgetExceeded;
        return report;
    }
    if (reference_mask.size() != static_cast<std::size_t>(pixel_count)) {
        report.error = SvgCertificationError::InvalidReference;
        return report;
    }
    for (std::uint8_t value : reference_mask) {
        if (value > 1U) {
            report.error = SvgCertificationError::InvalidReference;
            return report;
        }
    }

    std::vector<std::vector<DoublePoint>> polygons;
    polygons.reserve(scene.paths.size());
    for (const SvgPath& path : scene.paths) {
        std::vector<DoublePoint> polygon;
        if (!flatten_svg_path(path, options.cubic_subdivisions, polygon)) {
            report.error = SvgCertificationError::InvalidScene;
            return report;
        }
        polygons.push_back(std::move(polygon));
    }

    std::uint64_t intersection_count = 0U;
    std::uint64_t union_count = 0U;
    std::uint64_t disagreement_count = 0U;
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const double px = static_cast<double>(x) + 0.5;
            const double py = static_cast<double>(y) + 0.5;
            bool rendered = false;
            for (const auto& polygon : polygons) {
                if (point_in_polygon(polygon, px, py)) {
                    rendered = !rendered;
                }
            }
            const bool reference = reference_mask[static_cast<std::size_t>(y) * width + x] != 0U;
            intersection_count += rendered && reference ? 1U : 0U;
            union_count += rendered || reference ? 1U : 0U;
            disagreement_count += rendered != reference ? 1U : 0U;
        }
    }

    report.compared_pixels = pixel_count;
    report.raster_iou = union_count == 0U
        ? 1.0
        : static_cast<double>(intersection_count) / static_cast<double>(union_count);
    report.disagreement_ratio = pixel_count == 0U
        ? 0.0
        : static_cast<double>(disagreement_count) / static_cast<double>(pixel_count);
    if (report.raster_iou < options.min_iou ||
        report.disagreement_ratio > options.max_disagreement_ratio) {
        report.error = SvgCertificationError::FidelityRejected;
    }
    return report;
}

std::string serialize_svg_path_data(const SvgPath& path) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6);
    bool first = true;
    for (const SvgCommand& command : path.commands) {
        if (!first) {
            stream << ' ';
        }
        first = false;
        switch (command.type) {
        case SvgCommandType::MoveTo:
            stream << "M " << command.end.x << ' ' << command.end.y;
            break;
        case SvgCommandType::LineTo:
            stream << "L " << command.end.x << ' ' << command.end.y;
            break;
        case SvgCommandType::CubicTo:
            stream << "C " << command.control1.x << ' ' << command.control1.y << ' '
                   << command.control2.x << ' ' << command.control2.y << ' '
                   << command.end.x << ' ' << command.end.y;
            break;
        case SvgCommandType::Close:
            stream << 'Z';
            break;
        }
    }
    return stream.str();
}

}  // namespace vektoryum::vector
