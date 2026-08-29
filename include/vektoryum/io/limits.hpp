#pragma once

#include <cstddef>
#include <cstdint>

#include "vektoryum/core/image.hpp"

namespace vektoryum::io {

struct DecodeLimits {
    std::uint32_t max_width{65'536U};
    std::uint32_t max_height{65'536U};
    std::uint64_t max_pixels{268'435'456ULL};
    std::size_t max_decoded_bytes{2ULL * 1024ULL * 1024ULL * 1024ULL};
};

enum class DecodeLimitError : std::uint8_t {
    None,
    InvalidImageSpec,
    WidthExceeded,
    HeightExceeded,
    PixelCountExceeded,
    ByteBudgetExceeded,
};

struct DecodeLimitValidation {
    DecodeLimitError error{DecodeLimitError::None};
    std::size_t decoded_bytes{};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == DecodeLimitError::None;
    }
};

[[nodiscard]] DecodeLimitValidation validate_decode_limits(
    const core::ImageSpec& spec,
    const DecodeLimits& limits = {}) noexcept;

}  // namespace vektoryum::io
