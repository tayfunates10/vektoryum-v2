#include "vektoryum/core/image.hpp"

#include <limits>

namespace vektoryum::core {
namespace {

[[nodiscard]] std::optional<std::size_t> checked_multiply(
    std::size_t left,
    std::size_t right) noexcept {
    if (left == 0U || right == 0U) {
        return std::size_t{0};
    }
    if (left > std::numeric_limits<std::size_t>::max() / right) {
        return std::nullopt;
    }
    return left * right;
}

[[nodiscard]] std::optional<std::size_t> raw_byte_size(const ImageSpec& spec) noexcept {
    auto pixels = checked_multiply(
        static_cast<std::size_t>(spec.width),
        static_cast<std::size_t>(spec.height));
    if (!pixels.has_value()) {
        return std::nullopt;
    }

    auto samples = checked_multiply(
        *pixels,
        static_cast<std::size_t>(channel_count(spec.layout)));
    if (!samples.has_value()) {
        return std::nullopt;
    }

    return checked_multiply(
        *samples,
        static_cast<std::size_t>(bytes_per_channel(spec.channel_type)));
}

}  // namespace

ImageSpecValidation validate_image_spec(const ImageSpec& spec) noexcept {
    if (spec.width == 0U) {
        return {ImageSpecError::ZeroWidth, 0U};
    }
    if (spec.height == 0U) {
        return {ImageSpecError::ZeroHeight, 0U};
    }

    const bool has_alpha = layout_has_alpha(spec.layout);
    if (!has_alpha && spec.alpha != AlphaMode::None) {
        return {ImageSpecError::AlphaModeWithoutAlphaChannel, 0U};
    }
    if (has_alpha && spec.alpha == AlphaMode::None) {
        return {ImageSpecError::MissingAlphaMode, 0U};
    }

    const auto byte_size = raw_byte_size(spec);
    if (!byte_size.has_value()) {
        return {ImageSpecError::SizeOverflow, 0U};
    }

    return {ImageSpecError::None, *byte_size};
}

std::optional<std::size_t> checked_image_byte_size(const ImageSpec& spec) noexcept {
    const auto validation = validate_image_spec(spec);
    if (!validation.ok()) {
        return std::nullopt;
    }
    return validation.byte_size;
}

}  // namespace vektoryum::core
