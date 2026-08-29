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
};

struct PhotoRestorationOptions {
    float denoise_strength{0.20F};
    float sharpen_strength{0.15F};
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
// Output samples are clamped to the source-normalized [0, 1] range.
[[nodiscard]] PhotoRestorationResult restore_photo(
    const resample::FloatImage& source,
    PhotoRestorationOptions options = {});

}  // namespace vektoryum::restoration
