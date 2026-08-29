#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vektoryum::resample {

enum class Filter : std::uint8_t {
    Bilinear,
    Bicubic,
    Lanczos3,
};

enum class ResampleError : std::uint8_t {
    None,
    ZeroSourceDimension,
    ZeroTargetDimension,
    InvalidChannelCount,
    SourceSizeMismatch,
    SizeOverflow,
};

struct FloatImage {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint8_t channels{};
    std::vector<float> pixels{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y, std::uint8_t channel) const noexcept;
};

struct ResampleOptions {
    Filter filter{Filter::Lanczos3};
    bool clamp_to_local_range{true};
};

struct ResampleResult {
    ResampleError error{ResampleError::None};
    FloatImage image{};

    [[nodiscard]] bool ok() const noexcept {
        return error == ResampleError::None;
    }
};

// Contract: samples are finite linear-light values. For alpha imagery callers should
// pass premultiplied color channels so interpolation cannot reveal hidden RGB.
[[nodiscard]] ResampleResult resize(
    const FloatImage& source,
    std::uint32_t target_width,
    std::uint32_t target_height,
    ResampleOptions options = {});

}  // namespace vektoryum::resample
