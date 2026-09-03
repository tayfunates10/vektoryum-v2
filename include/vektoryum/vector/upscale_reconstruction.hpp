#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "vektoryum/core/color.hpp"
#include "vektoryum/resample/resampler.hpp"
#include "vektoryum/vector/reconstruction.hpp"
#include "vektoryum/vector/source_paint.hpp"
#include "vektoryum/vector/svg_path.hpp"

namespace vektoryum::vector {

struct UpscaleReconstructionResult {
    bool valid{false};
    SvgScene scene{};
    std::vector<std::uint8_t> rgba8{};
    std::vector<std::uint8_t> coverage{};
};

namespace detail {

[[nodiscard]] inline std::uint8_t unit_to_u8(double value) noexcept {
    const double clamped = core::clamp_unit(value);
    return static_cast<std::uint8_t>(clamped * 255.0 + 0.5);
}

inline void scale_svg_path(SvgPath& path, double scale_x, double scale_y) noexcept {
    for (auto& command : path.commands) {
        command.control1.x *= scale_x;
        command.control1.y *= scale_y;
        command.control2.x *= scale_x;
        command.control2.y *= scale_y;
        command.end.x *= scale_x;
        command.end.y *= scale_y;
    }
}

inline void scale_svg_scene(
    SvgScene& scene,
    std::uint32_t target_width,
    std::uint32_t target_height) noexcept {
    const double scale_x = static_cast<double>(target_width) / static_cast<double>(scene.width);
    const double scale_y = static_cast<double>(target_height) / static_cast<double>(scene.height);
    for (auto& path : scene.paths) {
        scale_svg_path(path, scale_x, scale_y);
    }
    for (auto& layer : scene.paint_layers) {
        for (auto& path : layer.paths) {
            scale_svg_path(path, scale_x, scale_y);
        }
    }
    scene.width = target_width;
    scene.height = target_height;
}

}  // namespace detail

// Converts the actual premultiplied-linear RGBA upscale result back to a
// deterministic straight-sRGB reconstruction surface, reconstructs geometry at
// the upscaled resolution, derives paint from that same upscaled surface, and
// only then maps fitted double-precision SVG coordinates into source space.
// This prevents a pipeline from computing an upscale merely for provenance
// while reconstructing from the original-resolution mask.
[[nodiscard]] inline UpscaleReconstructionResult reconstruct_from_upscaled_rgba(
    const resample::FloatImage& upscaled,
    bool source_has_transparency,
    std::uint32_t target_width,
    std::uint32_t target_height) {
    UpscaleReconstructionResult result{};
    if (upscaled.width == 0U || upscaled.height == 0U || upscaled.channels != 4U ||
        target_width == 0U || target_height == 0U) {
        return result;
    }
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(upscaled.width) * static_cast<std::uint64_t>(upscaled.height);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U ||
        upscaled.pixels.size() != static_cast<std::size_t>(pixel_count) * 4U) {
        return result;
    }

    result.rgba8.resize(static_cast<std::size_t>(pixel_count) * 4U);
    result.coverage.resize(static_cast<std::size_t>(pixel_count), 0U);
    for (std::size_t pixel = 0U; pixel < static_cast<std::size_t>(pixel_count); ++pixel) {
        const std::size_t base = pixel * 4U;
        const core::Rgba64 premultiplied{
            core::clamp_unit(static_cast<double>(upscaled.pixels[base])),
            core::clamp_unit(static_cast<double>(upscaled.pixels[base + 1U])),
            core::clamp_unit(static_cast<double>(upscaled.pixels[base + 2U])),
            core::clamp_unit(static_cast<double>(upscaled.pixels[base + 3U])),
        };
        const core::Rgba64 straight = core::unpremultiply_alpha(premultiplied);
        const std::uint8_t r = detail::unit_to_u8(core::linear_to_srgb(straight.r));
        const std::uint8_t g = detail::unit_to_u8(core::linear_to_srgb(straight.g));
        const std::uint8_t b = detail::unit_to_u8(core::linear_to_srgb(straight.b));
        const std::uint8_t a = detail::unit_to_u8(straight.a);
        result.rgba8[base] = r;
        result.rgba8[base + 1U] = g;
        result.rgba8[base + 2U] = b;
        result.rgba8[base + 3U] = a;

        if (source_has_transparency) {
            result.coverage[pixel] = a;
        } else {
            const std::uint32_t luminance =
                54U * static_cast<std::uint32_t>(r) +
                183U * static_cast<std::uint32_t>(g) +
                19U * static_cast<std::uint32_t>(b);
            result.coverage[pixel] = luminance < (128U * 256U) ? 255U : 0U;
        }
    }

    const auto reconstructed = reconstruct_binary_mask(
        result.coverage,
        upscaled.width,
        upscaled.height);
    if (!reconstructed.ok()) {
        return result;
    }
    const auto fitted = fit_svg_paths(reconstructed.scene);
    if (!fitted.ok()) {
        return result;
    }
    result.scene = fitted.scene;
    if (!attach_source_fill_rgb(result.scene, result.rgba8, result.coverage)) {
        result.scene = {};
        return result;
    }
    detail::scale_svg_scene(result.scene, target_width, target_height);
    result.valid = true;
    return result;
}

}  // namespace vektoryum::vector
