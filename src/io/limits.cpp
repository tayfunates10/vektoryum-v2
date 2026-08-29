#include "vektoryum/io/limits.hpp"

namespace vektoryum::io {

DecodeLimitValidation validate_decode_limits(
    const core::ImageSpec& spec,
    const DecodeLimits& limits) noexcept {
    const auto image_validation = core::validate_image_spec(spec);
    if (!image_validation.ok()) {
        return {DecodeLimitError::InvalidImageSpec, 0U};
    }
    if (spec.width > limits.max_width) {
        return {DecodeLimitError::WidthExceeded, image_validation.byte_size};
    }
    if (spec.height > limits.max_height) {
        return {DecodeLimitError::HeightExceeded, image_validation.byte_size};
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(spec.width) * static_cast<std::uint64_t>(spec.height);
    if (pixels > limits.max_pixels) {
        return {DecodeLimitError::PixelCountExceeded, image_validation.byte_size};
    }
    if (image_validation.byte_size > limits.max_decoded_bytes) {
        return {DecodeLimitError::ByteBudgetExceeded, image_validation.byte_size};
    }

    return {DecodeLimitError::None, image_validation.byte_size};
}

}  // namespace vektoryum::io
