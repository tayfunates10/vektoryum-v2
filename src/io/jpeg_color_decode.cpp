#include "vektoryum/io/raster_decode.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace vektoryum::io::detail {

[[nodiscard]] RasterDecodeResult decode_jpeg_grayscale(std::span<const std::uint8_t> bytes);

namespace {

constexpr std::array<std::uint8_t, 64U> zigzag_to_natural{
    0U, 1U, 8U, 16U, 9U, 2U, 3U, 10U,
    17U, 24U, 32U, 25U, 18U, 11U, 4U, 5U,
    12U, 19U, 26U, 33U, 40U, 48U, 41U, 34U,
    27U, 20U, 13U, 6U, 7U, 14U, 21U, 28U,
    35U, 42U, 49U, 56U, 57U, 50U, 43U, 36U,
    29U, 22U, 15U, 23U, 30U, 37U, 44U, 51U,
    58U, 59U, 52U, 45U, 38U, 31U, 39U, 46U,
    53U, 60U, 61U, 54U, 47U, 55U, 62U, 63U,
};

constexpr std::size_t jpeg_max_pixels = 16U * 1024U * 1024U;

struct HuffmanTable {
    bool defined{false};
    std::array<std::uint16_t, 17U> count{};
    std::array<std::uint16_t, 17U> first_code{};
    std::array<std::uint16_t, 17U> first_symbol{};
    std::vector<std::uint8_t> symbols{};
};

struct QuantTable {
    bool defined{false};
    std::array<std::uint16_t, 64U> values{};
};

struct Component {
    std::uint8_t id{};
    std::uint8_t h{};
    std::uint8_t v{};
    std::uint8_t quant{};
    std::uint8_t dc{};
    std::uint8_t ac{};
};

struct Frame {
    bool defined{false};
    std::uint16_t width{};
    std::uint16_t height{};
    std::array<Component, 3U> components{};
};

[[nodiscard]] RasterDecodeResult fail(RasterDecodeError error) noexcept {
    return {error, {}};
}

[[nodiscard]] bool checked_range(std::size_t offset, std::size_t length, std::size_t size) noexcept {
    return offset <= size && length <= size - offset;
}

[[nodiscard]] std::optional<std::uint16_t> read_u16_be(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    if (!checked_range(offset, 2U, bytes.size())) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1U]));
}

[[nodiscard]] bool build_huffman_table(HuffmanTable& table) noexcept {
    std::uint32_t code = 0U;
    std::uint32_t symbol_index = 0U;
    for (std::size_t length = 1U; length <= 16U; ++length) {
        const std::uint32_t count = table.count[length];
        const std::uint32_t limit = 1U << static_cast<unsigned>(length);
        if (code + count > limit || symbol_index + count > table.symbols.size()) {
            return false;
        }
        table.first_code[length] = static_cast<std::uint16_t>(code);
        table.first_symbol[length] = static_cast<std::uint16_t>(symbol_index);
        code = (code + count) << 1U;
        symbol_index += count;
    }
    return symbol_index == table.symbols.size();
}

class EntropyReader {
public:
    EntropyReader(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept
        : bytes_(bytes), pos_(offset) {}

    [[nodiscard]] std::optional<std::uint32_t> read_bits(unsigned count) noexcept {
        if (count > 16U) {
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        for (unsigned bit = 0U; bit < count; ++bit) {
            const auto next = read_bit();
            if (!next.has_value()) {
                return std::nullopt;
            }
            value = (value << 1U) | *next;
        }
        return value;
    }

    [[nodiscard]] bool at_eoi() noexcept {
        bits_remaining_ = 0U;
        if (!checked_range(pos_, 2U, bytes_.size()) || bytes_[pos_] != 0xffU) {
            return false;
        }
        std::size_t marker_pos = pos_ + 1U;
        while (marker_pos < bytes_.size() && bytes_[marker_pos] == 0xffU) {
            ++marker_pos;
        }
        return marker_pos < bytes_.size() && bytes_[marker_pos] == 0xd9U;
    }

private:
    [[nodiscard]] std::optional<std::uint32_t> read_bit() noexcept {
        if (bits_remaining_ == 0U) {
            if (pos_ >= bytes_.size()) {
                return std::nullopt;
            }
            std::uint8_t value = bytes_[pos_++];
            if (value == 0xffU) {
                if (pos_ >= bytes_.size() || bytes_[pos_] != 0x00U) {
                    --pos_;
                    return std::nullopt;
                }
                ++pos_;
            }
            current_ = static_cast<std::uint32_t>(value);
            bits_remaining_ = 8U;
        }
        --bits_remaining_;
        return (current_ >> bits_remaining_) & 1U;
    }

    std::span<const std::uint8_t> bytes_{};
    std::size_t pos_{};
    std::uint32_t current_{};
    unsigned bits_remaining_{};
};

[[nodiscard]] std::optional<std::uint8_t> decode_huffman(
    EntropyReader& reader,
    const HuffmanTable& table) noexcept {
    if (!table.defined) {
        return std::nullopt;
    }
    std::uint32_t code = 0U;
    for (std::size_t length = 1U; length <= 16U; ++length) {
        const auto bit = reader.read_bits(1U);
        if (!bit.has_value()) {
            return std::nullopt;
        }
        code = (code << 1U) | *bit;
        const std::uint32_t first = table.first_code[length];
        const std::uint32_t count = table.count[length];
        if (code >= first && code < first + count) {
            const std::uint32_t index = static_cast<std::uint32_t>(table.first_symbol[length]) + code - first;
            if (index >= table.symbols.size()) {
                return std::nullopt;
            }
            return table.symbols[index];
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> receive_extend(EntropyReader& reader, unsigned category) noexcept {
    if (category == 0U) {
        return 0;
    }
    if (category > 11U) {
        return std::nullopt;
    }
    const auto bits = reader.read_bits(category);
    if (!bits.has_value()) {
        return std::nullopt;
    }
    const std::uint32_t threshold = 1U << (category - 1U);
    if (*bits >= threshold) {
        return static_cast<std::int32_t>(*bits);
    }
    return static_cast<std::int32_t>(*bits) - static_cast<std::int32_t>((1U << category) - 1U);
}

[[nodiscard]] const std::array<std::array<double, 8U>, 8U>& cosine_table() {
    static const auto table = [] {
        std::array<std::array<double, 8U>, 8U> result{};
        constexpr double pi = 3.141592653589793238462643383279502884;
        for (std::size_t x = 0U; x < 8U; ++x) {
            for (std::size_t u = 0U; u < 8U; ++u) {
                result[x][u] = std::cos((static_cast<double>(2U * x + 1U) * static_cast<double>(u) * pi) / 16.0);
            }
        }
        return result;
    }();
    return table;
}

[[nodiscard]] std::array<std::uint8_t, 64U> inverse_dct(
    const std::array<std::int32_t, 64U>& coefficients) {
    const auto& cosine = cosine_table();
    constexpr double inv_sqrt2 = 0.70710678118654752440;
    std::array<std::array<double, 8U>, 8U> temp{};
    for (std::size_t v = 0U; v < 8U; ++v) {
        for (std::size_t x = 0U; x < 8U; ++x) {
            double sum = 0.0;
            for (std::size_t u = 0U; u < 8U; ++u) {
                sum += (u == 0U ? inv_sqrt2 : 1.0) * static_cast<double>(coefficients[v * 8U + u]) * cosine[x][u];
            }
            temp[v][x] = sum;
        }
    }

    std::array<std::uint8_t, 64U> output{};
    for (std::size_t y = 0U; y < 8U; ++y) {
        for (std::size_t x = 0U; x < 8U; ++x) {
            double sum = 0.0;
            for (std::size_t v = 0U; v < 8U; ++v) {
                sum += (v == 0U ? inv_sqrt2 : 1.0) * temp[v][x] * cosine[y][v];
            }
            output[y * 8U + x] = static_cast<std::uint8_t>(
                std::clamp(std::lround(sum * 0.25 + 128.0), 0L, 255L));
        }
    }
    return output;
}

[[nodiscard]] bool parse_dqt(
    std::span<const std::uint8_t> payload,
    std::array<QuantTable, 4U>& tables) noexcept {
    std::size_t pos = 0U;
    while (pos < payload.size()) {
        const std::uint8_t info = payload[pos++];
        const std::uint8_t precision = static_cast<std::uint8_t>(info >> 4U);
        const std::uint8_t id = static_cast<std::uint8_t>(info & 0x0fU);
        if (precision != 0U || id >= tables.size() || !checked_range(pos, 64U, payload.size())) {
            return false;
        }
        QuantTable table{};
        table.defined = true;
        for (std::size_t i = 0U; i < 64U; ++i) {
            const std::uint8_t value = payload[pos + i];
            if (value == 0U) {
                return false;
            }
            table.values[zigzag_to_natural[i]] = value;
        }
        pos += 64U;
        tables[id] = table;
    }
    return pos == payload.size();
}

[[nodiscard]] bool parse_dht(
    std::span<const std::uint8_t> payload,
    std::array<HuffmanTable, 4U>& dc_tables,
    std::array<HuffmanTable, 4U>& ac_tables) {
    std::size_t pos = 0U;
    while (pos < payload.size()) {
        if (!checked_range(pos, 17U, payload.size())) {
            return false;
        }
        const std::uint8_t info = payload[pos++];
        const std::uint8_t table_class = static_cast<std::uint8_t>(info >> 4U);
        const std::uint8_t id = static_cast<std::uint8_t>(info & 0x0fU);
        if (table_class > 1U || id >= 4U) {
            return false;
        }
        HuffmanTable table{};
        std::size_t symbol_count = 0U;
        for (std::size_t length = 1U; length <= 16U; ++length) {
            table.count[length] = payload[pos++];
            symbol_count += table.count[length];
        }
        if (symbol_count == 0U || symbol_count > 256U || !checked_range(pos, symbol_count, payload.size())) {
            return false;
        }
        table.symbols.assign(payload.begin() + static_cast<std::ptrdiff_t>(pos),
                             payload.begin() + static_cast<std::ptrdiff_t>(pos + symbol_count));
        pos += symbol_count;
        table.defined = build_huffman_table(table);
        if (!table.defined) {
            return false;
        }
        if (table_class == 0U) {
            dc_tables[id] = std::move(table);
        } else {
            ac_tables[id] = std::move(table);
        }
    }
    return pos == payload.size();
}

[[nodiscard]] std::optional<std::array<std::uint8_t, 64U>> decode_block(
    EntropyReader& reader,
    std::int32_t& previous_dc,
    const QuantTable& quant,
    const HuffmanTable& dc,
    const HuffmanTable& ac) {
    std::array<std::int32_t, 64U> coefficients{};
    const auto dc_symbol = decode_huffman(reader, dc);
    if (!dc_symbol.has_value() || *dc_symbol > 11U) {
        return std::nullopt;
    }
    const auto dc_delta = receive_extend(reader, static_cast<unsigned>(*dc_symbol));
    if (!dc_delta.has_value()) {
        return std::nullopt;
    }
    previous_dc += *dc_delta;
    coefficients[0U] = previous_dc * static_cast<std::int32_t>(quant.values[0U]);

    std::size_t k = 1U;
    while (k < 64U) {
        const auto symbol = decode_huffman(reader, ac);
        if (!symbol.has_value()) {
            return std::nullopt;
        }
        if (*symbol == 0U) {
            break;
        }
        const unsigned run = static_cast<unsigned>(*symbol >> 4U);
        const unsigned category = static_cast<unsigned>(*symbol & 0x0fU);
        if (category == 0U) {
            if (run != 15U || k + 16U > 64U) {
                return std::nullopt;
            }
            k += 16U;
            continue;
        }
        if (category > 10U || k + run >= 64U) {
            return std::nullopt;
        }
        k += run;
        const auto value = receive_extend(reader, category);
        if (!value.has_value()) {
            return std::nullopt;
        }
        const std::size_t natural = zigzag_to_natural[k];
        coefficients[natural] = *value * static_cast<std::int32_t>(quant.values[natural]);
        ++k;
    }
    return inverse_dct(coefficients);
}

[[nodiscard]] std::uint8_t clamp_channel(double value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

[[nodiscard]] RasterDecodeResult decode_color_scan(
    std::span<const std::uint8_t> bytes,
    std::size_t entropy_offset,
    const Frame& frame,
    const std::array<QuantTable, 4U>& quant_tables,
    const std::array<HuffmanTable, 4U>& dc_tables,
    const std::array<HuffmanTable, 4U>& ac_tables) {
    const std::size_t width = frame.width;
    const std::size_t height = frame.height;
    if (width == 0U || height == 0U || width > raster_decode_max_dimension || height > raster_decode_max_dimension) {
        return fail(RasterDecodeError::DimensionLimitExceeded);
    }
    if (width > jpeg_max_pixels / height || width * height > raster_decode_max_pixels) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }

    const std::uint8_t max_h = std::max({frame.components[0U].h, frame.components[1U].h, frame.components[2U].h});
    const std::uint8_t max_v = std::max({frame.components[0U].v, frame.components[1U].v, frame.components[2U].v});
    const bool is_444 = max_h == 1U && max_v == 1U &&
                        frame.components[0U].h == 1U && frame.components[0U].v == 1U &&
                        frame.components[1U].h == 1U && frame.components[1U].v == 1U &&
                        frame.components[2U].h == 1U && frame.components[2U].v == 1U;
    const bool is_420 = frame.components[0U].h == 2U && frame.components[0U].v == 2U &&
                        frame.components[1U].h == 1U && frame.components[1U].v == 1U &&
                        frame.components[2U].h == 1U && frame.components[2U].v == 1U;
    if (!is_444 && !is_420) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }

    for (const Component& component : frame.components) {
        if (component.quant >= quant_tables.size() || component.dc >= dc_tables.size() || component.ac >= ac_tables.size() ||
            !quant_tables[component.quant].defined || !dc_tables[component.dc].defined || !ac_tables[component.ac].defined) {
            return fail(RasterDecodeError::MalformedContainer);
        }
    }

    const std::size_t pixel_count = width * height;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4U) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }
    DecodedRaster decoded{};
    decoded.spec.width = frame.width;
    decoded.spec.height = frame.height;
    decoded.spec.layout = core::PixelLayout::RGBA;
    decoded.spec.channel_type = core::ChannelType::UInt8;
    decoded.spec.transfer = core::TransferFunction::SRGB;
    decoded.spec.primaries = core::ColorPrimaries::SRGB;
    decoded.spec.alpha = core::AlphaMode::Straight;
    decoded.rgba8.resize(pixel_count * 4U);

    EntropyReader reader(bytes, entropy_offset);
    std::array<std::int32_t, 3U> previous_dc{};
    const std::size_t mcu_width = static_cast<std::size_t>(max_h) * 8U;
    const std::size_t mcu_height = static_cast<std::size_t>(max_v) * 8U;
    const std::size_t mcus_x = (width + mcu_width - 1U) / mcu_width;
    const std::size_t mcus_y = (height + mcu_height - 1U) / mcu_height;

    for (std::size_t mcu_y = 0U; mcu_y < mcus_y; ++mcu_y) {
        for (std::size_t mcu_x = 0U; mcu_x < mcus_x; ++mcu_x) {
            std::array<std::array<std::array<std::uint8_t, 64U>, 4U>, 3U> blocks{};
            for (std::size_t component_index = 0U; component_index < 3U; ++component_index) {
                const Component& component = frame.components[component_index];
                const std::size_t block_count = static_cast<std::size_t>(component.h) * component.v;
                for (std::size_t block_index = 0U; block_index < block_count; ++block_index) {
                    const auto block = decode_block(reader, previous_dc[component_index],
                                                    quant_tables[component.quant],
                                                    dc_tables[component.dc], ac_tables[component.ac]);
                    if (!block.has_value()) {
                        return fail(RasterDecodeError::TruncatedPixelData);
                    }
                    blocks[component_index][block_index] = *block;
                }
            }

            for (std::size_t local_y = 0U; local_y < mcu_height; ++local_y) {
                const std::size_t y = mcu_y * mcu_height + local_y;
                if (y >= height) {
                    break;
                }
                for (std::size_t local_x = 0U; local_x < mcu_width; ++local_x) {
                    const std::size_t x = mcu_x * mcu_width + local_x;
                    if (x >= width) {
                        break;
                    }

                    std::array<std::uint8_t, 3U> sample{};
                    for (std::size_t component_index = 0U; component_index < 3U; ++component_index) {
                        const Component& component = frame.components[component_index];
                        const std::size_t sample_x = local_x * component.h / max_h;
                        const std::size_t sample_y = local_y * component.v / max_v;
                        const std::size_t block_x = sample_x / 8U;
                        const std::size_t block_y = sample_y / 8U;
                        const std::size_t block_index = block_y * component.h + block_x;
                        sample[component_index] = blocks[component_index][block_index][(sample_y % 8U) * 8U + (sample_x % 8U)];
                    }

                    const double y_value = static_cast<double>(sample[0U]);
                    const double cb = static_cast<double>(sample[1U]) - 128.0;
                    const double cr = static_cast<double>(sample[2U]) - 128.0;
                    const std::size_t target = (y * width + x) * 4U;
                    decoded.rgba8[target] = clamp_channel(y_value + 1.402 * cr);
                    decoded.rgba8[target + 1U] = clamp_channel(y_value - 0.344136 * cb - 0.714136 * cr);
                    decoded.rgba8[target + 2U] = clamp_channel(y_value + 1.772 * cb);
                    decoded.rgba8[target + 3U] = 255U;
                }
            }
        }
    }

    if (!reader.at_eoi()) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    return {RasterDecodeError::None, std::move(decoded)};
}

}  // namespace

RasterDecodeResult decode_jpeg(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 4U || bytes[0U] != 0xffU || bytes[1U] != 0xd8U) {
        return fail(RasterDecodeError::MalformedContainer);
    }

    std::array<QuantTable, 4U> quant_tables{};
    std::array<HuffmanTable, 4U> dc_tables{};
    std::array<HuffmanTable, 4U> ac_tables{};
    Frame frame{};
    std::size_t pos = 2U;
    bool saw_three_component_frame = false;

    while (pos < bytes.size()) {
        if (bytes[pos] != 0xffU) {
            return saw_three_component_frame ? fail(RasterDecodeError::MalformedContainer) : decode_jpeg_grayscale(bytes);
        }
        while (pos < bytes.size() && bytes[pos] == 0xffU) {
            ++pos;
        }
        if (pos >= bytes.size()) {
            return saw_three_component_frame ? fail(RasterDecodeError::TruncatedPixelData) : decode_jpeg_grayscale(bytes);
        }
        const std::uint8_t marker = bytes[pos++];
        if (marker == 0xd9U) {
            return saw_three_component_frame ? fail(RasterDecodeError::MalformedContainer) : decode_jpeg_grayscale(bytes);
        }
        if (marker == 0xd8U || marker == 0x01U || (marker >= 0xd0U && marker <= 0xd7U)) {
            return saw_three_component_frame ? fail(RasterDecodeError::MalformedContainer) : decode_jpeg_grayscale(bytes);
        }
        const auto length_value = read_u16_be(bytes, pos);
        if (!length_value.has_value() || *length_value < 2U) {
            return saw_three_component_frame ? fail(RasterDecodeError::MalformedContainer) : decode_jpeg_grayscale(bytes);
        }
        const std::size_t payload_size = static_cast<std::size_t>(*length_value) - 2U;
        pos += 2U;
        if (!checked_range(pos, payload_size, bytes.size())) {
            return saw_three_component_frame ? fail(RasterDecodeError::TruncatedPixelData) : decode_jpeg_grayscale(bytes);
        }
        const auto payload = bytes.subspan(pos, payload_size);

        if (marker == 0xdbU) {
            if (!parse_dqt(payload, quant_tables)) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
        } else if (marker == 0xc4U) {
            if (!parse_dht(payload, dc_tables, ac_tables)) {
                return fail(RasterDecodeError::MalformedContainer);
            }
        } else if (marker == 0xc0U) {
            if (payload.size() >= 6U && payload[0U] == 8U && payload[5U] == 1U) {
                return decode_jpeg_grayscale(bytes);
            }
            if (frame.defined || payload.size() != 15U || payload[0U] != 8U || payload[5U] != 3U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            const auto height = read_u16_be(payload, 1U);
            const auto width = read_u16_be(payload, 3U);
            if (!height.has_value() || !width.has_value() || *height == 0U || *width == 0U) {
                return fail(RasterDecodeError::MalformedContainer);
            }
            frame.defined = true;
            frame.width = *width;
            frame.height = *height;
            saw_three_component_frame = true;
            for (std::size_t i = 0U; i < 3U; ++i) {
                const std::size_t offset = 6U + i * 3U;
                const std::uint8_t sampling = payload[offset + 1U];
                frame.components[i] = Component{
                    payload[offset],
                    static_cast<std::uint8_t>(sampling >> 4U),
                    static_cast<std::uint8_t>(sampling & 0x0fU),
                    payload[offset + 2U],
                    0U,
                    0U,
                };
                if (frame.components[i].h == 0U || frame.components[i].v == 0U || frame.components[i].h > 2U ||
                    frame.components[i].v > 2U || frame.components[i].quant >= quant_tables.size()) {
                    return fail(RasterDecodeError::UnsupportedFeature);
                }
            }
        } else if ((marker >= 0xc1U && marker <= 0xcfU) && marker != 0xc4U && marker != 0xc8U && marker != 0xccU) {
            return fail(RasterDecodeError::UnsupportedFeature);
        } else if (marker == 0xddU) {
            if (payload.size() != 2U) {
                return fail(RasterDecodeError::MalformedContainer);
            }
            const auto interval = read_u16_be(payload, 0U);
            if (!interval.has_value() || *interval != 0U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
        } else if (marker == 0xdaU) {
            if (!saw_three_component_frame) {
                return decode_jpeg_grayscale(bytes);
            }
            if (!frame.defined || payload.size() != 10U || payload[0U] != 3U ||
                payload[7U] != 0U || payload[8U] != 63U || payload[9U] != 0U) {
                return fail(RasterDecodeError::UnsupportedFeature);
            }
            std::array<bool, 3U> seen{};
            for (std::size_t scan_index = 0U; scan_index < 3U; ++scan_index) {
                const std::uint8_t id = payload[1U + scan_index * 2U];
                std::size_t component_index = 3U;
                for (std::size_t i = 0U; i < 3U; ++i) {
                    if (frame.components[i].id == id) {
                        component_index = i;
                        break;
                    }
                }
                if (component_index >= 3U || seen[component_index]) {
                    return fail(RasterDecodeError::MalformedContainer);
                }
                seen[component_index] = true;
                const std::uint8_t selector = payload[2U + scan_index * 2U];
                frame.components[component_index].dc = static_cast<std::uint8_t>(selector >> 4U);
                frame.components[component_index].ac = static_cast<std::uint8_t>(selector & 0x0fU);
            }
            return decode_color_scan(bytes, pos + payload_size, frame, quant_tables, dc_tables, ac_tables);
        }

        pos += payload_size;
    }

    return saw_three_component_frame ? fail(RasterDecodeError::TruncatedPixelData) : decode_jpeg_grayscale(bytes);
}

}  // namespace vektoryum::io::detail
