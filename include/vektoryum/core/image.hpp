#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace vektoryum::core {

enum class ChannelType : std::uint8_t {
    UInt8,
    UInt16,
    Float32,
};

enum class PixelLayout : std::uint8_t {
    Gray,
    GrayAlpha,
    RGB,
    RGBA,
};

enum class TransferFunction : std::uint8_t {
    Linear,
    SRGB,
};

enum class ColorPrimaries : std::uint8_t {
    SRGB,
};

enum class AlphaMode : std::uint8_t {
    None,
    Straight,
    Premultiplied,
};

struct ImageSpec {
    std::uint32_t width{};
    std::uint32_t height{};
    PixelLayout layout{PixelLayout::RGBA};
    ChannelType channel_type{ChannelType::UInt8};
    TransferFunction transfer{TransferFunction::SRGB};
    ColorPrimaries primaries{ColorPrimaries::SRGB};
    AlphaMode alpha{AlphaMode::Straight};
};

enum class ImageSpecError : std::uint8_t {
    None,
    ZeroWidth,
    ZeroHeight,
    AlphaModeWithoutAlphaChannel,
    MissingAlphaMode,
    SizeOverflow,
};

struct ImageSpecValidation {
    ImageSpecError error{ImageSpecError::None};
    std::size_t byte_size{};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error == ImageSpecError::None;
    }
};

[[nodiscard]] constexpr std::uint8_t channel_count(PixelLayout layout) noexcept {
    switch (layout) {
        case PixelLayout::Gray:
            return 1;
        case PixelLayout::GrayAlpha:
            return 2;
        case PixelLayout::RGB:
            return 3;
        case PixelLayout::RGBA:
            return 4;
    }
    return 0;
}

[[nodiscard]] constexpr bool layout_has_alpha(PixelLayout layout) noexcept {
    return layout == PixelLayout::GrayAlpha || layout == PixelLayout::RGBA;
}

[[nodiscard]] constexpr std::uint8_t bytes_per_channel(ChannelType type) noexcept {
    switch (type) {
        case ChannelType::UInt8:
            return 1;
        case ChannelType::UInt16:
            return 2;
        case ChannelType::Float32:
            return 4;
    }
    return 0;
}

[[nodiscard]] constexpr std::uint8_t bits_per_channel(ChannelType type) noexcept {
    return static_cast<std::uint8_t>(bytes_per_channel(type) * 8U);
}

[[nodiscard]] ImageSpecValidation validate_image_spec(const ImageSpec& spec) noexcept;

[[nodiscard]] std::optional<std::size_t> checked_image_byte_size(const ImageSpec& spec) noexcept;

}  // namespace vektoryum::core
