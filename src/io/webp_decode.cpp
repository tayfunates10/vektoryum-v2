#include "vektoryum/io/raster_decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace vektoryum::io::detail {
namespace {

class LsbBitReader {
public:
    explicit LsbBitReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] std::optional<std::uint32_t> read(std::uint32_t count) noexcept {
        if (count > 32U || bit_offset_ > bytes_.size() * 8U ||
            static_cast<std::size_t>(count) > bytes_.size() * 8U - bit_offset_) {
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        for (std::uint32_t i = 0U; i < count; ++i) {
            const std::size_t absolute = bit_offset_ + static_cast<std::size_t>(i);
            const auto bit = static_cast<std::uint32_t>((bytes_[absolute / 8U] >> (absolute % 8U)) & 1U);
            value |= bit << i;
        }
        bit_offset_ += static_cast<std::size_t>(count);
        return value;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t bit_offset_{0U};
};

struct SingletonPrefixGroup {
    std::uint16_t green{};
    std::uint8_t red{};
    std::uint8_t blue{};
    std::uint8_t alpha{};
    std::uint8_t distance{};
};

[[nodiscard]] RasterDecodeResult fail(RasterDecodeError error) noexcept {
    return {error, {}};
}

[[nodiscard]] std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

[[nodiscard]] bool fourcc(std::span<const std::uint8_t> bytes, std::size_t offset, const std::array<char, 4U>& text) noexcept {
    return bytes.size() >= offset + 4U &&
           bytes[offset] == static_cast<std::uint8_t>(text[0U]) &&
           bytes[offset + 1U] == static_cast<std::uint8_t>(text[1U]) &&
           bytes[offset + 2U] == static_cast<std::uint8_t>(text[2U]) &&
           bytes[offset + 3U] == static_cast<std::uint8_t>(text[3U]);
}

[[nodiscard]] std::optional<std::uint16_t> read_singleton_symbol(
    LsbBitReader& reader,
    std::uint16_t alphabet_size) noexcept {
    const auto simple = reader.read(1U);
    if (!simple.has_value() || *simple != 1U) {
        return std::nullopt;
    }
    const auto symbol_count_minus_one = reader.read(1U);
    if (!symbol_count_minus_one.has_value() || *symbol_count_minus_one != 0U) {
        return std::nullopt;
    }
    const auto first_is_eight_bits = reader.read(1U);
    if (!first_is_eight_bits.has_value()) {
        return std::nullopt;
    }
    const auto symbol = reader.read(*first_is_eight_bits == 0U ? 1U : 8U);
    if (!symbol.has_value() || *symbol >= alphabet_size) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*symbol);
}

[[nodiscard]] std::optional<SingletonPrefixGroup> read_singleton_prefix_group(
    LsbBitReader& reader,
    std::uint16_t green_alphabet_size) noexcept {
    const auto green = read_singleton_symbol(reader, green_alphabet_size);
    const auto red = read_singleton_symbol(reader, 256U);
    const auto blue = read_singleton_symbol(reader, 256U);
    const auto alpha = read_singleton_symbol(reader, 256U);
    const auto distance = read_singleton_symbol(reader, 40U);
    if (!green.has_value() || !red.has_value() || !blue.has_value() || !alpha.has_value() || !distance.has_value()) {
        return std::nullopt;
    }
    return SingletonPrefixGroup{
        *green,
        static_cast<std::uint8_t>(*red),
        static_cast<std::uint8_t>(*blue),
        static_cast<std::uint8_t>(*alpha),
        static_cast<std::uint8_t>(*distance),
    };
}

[[nodiscard]] std::optional<std::array<std::uint8_t, 4U>> read_single_literal_image(
    LsbBitReader& reader,
    bool allow_meta_prefix) noexcept {
    const auto color_cache_used = reader.read(1U);
    if (!color_cache_used.has_value() || *color_cache_used != 0U) {
        return std::nullopt;
    }
    if (allow_meta_prefix) {
        const auto meta_prefix_used = reader.read(1U);
        if (!meta_prefix_used.has_value() || *meta_prefix_used != 0U) {
            return std::nullopt;
        }
    }
    const auto group = read_singleton_prefix_group(reader, 280U);
    if (!group.has_value() || group->green >= 256U) {
        return std::nullopt;
    }
    return std::array<std::uint8_t, 4U>{
        group->red,
        static_cast<std::uint8_t>(group->green),
        group->blue,
        group->alpha,
    };
}

}  // namespace

RasterDecodeResult decode_webp(std::span<const std::uint8_t> bytes) noexcept {
    try {
        constexpr std::size_t riff_header_size = 20U;
        if (bytes.size() < riff_header_size + 5U ||
            !fourcc(bytes, 0U, {'R', 'I', 'F', 'F'}) ||
            !fourcc(bytes, 8U, {'W', 'E', 'B', 'P'}) ||
            !fourcc(bytes, 12U, {'V', 'P', '8', 'L'})) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        const std::uint32_t riff_size = read_le32(bytes, 4U);
        const std::uint32_t vp8l_size = read_le32(bytes, 16U);
        if (riff_size != bytes.size() - 8U ||
            static_cast<std::size_t>(vp8l_size) > bytes.size() - riff_header_size ||
            vp8l_size < 5U) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        const auto payload = bytes.subspan(riff_header_size, static_cast<std::size_t>(vp8l_size));
        if (payload[0U] != 0x2fU) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        LsbBitReader reader(payload.subspan(1U));
        const auto width_minus_one = reader.read(14U);
        const auto height_minus_one = reader.read(14U);
        const auto alpha_is_used = reader.read(1U);
        const auto version = reader.read(3U);
        if (!width_minus_one.has_value() || !height_minus_one.has_value() ||
            !alpha_is_used.has_value() || !version.has_value() || *version != 0U) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        const std::uint32_t width = *width_minus_one + 1U;
        const std::uint32_t height = *height_minus_one + 1U;
        if (width > raster_decode_max_dimension || height > raster_decode_max_dimension ||
            static_cast<std::size_t>(width) > raster_decode_max_pixels / static_cast<std::size_t>(height)) {
            return fail(RasterDecodeError::PixelBudgetExceeded);
        }

        // R2 starts with a deliberately bounded, standards-parsed VP8L subset:
        // one literal pixel, optionally represented through a one-entry color-indexing transform.
        // Unsupported valid VP8L features fail closed instead of being approximated.
        if (width != 1U || height != 1U) {
            return fail(RasterDecodeError::UnsupportedFeature);
        }

        const auto transform_present = reader.read(1U);
        if (!transform_present.has_value()) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        std::array<std::uint8_t, 4U> pixel{};
        if (*transform_present == 0U) {
            const auto literal = read_single_literal_image(reader, true);
            if (!literal.has_value()) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            pixel = *literal;
        } else {
            const auto transform_type = reader.read(2U);
            if (!transform_type.has_value() || *transform_type != 3U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            const auto color_table_size_minus_one = reader.read(8U);
            if (!color_table_size_minus_one.has_value() || *color_table_size_minus_one != 0U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }

            const auto palette_delta = read_single_literal_image(reader, false);
            if (!palette_delta.has_value()) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            const auto another_transform = reader.read(1U);
            if (!another_transform.has_value() || *another_transform != 0U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            const auto indexed_literal = read_single_literal_image(reader, true);
            if (!indexed_literal.has_value() || (*indexed_literal)[1U] != 0U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            pixel = *palette_delta;
        }

        DecodedRaster decoded{};
        decoded.spec.width = width;
        decoded.spec.height = height;
        decoded.spec.layout = core::PixelLayout::RGBA;
        decoded.spec.channel_type = core::ChannelType::UInt8;
        decoded.spec.transfer = core::TransferFunction::SRGB;
        decoded.spec.primaries = core::ColorPrimaries::SRGB;
        decoded.spec.alpha = core::AlphaMode::Straight;
        decoded.rgba8.assign(pixel.begin(), pixel.end());
        return {RasterDecodeError::None, std::move(decoded)};
    } catch (...) {
        return fail(RasterDecodeError::MalformedContainer);
    }
}

}  // namespace vektoryum::io::detail
