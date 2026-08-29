#include "vektoryum/core/color.hpp"

#include <cmath>

namespace vektoryum::core {

double clamp_unit(double value) noexcept {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

double srgb_to_linear(double encoded) noexcept {
    if (encoded <= 0.04045) {
        return encoded / 12.92;
    }
    return std::pow((encoded + 0.055) / 1.055, 2.4);
}

double linear_to_srgb(double linear) noexcept {
    if (linear <= 0.0031308) {
        return 12.92 * linear;
    }
    return 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
}

Rgba64 premultiply_alpha(Rgba64 straight) noexcept {
    const double alpha = clamp_unit(straight.a);
    return {
        straight.r * alpha,
        straight.g * alpha,
        straight.b * alpha,
        alpha,
    };
}

Rgba64 unpremultiply_alpha(Rgba64 premultiplied) noexcept {
    const double alpha = clamp_unit(premultiplied.a);
    if (alpha <= 0.0) {
        return {0.0, 0.0, 0.0, 0.0};
    }
    return {
        premultiplied.r / alpha,
        premultiplied.g / alpha,
        premultiplied.b / alpha,
        alpha,
    };
}

}  // namespace vektoryum::core
