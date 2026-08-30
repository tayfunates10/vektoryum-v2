#include "vektoryum/io/raster_decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace vektoryum::io::detail {
namespace {

[[nodiscard]] bool checked_range(std::size_t offset, std::size_t length, std::size_t size) noexcept {
    return offset <= size && length <= size - offset;
}

[[nodiscard]] std::uint32_t read_be32(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3U]);
}

[[nodiscard]] std::uint32_t crc32_update(std::uint32_t crc, std::uint8_t value) noexcept {
    crc ^= static_cast<std::uint32_t>(value);
    for (unsigned bit = 0U; bit < 8U; ++bit) {
        const std::uint32_t mask = 0U - (crc & 1U);
        crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
    return crc;
}

[[nodiscard]] std::uint32_t chunk_crc(
    std::span<const std::uint8_t> type,
    std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t value : type) {
        crc = crc32_update(crc, value);
    }
    for (const std::uint8_t value : data) {
        crc = crc32_update(crc, value);
    }
    return crc ^ 0xffffffffU;
}

[[nodiscard]] std::uint32_t adler32(std::span<const std::uint8_t> data) noexcept {
    constexpr std::uint32_t modulus = 65521U;
    std::uint32_t a = 1U;
    std::uint32_t b = 0U;
    for (const std::uint8_t value : data) {
        a = (a + static_cast<std::uint32_t>(value)) % modulus;
        b = (b + a) % modulus;
    }
    return (b << 16U) | a;
}

struct StoredInflateResult {
    RasterDecodeError error{RasterDecodeError::None};
    std::vector<std::uint8_t> bytes{};
};

[[nodiscard]] StoredInflateResult inflate_stored_zlib(
    std::span<const std::uint8_t> compressed,
    std::size_t output_limit) {
    if (compressed.size() < 6U) {
        return {RasterDecodeError::MalformedContainer, {}};
    }

    const std::uint8_t cmf = compressed[0U];
    const std::uint8_t flg = compressed[1U];
    const std::uint16_t header = static_cast<std::uint16_t>(static_cast<std::uint16_t>(cmf) << 8U) |
                                 static_cast<std::uint16_t>(flg);
    if ((cmf & 0x0fU) != 8U || (cmf >> 4U) > 7U || header % 31U != 0U) {
        return {RasterDecodeError::MalformedContainer, {}};
    }
    if ((flg & 0x20U) != 0U) {
        return {RasterDecodeError::UnsupportedFeature, {}};
    }

    std::size_t byte_offset = 2U;
    unsigned bit_offset = 0U;
    bool final_block = false;
    std::vector<std::uint8_t> output;
    output.reserve(std::min(output_limit, compressed.size() * 2U));

    auto read_bits = [&](unsigned count, std::uint32_t& value) -> bool {
        value = 0U;
        for (unsigned i = 0U; i < count; ++i) {
            if (byte_offset >= compressed.size() - 4U) {
                return false;
            }
            const std::uint32_t bit = (static_cast<std::uint32_t>(compressed[byte_offset]) >> bit_offset) & 1U;
            value |= bit << i;
            ++bit_offset;
            if (bit_offset == 8U) {
                bit_offset = 0U;
                ++byte_offset;
            }
        }
        return true;
    };

    while (!final_block) {
        std::uint32_t final_value = 0U;
        std::uint32_t block_type = 0U;
        if (!read_bits(1U, final_value) || !read_bits(2U, block_type)) {
            return {RasterDecodeError::TruncatedPixelData, {}};
        }
        final_block = final_value != 0U;
        if (block_type != 0U) {
            return {RasterDecodeError::UnsupportedFeature, {}};
        }

        if (bit_offset != 0U) {
            bit_offset = 0U;
            ++byte_offset;
        }
        if (!checked_range(byte_offset, 4U, compressed.size() - 4U)) {
            return {RasterDecodeError::TruncatedPixelData, {}};
        }
        const std::uint16_t length = static_cast<std::uint16_t>(compressed[byte_offset]) |
                                     static_cast<std::uint16_t>(static_cast<std::uint16_t>(compressed[byte_offset + 1U]) << 8U);
        const std::uint16_t inverse = static_cast<std::uint16_t>(compressed[byte_offset + 2U]) |
                                      static_cast<std::uint16_t>(static_cast<std::uint16_t>(compressed[byte_offset + 3U]) << 8U);
        if (static_cast<std::uint16_t>(length ^ 0xffffU) != inverse) {
            return {RasterDecodeError::MalformedContainer, {}};
        }
        byte_offset += 4U;
        const std::size_t length_size = static_cast<std::size_t>(length);
        if (!checked_range(byte_offset, length_size, compressed.size() - 4U)) {
            return {RasterDecodeError::TruncatedPixelData, {}};
        }
        if (length_size > output_limit - output.size()) {
            return {RasterDecodeError::PixelBudgetExceeded, {}};
        }
        output.insert(
            output.end(),
            compressed.begin() + static_cast<std::ptrdiff_t>(byte_offset),
            compressed.begin() + static_cast<std::ptrdiff_t>(byte_offset + length_size));
        byte_offset += length_size;
    }

    if (bit_offset != 0U || byte_offset + 4U != compressed.size()) {
        return {RasterDecodeError::MalformedContainer, {}};
    }
    const std::uint32_t expected_adler = read_be32(compressed, byte_offset);
    if (adler32(output) != expected_adler) {
        return {RasterDecodeError::MalformedContainer, {}};
    }
    return {RasterDecodeError::None, std::move(output)};
}

[[nodiscard]] std::uint8_t paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c) noexcept {
    const int ai = static_cast<int>(a);
    const int bi = static_cast<int>(b);
    const int ci = static_cast<int>(c);
    const int prediction = ai + bi - ci;
    const int pa = prediction > ai ? prediction - ai : ai - prediction;
    const int pb = prediction > bi ? prediction - bi : bi - prediction;
    const int pc = prediction > ci ? prediction - ci : ci - prediction;
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

[[nodiscard]] bool unfilter_rows(
    std::span<const std::uint8_t> filtered,
    std::size_t width,
    std::size_t height,
    std::size_t bytes_per_pixel,
    std::vector<std::uint8_t>& pixels) {
    const std::size_t row_bytes = width * bytes_per_pixel;
    if (row_bytes > std::numeric_limits<std::size_t>::max() - 1U) {
        return false;
    }
    const std::size_t stride = row_bytes + 1U;
    if (height > std::numeric_limits<std::size_t>::max() / stride || filtered.size() != height * stride) {
        return false;
    }
    pixels.assign(height * row_bytes, 0U);

    for (std::size_t y = 0U; y < height; ++y) {
        const std::size_t source_row = y * stride;
        const std::size_t target_row = y * row_bytes;
        const std::uint8_t filter = filtered[source_row];
        if (filter > 4U) {
            return false;
        }
        for (std::size_t x = 0U; x < row_bytes; ++x) {
            const std::uint8_t raw = filtered[source_row + 1U + x];
            const std::uint8_t left = x >= bytes_per_pixel ? pixels[target_row + x - bytes_per_pixel] : 0U;
            const std::uint8_t up = y != 0U ? pixels[target_row - row_bytes + x] : 0U;
            const std::uint8_t up_left = y != 0U && x >= bytes_per_pixel
                ? pixels[target_row - row_bytes + x - bytes_per_pixel]
                : 0U;
            std::uint8_t reconstructed = raw;
            switch (filter) {
                case 0U: break;
                case 1U: reconstructed = static_cast<std::uint8_t>(static_cast<unsigned>(raw) + static_cast<unsigned>(left)); break;
                case 2U: reconstructed = static_cast<std::uint8_t>(static_cast<unsigned>(raw) + static_cast<unsigned>(up)); break;
                case 3U: {
                    const unsigned average = (static_cast<unsigned>(left) + static_cast<unsigned>(up)) / 2U;
                    reconstructed = static_cast<std::uint8_t>(static_cast<unsigned>(raw) + average);
                    break;
                }
                case 4U: reconstructed = static_cast<std::uint8_t>(static_cast<unsigned>(raw) + static_cast<unsigned>(paeth(left, up, up_left))); break;
                default: return false;
            }
            pixels[target_row + x] = reconstructed;
        }
    }
    return true;
}

}  // namespace

RasterDecodeResult decode_png(std::span<const std::uint8_t> bytes) {
    constexpr std::array<std::uint8_t, 8U> signature{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};
    if (bytes.size() < signature.size() || !std::equal(signature.begin(), signature.end(), bytes.begin())) {
        return {RasterDecodeError::MalformedContainer, {}};
    }

    std::size_t offset = signature.size();
    bool seen_ihdr = false;
    bool seen_idat = false;
    bool idat_closed = false;
    bool seen_iend = false;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint8_t color_type = 0U;
    std::size_t bytes_per_pixel = 0U;
    std::vector<std::uint8_t> idat;

    while (offset < bytes.size()) {
        if (!checked_range(offset, 12U, bytes.size())) {
            return {RasterDecodeError::MalformedContainer, {}};
        }
        const std::uint32_t length_value = read_be32(bytes, offset);
        const std::size_t length = static_cast<std::size_t>(length_value);
        const std::size_t type_offset = offset + 4U;
        const std::size_t data_offset = offset + 8U;
        if (!checked_range(data_offset, length, bytes.size()) || !checked_range(data_offset + length, 4U, bytes.size())) {
            return {RasterDecodeError::MalformedContainer, {}};
        }
        const auto type = bytes.subspan(type_offset, 4U);
        const auto data = bytes.subspan(data_offset, length);
        const std::uint32_t expected_crc = read_be32(bytes, data_offset + length);
        if (chunk_crc(type, data) != expected_crc) {
            return {RasterDecodeError::MalformedContainer, {}};
        }

        const bool is_ihdr = type[0U] == 'I' && type[1U] == 'H' && type[2U] == 'D' && type[3U] == 'R';
        const bool is_idat = type[0U] == 'I' && type[1U] == 'D' && type[2U] == 'A' && type[3U] == 'T';
        const bool is_iend = type[0U] == 'I' && type[1U] == 'E' && type[2U] == 'N' && type[3U] == 'D';

        if (!seen_ihdr && !is_ihdr) {
            return {RasterDecodeError::MalformedContainer, {}};
        }
        if (is_ihdr) {
            if (seen_ihdr || length != 13U) {
                return {RasterDecodeError::MalformedContainer, {}};
            }
            seen_ihdr = true;
            width = read_be32(data, 0U);
            height = read_be32(data, 4U);
            const std::uint8_t bit_depth = data[8U];
            color_type = data[9U];
            const std::uint8_t compression = data[10U];
            const std::uint8_t filter = data[11U];
            const std::uint8_t interlace = data[12U];
            if (width == 0U || height == 0U || width > raster_decode_max_dimension || height > raster_decode_max_dimension) {
                return {RasterDecodeError::DimensionLimitExceeded, {}};
            }
            const std::size_t width_size = static_cast<std::size_t>(width);
            const std::size_t height_size = static_cast<std::size_t>(height);
            if (width_size > raster_decode_max_pixels / height_size || width_size * height_size > raster_decode_max_pixels) {
                return {RasterDecodeError::PixelBudgetExceeded, {}};
            }
            if (bit_depth != 8U || compression != 0U || filter != 0U || interlace != 0U) {
                return {RasterDecodeError::UnsupportedFeature, {}};
            }
            switch (color_type) {
                case 0U: bytes_per_pixel = 1U; break;
                case 2U: bytes_per_pixel = 3U; break;
                case 4U: bytes_per_pixel = 2U; break;
                case 6U: bytes_per_pixel = 4U; break;
                default: return {RasterDecodeError::UnsupportedFeature, {}};
            }
        } else if (is_idat) {
            if (!seen_ihdr || idat_closed || seen_iend) {
                return {RasterDecodeError::MalformedContainer, {}};
            }
            seen_idat = true;
            if (length > raster_decode_max_pixels * 5U - idat.size()) {
                return {RasterDecodeError::PixelBudgetExceeded, {}};
            }
            idat.insert(idat.end(), data.begin(), data.end());
        } else if (is_iend) {
            if (!seen_idat || seen_iend || length != 0U) {
                return {RasterDecodeError::MalformedContainer, {}};
            }
            seen_iend = true;
            offset = data_offset + length + 4U;
            break;
        } else {
            if (seen_idat) {
                idat_closed = true;
            }
            const bool critical = (type[0U] & 0x20U) == 0U;
            if (critical) {
                return {RasterDecodeError::UnsupportedFeature, {}};
            }
        }
        offset = data_offset + length + 4U;
    }

    if (!seen_ihdr || !seen_idat || !seen_iend || offset != bytes.size()) {
        return {RasterDecodeError::MalformedContainer, {}};
    }

    const std::size_t width_size = static_cast<std::size_t>(width);
    const std::size_t height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / bytes_per_pixel) {
        return {RasterDecodeError::PixelBudgetExceeded, {}};
    }
    const std::size_t row_bytes = width_size * bytes_per_pixel;
    if (row_bytes == std::numeric_limits<std::size_t>::max() || height_size > std::numeric_limits<std::size_t>::max() / (row_bytes + 1U)) {
        return {RasterDecodeError::PixelBudgetExceeded, {}};
    }
    const std::size_t filtered_size = height_size * (row_bytes + 1U);
    const StoredInflateResult inflated = inflate_stored_zlib(idat, filtered_size);
    if (inflated.error != RasterDecodeError::None) {
        return {inflated.error, {}};
    }
    if (inflated.bytes.size() != filtered_size) {
        return {RasterDecodeError::TruncatedPixelData, {}};
    }

    std::vector<std::uint8_t> pixels;
    if (!unfilter_rows(inflated.bytes, width_size, height_size, bytes_per_pixel, pixels)) {
        return {RasterDecodeError::MalformedContainer, {}};
    }

    const std::size_t pixel_count = width_size * height_size;
    DecodedRaster decoded{};
    decoded.spec.width = width;
    decoded.spec.height = height;
    decoded.spec.layout = core::PixelLayout::RGBA;
    decoded.spec.channel_type = core::ChannelType::UInt8;
    decoded.spec.transfer = core::TransferFunction::SRGB;
    decoded.spec.primaries = core::ColorPrimaries::SRGB;
    decoded.spec.alpha = core::AlphaMode::Straight;
    decoded.rgba8.resize(pixel_count * 4U);

    for (std::size_t i = 0U; i < pixel_count; ++i) {
        const std::size_t source = i * bytes_per_pixel;
        const std::size_t target = i * 4U;
        if (color_type == 0U) {
            const std::uint8_t gray = pixels[source];
            decoded.rgba8[target] = gray;
            decoded.rgba8[target + 1U] = gray;
            decoded.rgba8[target + 2U] = gray;
            decoded.rgba8[target + 3U] = 255U;
        } else if (color_type == 2U) {
            decoded.rgba8[target] = pixels[source];
            decoded.rgba8[target + 1U] = pixels[source + 1U];
            decoded.rgba8[target + 2U] = pixels[source + 2U];
            decoded.rgba8[target + 3U] = 255U;
        } else if (color_type == 4U) {
            const std::uint8_t gray = pixels[source];
            decoded.rgba8[target] = gray;
            decoded.rgba8[target + 1U] = gray;
            decoded.rgba8[target + 2U] = gray;
            decoded.rgba8[target + 3U] = pixels[source + 1U];
        } else {
            decoded.rgba8[target] = pixels[source];
            decoded.rgba8[target + 1U] = pixels[source + 1U];
            decoded.rgba8[target + 2U] = pixels[source + 2U];
            decoded.rgba8[target + 3U] = pixels[source + 3U];
        }
    }

    return {RasterDecodeError::None, std::move(decoded)};
}

}  // namespace vektoryum::io::detail
