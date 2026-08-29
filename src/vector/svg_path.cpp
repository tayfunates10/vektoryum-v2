#include "vektoryum/vector/svg_path.hpp"

#include <algorithm>
#include <cmath>
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
