#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "vektoryum/core/color.hpp"
#include "vektoryum/resample/resampler.hpp"
#include "vektoryum/vector/curve_recovery.hpp"
#include "vektoryum/vector/reconstruction.hpp"
#include "vektoryum/vector/source_paint.hpp"
#include "vektoryum/vector/svg_path.hpp"

namespace vektoryum::vector {

struct UpscaleReconstructionResult {
    bool valid{false};
    bool used_curves{false};
    SvgScene scene{};
    SvgCertificationReport curve_certification{};
    std::vector<std::uint8_t> rgba8{};
    std::vector<std::uint8_t> coverage{};
};

namespace detail {

[[nodiscard]] inline std::uint8_t unit_to_u8(double value) noexcept {
    const double clamped = core::clamp_unit(value);
    return static_cast<std::uint8_t>(clamped * 255.0 + 0.5);
}

[[nodiscard]] inline bool decode_reconstruction_surface(
    const resample::FloatImage& surface,
    bool source_has_transparency,
    std::vector<std::uint8_t>& rgba8,
    std::vector<std::uint8_t>& coverage) {
    if (surface.width == 0U || surface.height == 0U || surface.channels != 4U) {
        return false;
    }
    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(surface.width) * static_cast<std::uint64_t>(surface.height);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U ||
        surface.pixels.size() != static_cast<std::size_t>(pixel_count) * 4U) {
        return false;
    }

    rgba8.resize(static_cast<std::size_t>(pixel_count) * 4U);
    coverage.resize(static_cast<std::size_t>(pixel_count), 0U);
    for (std::size_t pixel = 0U; pixel < static_cast<std::size_t>(pixel_count); ++pixel) {
        const std::size_t base = pixel * 4U;
        const core::Rgba64 premultiplied{
            core::clamp_unit(static_cast<double>(surface.pixels[base])),
            core::clamp_unit(static_cast<double>(surface.pixels[base + 1U])),
            core::clamp_unit(static_cast<double>(surface.pixels[base + 2U])),
            core::clamp_unit(static_cast<double>(surface.pixels[base + 3U])),
        };
        const core::Rgba64 straight = core::unpremultiply_alpha(premultiplied);
        const std::uint8_t r = unit_to_u8(core::linear_to_srgb(straight.r));
        const std::uint8_t g = unit_to_u8(core::linear_to_srgb(straight.g));
        const std::uint8_t b = unit_to_u8(core::linear_to_srgb(straight.b));
        const std::uint8_t a = unit_to_u8(straight.a);
        rgba8[base] = r;
        rgba8[base + 1U] = g;
        rgba8[base + 2U] = b;
        rgba8[base + 3U] = a;

        if (source_has_transparency) {
            coverage[pixel] = a;
        } else {
            const std::uint32_t luminance =
                54U * static_cast<std::uint32_t>(r) +
                183U * static_cast<std::uint32_t>(g) +
                19U * static_cast<std::uint32_t>(b);
            coverage[pixel] = luminance < (128U * 256U) ? 255U : 0U;
        }
    }
    return true;
}

}  // namespace detail

// The actual upscale output remains the sole reconstruction authority. Before
// vectorization it is deterministically sampled back onto the requested source
// grid so the existing source-space fidelity gate can compare like-for-like
// pixels without loosening its IoU/disagreement thresholds. No original mask or
// decoded source pixels participate in geometry or paint reconstruction here.
// Cubic recovery is attempted on the production geometry and is accepted only
// through the existing certify_svg_scene thresholds (IoU >= 0.995 and
// disagreement <= 0.005); rejected candidates fail back to the exact polygon.
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

    resample::FloatImage reconstruction_surface = upscaled;
    if (upscaled.width != target_width || upscaled.height != target_height) {
        const auto sampled = resample::resize(
            upscaled,
            target_width,
            target_height,
            resample::ResampleOptions{resample::Filter::Lanczos3, true});
        if (!sampled.ok()) {
            return result;
        }
        reconstruction_surface = sampled.image;
    }

    if (!detail::decode_reconstruction_surface(
            reconstruction_surface,
            source_has_transparency,
            result.rgba8,
            result.coverage)) {
        return result;
    }

    const auto reconstructed = reconstruct_binary_mask(
        result.coverage,
        target_width,
        target_height);
    if (!reconstructed.ok()) {
        return result;
    }

    std::vector<std::uint8_t> reference_mask(result.coverage.size(), 0U);
    std::transform(
        result.coverage.begin(),
        result.coverage.end(),
        reference_mask.begin(),
        [](std::uint8_t value) {
            return static_cast<std::uint8_t>(coverage_is_foreground(value));
        });

    const auto recovered = recover_curves_certified(
        reconstructed.scene,
        reference_mask,
        target_width,
        target_height);
    if (!recovered.ok()) {
        return result;
    }
    result.scene = recovered.scene;
    result.used_curves = recovered.used_curves;
    result.curve_certification = recovered.certification;

    if (!attach_source_fill_rgb(result.scene, result.rgba8, result.coverage)) {
        result.scene = {};
        return result;
    }
    result.valid = true;
    return result;
}

}  // namespace vektoryum::vector
