#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vektoryum::certification {

struct FinalSvgRaster {
    bool valid{false};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<std::uint8_t> rgba8;
};

namespace final_svg_detail {

struct Point {
    double x{0.0};
    double y{0.0};
};

struct Subpath {
    std::vector<Point> points;
};

struct PaintPath {
    std::array<std::uint8_t, 3U> rgb{0U, 0U, 0U};
    std::vector<Subpath> subpaths;
};

[[nodiscard]] inline bool ascii_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline void skip_space(std::string_view text, std::size_t& pos) noexcept {
    while (pos < text.size() && ascii_space(text[pos])) {
        ++pos;
    }
}

[[nodiscard]] inline bool parse_unsigned(std::string_view text, std::uint32_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<std::uint64_t>(ch - '0');
        if (parsed > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }
    value = static_cast<std::uint32_t>(parsed);
    return value != 0U;
}

[[nodiscard]] inline int hex_nibble(char ch) noexcept {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

[[nodiscard]] inline bool parse_rgb(std::string_view text, std::array<std::uint8_t, 3U>& rgb) noexcept {
    if (text.size() != 7U || text[0] != '#') {
        return false;
    }
    for (std::size_t channel = 0U; channel < 3U; ++channel) {
        const int hi = hex_nibble(text[1U + channel * 2U]);
        const int lo = hex_nibble(text[2U + channel * 2U]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        rgb[channel] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

[[nodiscard]] inline bool parse_number(std::string_view text, std::size_t& pos, double& value) noexcept {
    skip_space(text, pos);
    if (pos >= text.size()) {
        return false;
    }
    const std::size_t start = pos;
    if (text[pos] == '-' || text[pos] == '+') {
        ++pos;
    }
    bool have_digit = false;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
        have_digit = true;
        ++pos;
    }
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            have_digit = true;
            ++pos;
        }
    }
    if (!have_digit) {
        return false;
    }
    const std::string token{text.substr(start, pos - start)};
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return end != token.c_str() && *end == '\0' && std::isfinite(value);
}

[[nodiscard]] inline Point cubic_point(
    const Point& p0,
    const Point& p1,
    const Point& p2,
    const Point& p3,
    double t) noexcept {
    const double u = 1.0 - t;
    const double uu = u * u;
    const double tt = t * t;
    return {
        uu * u * p0.x + 3.0 * uu * t * p1.x + 3.0 * u * tt * p2.x + tt * t * p3.x,
        uu * u * p0.y + 3.0 * uu * t * p1.y + 3.0 * u * tt * p2.y + tt * t * p3.y,
    };
}

[[nodiscard]] inline bool parse_path_data(std::string_view data, PaintPath& path) {
    std::size_t pos = 0U;
    Subpath* current = nullptr;
    Point current_point{};
    while (true) {
        skip_space(data, pos);
        if (pos >= data.size()) {
            break;
        }
        const char command = data[pos++];
        if (command == 'M') {
            double x = 0.0;
            double y = 0.0;
            if (!parse_number(data, pos, x) || !parse_number(data, pos, y)) {
                return false;
            }
            path.subpaths.push_back({});
            current = &path.subpaths.back();
            current_point = {x, y};
            current->points.push_back(current_point);
        } else if (command == 'L') {
            if (current == nullptr) {
                return false;
            }
            double x = 0.0;
            double y = 0.0;
            if (!parse_number(data, pos, x) || !parse_number(data, pos, y)) {
                return false;
            }
            current_point = {x, y};
            current->points.push_back(current_point);
        } else if (command == 'C') {
            if (current == nullptr) {
                return false;
            }
            Point c1{};
            Point c2{};
            Point end{};
            if (!parse_number(data, pos, c1.x) || !parse_number(data, pos, c1.y) ||
                !parse_number(data, pos, c2.x) || !parse_number(data, pos, c2.y) ||
                !parse_number(data, pos, end.x) || !parse_number(data, pos, end.y)) {
                return false;
            }
            constexpr std::size_t cubic_steps = 32U;
            const Point start = current_point;
            for (std::size_t step = 1U; step <= cubic_steps; ++step) {
                const double t = static_cast<double>(step) / static_cast<double>(cubic_steps);
                current->points.push_back(cubic_point(start, c1, c2, end, t));
            }
            current_point = end;
        } else if (command == 'Z') {
            if (current == nullptr || current->points.size() < 3U) {
                return false;
            }
            current = nullptr;
        } else {
            return false;
        }
    }
    if (current != nullptr || path.subpaths.empty()) {
        return false;
    }
    return true;
}

[[nodiscard]] inline bool extract_attribute(
    std::string_view element,
    std::string_view name,
    std::string_view& value) noexcept {
    const std::string needle = std::string(name) + "=\"";
    const std::size_t start = element.find(needle);
    if (start == std::string_view::npos) {
        return false;
    }
    const std::size_t value_start = start + needle.size();
    const std::size_t end = element.find('"', value_start);
    if (end == std::string_view::npos) {
        return false;
    }
    value = element.substr(value_start, end - value_start);
    return true;
}

[[nodiscard]] inline bool point_inside_subpath(const Subpath& subpath, double x, double y) noexcept {
    bool inside = false;
    const std::size_t count = subpath.points.size();
    for (std::size_t index = 0U, previous = count - 1U; index < count; previous = index++) {
        const Point& a = subpath.points[index];
        const Point& b = subpath.points[previous];
        const bool crosses = (a.y > y) != (b.y > y);
        if (crosses) {
            const double intersect_x = (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x;
            if (x < intersect_x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

[[nodiscard]] inline bool point_inside_even_odd(const PaintPath& path, double x, double y) noexcept {
    bool inside = false;
    for (const auto& subpath : path.subpaths) {
        if (point_inside_subpath(subpath, x, y)) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace final_svg_detail

// Parses and rasterizes only the deterministic SVG subset emitted by the canonical
// Vektoryum SVG encoder. It deliberately consumes final serialized bytes and does
// not accept SvgScene or reconstruction objects, keeping certification independent
// from the pre-serialization representation.
[[nodiscard]] inline FinalSvgRaster rasterize_final_serialized_svg(
    std::span<const std::uint8_t> svg_bytes,
    std::uint64_t max_pixels = 1'000'000U) {
    FinalSvgRaster result{};
    if (svg_bytes.empty()) {
        return result;
    }
    const std::string text(svg_bytes.begin(), svg_bytes.end());
    const std::string_view svg{text};
    const std::size_t root_end = svg.find('>');
    if (!svg.starts_with("<svg ") || root_end == std::string_view::npos || !svg.ends_with("</svg>\n")) {
        return result;
    }

    std::string_view width_text;
    std::string_view height_text;
    if (!final_svg_detail::extract_attribute(svg.substr(0U, root_end + 1U), "width", width_text) ||
        !final_svg_detail::extract_attribute(svg.substr(0U, root_end + 1U), "height", height_text) ||
        !final_svg_detail::parse_unsigned(width_text, result.width) ||
        !final_svg_detail::parse_unsigned(height_text, result.height)) {
        return result;
    }
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(result.width) * result.height;
    if (pixel_count == 0U || pixel_count > max_pixels || pixel_count > (std::numeric_limits<std::size_t>::max() / 4U)) {
        return result;
    }

    std::vector<final_svg_detail::PaintPath> paths;
    std::size_t scan = root_end + 1U;
    while (true) {
        const std::size_t path_start = svg.find("<path ", scan);
        if (path_start == std::string_view::npos) {
            break;
        }
        const std::size_t path_end = svg.find("/>\n", path_start);
        if (path_end == std::string_view::npos) {
            return result;
        }
        const std::string_view element = svg.substr(path_start, path_end + 3U - path_start);
        std::string_view data;
        std::string_view fill;
        std::string_view fill_rule;
        if (!final_svg_detail::extract_attribute(element, "d", data) ||
            !final_svg_detail::extract_attribute(element, "fill", fill) ||
            !final_svg_detail::extract_attribute(element, "fill-rule", fill_rule) ||
            fill_rule != "evenodd") {
            return result;
        }
        final_svg_detail::PaintPath path;
        if (!final_svg_detail::parse_rgb(fill, path.rgb) || !final_svg_detail::parse_path_data(data, path)) {
            return result;
        }
        paths.push_back(std::move(path));
        scan = path_end + 3U;
    }
    if (paths.empty()) {
        return result;
    }

    result.rgba8.assign(static_cast<std::size_t>(pixel_count) * 4U, 0U);
    for (std::uint32_t y = 0U; y < result.height; ++y) {
        for (std::uint32_t x = 0U; x < result.width; ++x) {
            const double px = static_cast<double>(x) + 0.5;
            const double py = static_cast<double>(y) + 0.5;
            std::array<std::uint8_t, 3U> rgb{0U, 0U, 0U};
            bool covered = false;
            for (const auto& path : paths) {
                if (final_svg_detail::point_inside_even_odd(path, px, py)) {
                    rgb = path.rgb;
                    covered = true;
                }
            }
            if (covered) {
                const std::size_t base = (static_cast<std::size_t>(y) * result.width + x) * 4U;
                result.rgba8[base] = rgb[0];
                result.rgba8[base + 1U] = rgb[1];
                result.rgba8[base + 2U] = rgb[2];
                result.rgba8[base + 3U] = 255U;
            }
        }
    }
    result.valid = true;
    return result;
}

}  // namespace vektoryum::certification
