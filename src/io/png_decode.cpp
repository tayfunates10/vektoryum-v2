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

struct InflateResult {
    RasterDecodeError error{RasterDecodeError::None};
    std::vector<std::uint8_t> bytes{};
};

class DeflateBitReader {
public:
    DeflateBitReader(std::span<const std::uint8_t> bytes, std::size_t begin, std::size_t end) noexcept
        : bytes_(bytes), byte_offset_(begin), end_(end) {}

    [[nodiscard]] bool read_bits(unsigned count, std::uint32_t& value) noexcept {
        value = 0U;
        for (unsigned i = 0U; i < count; ++i) {
            if (byte_offset_ >= end_) {
                return false;
            }
            const std::uint32_t bit =
                (static_cast<std::uint32_t>(bytes_[byte_offset_]) >> bit_offset_) & 1U;
            value |= bit << i;
            ++bit_offset_;
            if (bit_offset_ == 8U) {
                bit_offset_ = 0U;
                ++byte_offset_;
            }
        }
        return true;
    }

    [[nodiscard]] bool align_to_byte() noexcept {
        if (bit_offset_ == 0U) {
            return true;
        }
        bit_offset_ = 0U;
        ++byte_offset_;
        return byte_offset_ <= end_;
    }

    [[nodiscard]] bool align_zero_padding() noexcept {
        if (bit_offset_ == 0U) {
            return true;
        }
        if (byte_offset_ >= end_) {
            return false;
        }
        const std::uint8_t mask = static_cast<std::uint8_t>(0xffU << bit_offset_);
        if ((bytes_[byte_offset_] & mask) != 0U) {
            return false;
        }
        bit_offset_ = 0U;
        ++byte_offset_;
        return byte_offset_ <= end_;
    }

    [[nodiscard]] bool read_u16_le(std::uint16_t& value) noexcept {
        if (bit_offset_ != 0U || !checked_range(byte_offset_, 2U, end_)) {
            return false;
        }
        value = static_cast<std::uint16_t>(bytes_[byte_offset_]) |
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes_[byte_offset_ + 1U]) << 8U);
        byte_offset_ += 2U;
        return true;
    }

    [[nodiscard]] bool read_bytes(std::size_t length, std::vector<std::uint8_t>& output, std::size_t output_limit) {
        if (bit_offset_ != 0U || !checked_range(byte_offset_, length, end_)) {
            return false;
        }
        if (length > output_limit - output.size()) {
            return false;
        }
        output.insert(
            output.end(),
            bytes_.begin() + static_cast<std::ptrdiff_t>(byte_offset_),
            bytes_.begin() + static_cast<std::ptrdiff_t>(byte_offset_ + length));
        byte_offset_ += length;
        return true;
    }

    [[nodiscard]] bool at_end() const noexcept {
        return bit_offset_ == 0U && byte_offset_ == end_;
    }

private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t byte_offset_{};
    std::size_t end_{};
    unsigned bit_offset_{};
};

struct DeflateHuffman {
    std::array<std::uint16_t, 288U> reversed_codes{};
    std::array<std::uint8_t, 288U> lengths{};
    std::size_t symbol_count{};
    unsigned max_bits{};
};

[[nodiscard]] std::uint16_t reverse_bits(std::uint16_t code, unsigned length) noexcept {
    std::uint16_t reversed = 0U;
    for (unsigned i = 0U; i < length; ++i) {
        reversed = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(reversed << 1U) |
            static_cast<std::uint16_t>((code >> i) & 1U));
    }
    return reversed;
}

[[nodiscard]] bool build_huffman(std::span<const std::uint8_t> lengths, DeflateHuffman& table) noexcept {
    if (lengths.empty() || lengths.size() > table.lengths.size()) {
        return false;
    }
    std::array<std::uint16_t, 16U> count{};
    std::array<std::uint16_t, 16U> next_code{};
    for (const std::uint8_t length : lengths) {
        if (length > 15U) {
            return false;
        }
        if (length != 0U) {
            ++count[length];
            table.max_bits = std::max(table.max_bits, static_cast<unsigned>(length));
        }
    }
    if (table.max_bits == 0U) {
        return false;
    }

    std::uint32_t code = 0U;
    for (unsigned bits = 1U; bits <= 15U; ++bits) {
        code = (code + static_cast<std::uint32_t>(count[bits - 1U])) << 1U;
        const std::uint32_t limit = 1U << bits;
        if (code + static_cast<std::uint32_t>(count[bits]) > limit) {
            return false;
        }
        next_code[bits] = static_cast<std::uint16_t>(code);
    }

    table.symbol_count = lengths.size();
    for (std::size_t symbol = 0U; symbol < lengths.size(); ++symbol) {
        const std::uint8_t length = lengths[symbol];
        table.lengths[symbol] = length;
        if (length != 0U) {
            const std::uint16_t canonical = next_code[length]++;
            table.reversed_codes[symbol] = reverse_bits(canonical, static_cast<unsigned>(length));
        }
    }
    return true;
}

[[nodiscard]] bool decode_symbol(
    DeflateBitReader& reader,
    const DeflateHuffman& table,
    std::uint16_t& symbol) noexcept {
    std::uint32_t code = 0U;
    for (unsigned length = 1U; length <= table.max_bits; ++length) {
        std::uint32_t bit = 0U;
        if (!reader.read_bits(1U, bit)) {
            return false;
        }
        code |= bit << (length - 1U);
        for (std::size_t candidate = 0U; candidate < table.symbol_count; ++candidate) {
            if (table.lengths[candidate] == length && table.reversed_codes[candidate] == code) {
                symbol = static_cast<std::uint16_t>(candidate);
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] DeflateHuffman fixed_literal_table() noexcept {
    std::array<std::uint8_t, 288U> lengths{};
    for (std::size_t i = 0U; i <= 143U; ++i) {
        lengths[i] = 8U;
    }
    for (std::size_t i = 144U; i <= 255U; ++i) {
        lengths[i] = 9U;
    }
    for (std::size_t i = 256U; i <= 279U; ++i) {
        lengths[i] = 7U;
    }
    for (std::size_t i = 280U; i <= 287U; ++i) {
        lengths[i] = 8U;
    }
    DeflateHuffman table{};
    (void)build_huffman(lengths, table);
    return table;
}

[[nodiscard]] DeflateHuffman fixed_distance_table() noexcept {
    std::array<std::uint8_t, 32U> lengths{};
    lengths.fill(5U);
    DeflateHuffman table{};
    (void)build_huffman(lengths, table);
    return table;
}

[[nodiscard]] RasterDecodeError decode_fixed_block(
    DeflateBitReader& reader,
    std::vector<std::uint8_t>& output,
    std::size_t output_limit) {
    static const DeflateHuffman literals = fixed_literal_table();
    static const DeflateHuffman distances = fixed_distance_table();
    constexpr std::array<std::uint16_t, 29U> length_base{
        3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U, 15U, 17U, 19U, 23U, 27U,
        31U, 35U, 43U, 51U, 59U, 67U, 83U, 99U, 115U, 131U, 163U, 195U, 227U, 258U};
    constexpr std::array<std::uint8_t, 29U> length_extra{
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U, 1U, 1U, 2U, 2U, 2U,
        2U, 3U, 3U, 3U, 3U, 4U, 4U, 4U, 4U, 5U, 5U, 5U, 5U, 0U};
    constexpr std::array<std::uint16_t, 30U> distance_base{
        1U, 2U, 3U, 4U, 5U, 7U, 9U, 13U, 17U, 25U, 33U, 49U, 65U, 97U, 129U,
        193U, 257U, 385U, 513U, 769U, 1025U, 1537U, 2049U, 3073U, 4097U, 6145U,
        8193U, 12289U, 16385U, 24577U};
    constexpr std::array<std::uint8_t, 30U> distance_extra{
        0U, 0U, 0U, 0U, 1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 5U, 6U,
        6U, 7U, 7U, 8U, 8U, 9U, 9U, 10U, 10U, 11U, 11U, 12U, 12U, 13U, 13U};

    for (;;) {
        std::uint16_t symbol = 0U;
        if (!decode_symbol(reader, literals, symbol)) {
            return RasterDecodeError::TruncatedPixelData;
        }
        if (symbol < 256U) {
            if (output.size() >= output_limit) {
                return RasterDecodeError::PixelBudgetExceeded;
            }
            output.push_back(static_cast<std::uint8_t>(symbol));
            continue;
        }
        if (symbol == 256U) {
            return RasterDecodeError::None;
        }
        if (symbol < 257U || symbol > 285U) {
            return RasterDecodeError::MalformedContainer;
        }

        const std::size_t length_index = static_cast<std::size_t>(symbol - 257U);
        std::uint32_t extra_length = 0U;
        if (!reader.read_bits(length_extra[length_index], extra_length)) {
            return RasterDecodeError::TruncatedPixelData;
        }
        const std::size_t match_length =
            static_cast<std::size_t>(length_base[length_index]) + static_cast<std::size_t>(extra_length);

        std::uint16_t distance_symbol = 0U;
        if (!decode_symbol(reader, distances, distance_symbol)) {
            return RasterDecodeError::TruncatedPixelData;
        }
        if (distance_symbol >= distance_base.size()) {
            return RasterDecodeError::MalformedContainer;
        }
        std::uint32_t extra_distance = 0U;
        const std::size_t distance_index = static_cast<std::size_t>(distance_symbol);
        if (!reader.read_bits(distance_extra[distance_index], extra_distance)) {
            return RasterDecodeError::TruncatedPixelData;
        }
        const std::size_t distance =
            static_cast<std::size_t>(distance_base[distance_index]) + static_cast<std::size_t>(extra_distance);
        if (distance == 0U || distance > output.size()) {
            return RasterDecodeError::MalformedContainer;
        }
        if (match_length > output_limit - output.size()) {
            return RasterDecodeError::PixelBudgetExceeded;
        }
        for (std::size_t i = 0U; i < match_length; ++i) {
            output.push_back(output[output.size() - distance]);
        }
    }
}

[[nodiscard]] InflateResult inflate_zlib(
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

    const std::size_t deflate_end = compressed.size() - 4U;
    DeflateBitReader reader(compressed, 2U, deflate_end);
    bool final_block = false;
    std::vector<std::uint8_t> output;
    output.reserve(std::min(output_limit, compressed.size() * 2U));

    while (!final_block) {
        std::uint32_t final_value = 0U;
        std::uint32_t block_type = 0U;
        if (!reader.read_bits(1U, final_value) || !reader.read_bits(2U, block_type)) {
            return {RasterDecodeError::TruncatedPixelData, {}};
        }
        final_block = final_value != 0U;

        if (block_type == 0U) {
            if (!reader.align_to_byte()) {
                return {RasterDecodeError::TruncatedPixelData, {}};
            }
            std::uint16_t length = 0U;
            std::uint16_t inverse = 0U;
            if (!reader.read_u16_le(length) || !reader.read_u16_le(inverse)) {
                return {RasterDecodeError::TruncatedPixelData, {}};
            }
            if (static_cast<std::uint16_t>(length ^ 0xffffU) != inverse) {
                return {RasterDecodeError::MalformedContainer, {}};
            }
            const std::size_t length_size = static_cast<std::size_t>(length);
            if (length_size > output_limit - output.size()) {
                return {RasterDecodeError::PixelBudgetExceeded, {}};
            }
            if (!reader.read_bytes(length_size, output, output_limit)) {
                return {RasterDecodeError::TruncatedPixelData, {}};
            }
        } else if (block_type == 1U) {
            const RasterDecodeError error = decode_fixed_block(reader, output, output_limit);
            if (error != RasterDecodeError::None) {
                return {error, {}};
            }
        } else if (block_type == 2U) {
            return {RasterDecodeError::UnsupportedFeature, {}};
        } else {
            return {RasterDecodeError::MalformedContainer, {}};
        }
    }

    if (!reader.align_zero_padding() || !reader.at_end()) {
        return {RasterDecodeError::MalformedContainer, {}};
    }
    const std::uint32_t expected_adler = read_be32(compressed, deflate_end);
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
    const InflateResult inflated = inflate_zlib(idat, filtered_size);
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
