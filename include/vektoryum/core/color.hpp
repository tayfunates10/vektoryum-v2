#pragma once

namespace vektoryum::core {

struct Rgba64 {
    double r{};
    double g{};
    double b{};
    double a{1.0};
};

[[nodiscard]] double srgb_to_linear(double encoded) noexcept;
[[nodiscard]] double linear_to_srgb(double linear) noexcept;
[[nodiscard]] double clamp_unit(double value) noexcept;

[[nodiscard]] Rgba64 premultiply_alpha(Rgba64 straight) noexcept;
[[nodiscard]] Rgba64 unpremultiply_alpha(Rgba64 premultiplied) noexcept;

}  // namespace vektoryum::core
