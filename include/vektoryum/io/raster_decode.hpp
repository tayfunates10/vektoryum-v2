#pragma once

#include "vektoryum/core/image.hpp"
#include "vektoryum/io/raster_input.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vektoryum::io {

enum class RasterDecodeError : std::uint8_t {
    None = 0,
    UnsupportedFormat,
    MalformedContainer,
    UnsupportedFeature,
    DimensionLimitExceeded,
    PixelBudgetExceeded,
    TruncatedPixelData,
};

struct DecodedRaster {
    core::ImageSpec spec{};
    std::vector<std::uint8_t> rgba8;
};

struct RasterDecodeResult {
    RasterDecodeError error{RasterDecodeError::None};
    DecodedRaster image{};

    [[nodiscard]] bool ok() const noexcept {
        return error == RasterDecodeError::None;
    }
};

inline constexpr std::uint32_t raster_decode_max_dimension = 32768U;
inline constexpr std::size_t raster_decode_max_pixels = 64U * 1024U * 1024U;

// Decoders normalize accepted inputs to deterministic RGBA8, sRGB transfer,
// sRGB primaries and straight alpha. Format-specific decoders must fail closed
// when they encounter an unsupported feature instead of silently approximating it.
[[nodiscard]] RasterDecodeResult decode_raster(
    RasterFormat format,
    std::span<const std::uint8_t> bytes) noexcept;

[[nodiscard]] RasterDecodeResult decode_raster(const RasterInput& input) noexcept;

[[nodiscard]] const char* raster_decode_error_name(RasterDecodeError error) noexcept;

}  // namespace vektoryum::io
