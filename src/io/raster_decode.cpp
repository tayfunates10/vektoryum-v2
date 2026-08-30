#include "vektoryum/io/raster_decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace vektoryum::io {
namespace {

enum class ByteOrder : std::uint8_t { Little, Big };

struct IfdEntry {
    std::uint16_t tag{};
    std::uint16_t type{};
    std::uint32_t count{};
    std::uint32_t value_or_offset{};
    std::size_t raw_value_offset{};
};

[[nodiscard]] bool checked_range(std::size_t offset, std::size_t length, std::size_t size) noexcept {
    return offset <= size && length <= size - offset;
}

[[nodiscard]] std::optional<std::uint16_t> read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    ByteOrder order) noexcept {
    if (!checked_range(offset, 2U, bytes.size())) {
        return std::nullopt;
    }
    const auto a = static_cast<std::uint16_t>(bytes[offset]);
    const auto b = static_cast<std::uint16_t>(bytes[offset + 1U]);
    if (order == ByteOrder::Little) {
        return static_cast<std::uint16_t>(a | static_cast<std::uint16_t>(b << 8U));
    }
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(a << 8U) | b);
}

[[nodiscard]] std::optional<std::uint32_t> read_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    ByteOrder order) noexcept {
    if (!checked_range(offset, 4U, bytes.size())) {
        return std::nullopt;
    }
    const auto b0 = static_cast<std::uint32_t>(bytes[offset]);
    const auto b1 = static_cast<std::uint32_t>(bytes[offset + 1U]);
    const auto b2 = static_cast<std::uint32_t>(bytes[offset + 2U]);
    const auto b3 = static_cast<std::uint32_t>(bytes[offset + 3U]);
    if (order == ByteOrder::Little) {
        return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
    }
    return (b0 << 24U) | (b1 << 16U) | (b2 << 8U) | b3;
}

[[nodiscard]] std::size_t tiff_type_size(std::uint16_t type) noexcept {
    switch (type) {
        case 3U: return 2U;  // SHORT
        case 4U: return 4U;  // LONG
        default: return 0U;
    }
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>> entry_values(
    std::span<const std::uint8_t> bytes,
    const IfdEntry& entry,
    ByteOrder order) {
    const std::size_t element_size = tiff_type_size(entry.type);
    if (element_size == 0U || entry.count == 0U) {
        return std::nullopt;
    }
    if (static_cast<std::size_t>(entry.count) > std::numeric_limits<std::size_t>::max() / element_size) {
        return std::nullopt;
    }
    const std::size_t total = static_cast<std::size_t>(entry.count) * element_size;
    const std::size_t base = total <= 4U ? entry.raw_value_offset : static_cast<std::size_t>(entry.value_or_offset);
    if (!checked_range(base, total, bytes.size())) {
        return std::nullopt;
    }

    std::vector<std::uint32_t> values;
    values.reserve(static_cast<std::size_t>(entry.count));
    for (std::size_t i = 0; i < static_cast<std::size_t>(entry.count); ++i) {
        const std::size_t at = base + i * element_size;
        if (entry.type == 3U) {
            const auto value = read_u16(bytes, at, order);
            if (!value.has_value()) {
                return std::nullopt;
            }
            values.push_back(static_cast<std::uint32_t>(*value));
        } else {
            const auto value = read_u32(bytes, at, order);
            if (!value.has_value()) {
                return std::nullopt;
            }
            values.push_back(*value);
        }
    }
    return values;
}

[[nodiscard]] std::optional<IfdEntry> find_entry(
    std::span<const IfdEntry> entries,
    std::uint16_t tag) noexcept {
    for (const IfdEntry& entry : entries) {
        if (entry.tag == tag) {
            return entry;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>> values_for_tag(
    std::span<const std::uint8_t> bytes,
    std::span<const IfdEntry> entries,
    std::uint16_t tag,
    ByteOrder order) {
    const auto entry = find_entry(entries, tag);
    if (!entry.has_value()) {
        return std::nullopt;
    }
    return entry_values(bytes, *entry, order);
}

[[nodiscard]] std::optional<std::uint32_t> scalar_for_tag(
    std::span<const std::uint8_t> bytes,
    std::span<const IfdEntry> entries,
    std::uint16_t tag,
    ByteOrder order) {
    const auto values = values_for_tag(bytes, entries, tag, order);
    if (!values.has_value() || values->size() != 1U) {
        return std::nullopt;
    }
    return (*values)[0U];
}

[[nodiscard]] RasterDecodeResult fail(RasterDecodeError error) noexcept {
    return {error, {}};
}

[[nodiscard]] std::uint8_t unpremultiply(std::uint8_t value, std::uint8_t alpha) noexcept {
    if (alpha == 0U) {
        return 0U;
    }
    const auto numerator = static_cast<std::uint32_t>(value) * 255U + static_cast<std::uint32_t>(alpha) / 2U;
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(255U, numerator / static_cast<std::uint32_t>(alpha)));
}

[[nodiscard]] RasterDecodeResult decode_tiff(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8U) {
        return fail(RasterDecodeError::MalformedContainer);
    }

    ByteOrder order{};
    if (bytes[0U] == 0x49U && bytes[1U] == 0x49U) {
        order = ByteOrder::Little;
    } else if (bytes[0U] == 0x4dU && bytes[1U] == 0x4dU) {
        order = ByteOrder::Big;
    } else {
        return fail(RasterDecodeError::MalformedContainer);
    }

    const auto magic = read_u16(bytes, 2U, order);
    const auto ifd_offset_value = read_u32(bytes, 4U, order);
    if (!magic.has_value() || *magic != 42U || !ifd_offset_value.has_value()) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    const std::size_t ifd_offset = static_cast<std::size_t>(*ifd_offset_value);
    const auto entry_count_value = read_u16(bytes, ifd_offset, order);
    if (!entry_count_value.has_value()) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    const std::size_t entry_count = static_cast<std::size_t>(*entry_count_value);
    if (entry_count > 256U || entry_count > (std::numeric_limits<std::size_t>::max() - 2U) / 12U) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    const std::size_t entries_start = ifd_offset + 2U;
    const std::size_t entries_bytes = entry_count * 12U;
    if (entries_start < ifd_offset || !checked_range(entries_start, entries_bytes + 4U, bytes.size())) {
        return fail(RasterDecodeError::MalformedContainer);
    }

    std::vector<IfdEntry> entries;
    entries.reserve(entry_count);
    for (std::size_t i = 0; i < entry_count; ++i) {
        const std::size_t offset = entries_start + i * 12U;
        const auto tag = read_u16(bytes, offset, order);
        const auto type = read_u16(bytes, offset + 2U, order);
        const auto count = read_u32(bytes, offset + 4U, order);
        const auto value = read_u32(bytes, offset + 8U, order);
        if (!tag.has_value() || !type.has_value() || !count.has_value() || !value.has_value()) {
            return fail(RasterDecodeError::MalformedContainer);
        }
        entries.push_back(IfdEntry{*tag, *type, *count, *value, offset + 8U});
    }

    const auto width = scalar_for_tag(bytes, entries, 256U, order);
    const auto height = scalar_for_tag(bytes, entries, 257U, order);
    const auto compression = scalar_for_tag(bytes, entries, 259U, order);
    const auto photometric = scalar_for_tag(bytes, entries, 262U, order);
    const auto samples = scalar_for_tag(bytes, entries, 277U, order);
    const auto strip_offsets = values_for_tag(bytes, entries, 273U, order);
    const auto strip_byte_counts = values_for_tag(bytes, entries, 279U, order);
    const auto bits = values_for_tag(bytes, entries, 258U, order);
    if (!width.has_value() || !height.has_value() || !compression.has_value() || !photometric.has_value() ||
        !samples.has_value() || !strip_offsets.has_value() || !strip_byte_counts.has_value() || !bits.has_value()) {
        return fail(RasterDecodeError::MalformedContainer);
    }

    if (*width == 0U || *height == 0U || *width > raster_decode_max_dimension || *height > raster_decode_max_dimension) {
        return fail(RasterDecodeError::DimensionLimitExceeded);
    }
    const std::size_t width_size = static_cast<std::size_t>(*width);
    const std::size_t height_size = static_cast<std::size_t>(*height);
    if (width_size > raster_decode_max_pixels / height_size) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }
    const std::size_t pixel_count = width_size * height_size;
    if (pixel_count > raster_decode_max_pixels) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }

    if (*compression != 1U) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    const auto planar = scalar_for_tag(bytes, entries, 284U, order);
    if (planar.has_value() && *planar != 1U) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    if (strip_offsets->size() != 1U || strip_byte_counts->size() != 1U) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    if (bits->size() != static_cast<std::size_t>(*samples)) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    for (const std::uint32_t bit_depth : *bits) {
        if (bit_depth != 8U) {
            return fail(RasterDecodeError::UnsupportedFeature);
        }
    }

    bool grayscale = false;
    bool associated_alpha = false;
    if (*photometric == 1U && *samples == 1U) {
        grayscale = true;
    } else if (*photometric == 2U && *samples == 3U) {
        grayscale = false;
    } else if (*photometric == 2U && *samples == 4U) {
        const auto extras = values_for_tag(bytes, entries, 338U, order);
        if (!extras.has_value() || extras->size() != 1U || ((*extras)[0U] != 1U && (*extras)[0U] != 2U)) {
            return fail(RasterDecodeError::UnsupportedFeature);
        }
        associated_alpha = (*extras)[0U] == 1U;
    } else {
        return fail(RasterDecodeError::UnsupportedFeature);
    }

    const std::size_t sample_count = static_cast<std::size_t>(*samples);
    if (pixel_count > std::numeric_limits<std::size_t>::max() / sample_count) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }
    const std::size_t expected_bytes = pixel_count * sample_count;
    if ((*strip_byte_counts)[0U] != expected_bytes) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    const std::size_t pixel_offset = static_cast<std::size_t>((*strip_offsets)[0U]);
    if (!checked_range(pixel_offset, expected_bytes, bytes.size())) {
        return fail(RasterDecodeError::TruncatedPixelData);
    }
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }

    DecodedRaster decoded{};
    decoded.spec.width = *width;
    decoded.spec.height = *height;
    decoded.spec.layout = core::PixelLayout::RGBA;
    decoded.spec.channel_type = core::ChannelType::UInt8;
    decoded.spec.transfer = core::TransferFunction::SRGB;
    decoded.spec.primaries = core::ColorPrimaries::SRGB;
    decoded.spec.alpha = core::AlphaMode::Straight;
    decoded.rgba8.resize(pixel_count * 4U);

    for (std::size_t i = 0; i < pixel_count; ++i) {
        const std::size_t source = pixel_offset + i * sample_count;
        const std::size_t target = i * 4U;
        if (grayscale) {
            const std::uint8_t value = bytes[source];
            decoded.rgba8[target] = value;
            decoded.rgba8[target + 1U] = value;
            decoded.rgba8[target + 2U] = value;
            decoded.rgba8[target + 3U] = 255U;
        } else {
            std::uint8_t red = bytes[source];
            std::uint8_t green = bytes[source + 1U];
            std::uint8_t blue = bytes[source + 2U];
            const std::uint8_t alpha = sample_count == 4U ? bytes[source + 3U] : 255U;
            if (associated_alpha) {
                red = unpremultiply(red, alpha);
                green = unpremultiply(green, alpha);
                blue = unpremultiply(blue, alpha);
            }
            decoded.rgba8[target] = red;
            decoded.rgba8[target + 1U] = green;
            decoded.rgba8[target + 2U] = blue;
            decoded.rgba8[target + 3U] = alpha;
        }
    }

    return {RasterDecodeError::None, std::move(decoded)};
}

}  // namespace

RasterDecodeResult decode_raster(RasterFormat format, std::span<const std::uint8_t> bytes) noexcept {
    try {
        switch (format) {
            case RasterFormat::Tiff: return decode_tiff(bytes);
            case RasterFormat::Png:
            case RasterFormat::Jpeg:
            case RasterFormat::Webp:
            case RasterFormat::Unknown: return fail(RasterDecodeError::UnsupportedFormat);
        }
    } catch (const std::bad_alloc&) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    } catch (...) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    return fail(RasterDecodeError::UnsupportedFormat);
}

RasterDecodeResult decode_raster(const RasterInput& input) noexcept {
    return decode_raster(input.format, input.bytes);
}

const char* raster_decode_error_name(RasterDecodeError error) noexcept {
    switch (error) {
        case RasterDecodeError::None: return "none";
        case RasterDecodeError::UnsupportedFormat: return "unsupported_format";
        case RasterDecodeError::MalformedContainer: return "malformed_container";
        case RasterDecodeError::UnsupportedFeature: return "unsupported_feature";
        case RasterDecodeError::DimensionLimitExceeded: return "dimension_limit_exceeded";
        case RasterDecodeError::PixelBudgetExceeded: return "pixel_budget_exceeded";
        case RasterDecodeError::TruncatedPixelData: return "truncated_pixel_data";
    }
    return "unknown";
}

}  // namespace vektoryum::io
