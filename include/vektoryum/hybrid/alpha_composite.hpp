#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace vektoryum::hybrid {

struct RgbaSample {
    double r{0.0};
    double g{0.0};
    double b{0.0};
    double a{0.0};
};

enum class AlphaCompositeError : std::uint8_t {
    None,
    EmptyLayers,
    InvalidChannel,
    HiddenRgbInTransparentPixel,
};

struct AlphaCompositeResult {
    AlphaCompositeError error{AlphaCompositeError::None};
    std::size_t layer_index{0U};
    RgbaSample output{};

    [[nodiscard]] bool ok() const noexcept { return error == AlphaCompositeError::None; }
};

[[nodiscard]] AlphaCompositeResult composite_alpha_safe(std::span<const RgbaSample> layers);

}  // namespace vektoryum::hybrid
