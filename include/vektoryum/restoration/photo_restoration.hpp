#pragma once

#include <cstdint>

#include "vektoryum/resample/resampler.hpp"

namespace vektoryum::restoration {

enum class RestorationError : std::uint8_t {
    None,
    InvalidImage,
    InvalidChannelCount,
    NonFiniteSample,
    SampleOutOfRange,
    InvalidOption,
    OutputTooLarge,
};

struct PhotoRestorationOptions {
    float denoise_strength{0.20F};
    float deblock_strength{0.15F};
    float sharpen_strength{0.15F};
};

struct SuperResolutionOptions {
    std::uint32_t scale{2U};
    std::uint64_t max_output_pixels{64ULL * 1024ULL * 1024ULL};
};

struct PhotoRestorationResult {
    RestorationError error{RestorationError::None};
    resample::FloatImage image{};

    [[nodiscard]] bool ok() const noexcept {
        return error == RestorationError::None;
    }
};

// Deterministic non-ML restoration for normalized linear-light samples.
// RGB/gray channels are processed; alpha, when present, is copied exactly.
// RGBA is treated as premultiplied and RGB never exceeds alpha.
// Output samples stay inside both [0, 1] and the local source envelope.
[[nodiscard]] PhotoRestorationResult restore_photo(
    const resample::FloatImage& source,
    PhotoRestorationOptions options = {});

// Deterministic project-owned bilinear non-ML super-resolution.
// Supported scales are 2x and 4x. RGBA interpolation is premultiplied-alpha safe,
// and every reconstructed channel is bounded by its four source contributors.
[[nodiscard]] PhotoRestorationResult super_resolve_photo(
    const resample::FloatImage& source,
    SuperResolutionOptions options = {});

}  // namespace vektoryum::restoration
