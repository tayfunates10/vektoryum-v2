#include "vektoryum/io/raster_decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace vektoryum::io::detail {
namespace {

// VP8L (WebP lossless) bitstream constants.
constexpr std::uint32_t literal_codes = 256U;
constexpr std::uint32_t length_codes = 24U;
constexpr std::uint32_t distance_codes = 40U;
constexpr std::uint32_t code_length_codes = 19U;
constexpr std::uint32_t max_code_length = 15U;
constexpr std::uint32_t max_cache_bits = 11U;
constexpr std::uint32_t plane_codes = 120U;
constexpr std::uint32_t predictor_modes = 14U;
constexpr std::uint8_t vp8l_signature = 0x2fU;
constexpr std::uint32_t opaque_black = 0xff000000U;

enum class TransformType : std::uint32_t {
    Predictor = 0U,
    CrossColor = 1U,
    SubtractGreen = 2U,
    ColorIndexing = 3U,
};

// A VP8L image stream nests at most one level: the top-level image owns
// transform data, an entropy image and a palette, and none of those may nest
// further.
constexpr unsigned max_stream_depth = 1U;

[[nodiscard]] std::uint32_t subsample_size(std::uint32_t size, std::uint32_t bits) noexcept {
    return (size + (1U << bits) - 1U) >> bits;
}

// Reads the bitstream least-significant-bit first. A read past the end marks
// the reader failed and yields zero, so a caller can batch several reads and
// check once instead of threading an error through every expression.
class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes), total_bits_(bytes.size() * 8U) {}

    [[nodiscard]] std::uint32_t read(std::uint32_t count) noexcept {
        std::uint32_t value = 0U;
        for (std::uint32_t i = 0U; i < count; ++i) {
            if (failed_ || bit_offset_ >= total_bits_) {
                failed_ = true;
                return 0U;
            }
            const std::size_t index = bit_offset_ >> 3U;
            const auto shift = static_cast<std::uint32_t>(bit_offset_ & 7U);
            const std::uint32_t bit = (static_cast<std::uint32_t>(bytes_[index]) >> shift) & 1U;
            value |= bit << i;
            ++bit_offset_;
        }
        return value;
    }

    [[nodiscard]] bool failed() const noexcept {
        return failed_;
    }

    [[nodiscard]] std::size_t remaining_bits() const noexcept {
        return bit_offset_ >= total_bits_ ? 0U : total_bits_ - bit_offset_;
    }

    void fail() noexcept {
        failed_ = true;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t total_bits_{};
    std::size_t bit_offset_{0U};
    bool failed_{false};
};

// Canonical prefix code. VP8L stores codes bit-reversed, exactly like DEFLATE,
// so a symbol is decoded by extending the code one bit at a time from the least
// significant end.
class PrefixCode {
public:
    [[nodiscard]] bool build(std::span<const std::uint8_t> lengths) {
        counts_.fill(0U);
        symbols_.clear();
        single_ = false;
        single_symbol_ = 0U;

        std::uint32_t used = 0U;
        std::uint32_t last = 0U;
        for (std::size_t symbol = 0U; symbol < lengths.size(); ++symbol) {
            const std::uint8_t length = lengths[symbol];
            if (length > max_code_length) {
                return false;
            }
            if (length != 0U) {
                ++counts_[length];
                ++used;
                last = static_cast<std::uint32_t>(symbol);
            }
        }
        if (used == 0U) {
            return false;
        }
        if (used == 1U) {
            set_single(last);
            return true;
        }

        // Reject both over- and under-subscribed codes.
        std::int32_t left = 1;
        for (std::uint32_t length = 1U; length <= max_code_length; ++length) {
            left <<= 1;
            left -= static_cast<std::int32_t>(counts_[length]);
            if (left < 0) {
                return false;
            }
        }
        if (left != 0) {
            return false;
        }

        std::array<std::uint32_t, max_code_length + 1U> offsets{};
        std::uint32_t running = 0U;
        for (std::uint32_t length = 1U; length <= max_code_length; ++length) {
            offsets[length] = running;
            running += counts_[length];
        }
        symbols_.assign(running, 0U);
        for (std::size_t symbol = 0U; symbol < lengths.size(); ++symbol) {
            const std::uint8_t length = lengths[symbol];
            if (length != 0U) {
                symbols_[offsets[length]++] = static_cast<std::uint16_t>(symbol);
            }
        }
        return true;
    }

    void set_single(std::uint32_t symbol) noexcept {
        counts_.fill(0U);
        symbols_.clear();
        single_ = true;
        single_symbol_ = symbol;
    }

    // A single-symbol code consumes no bits at all.
    [[nodiscard]] std::uint32_t read(BitReader& reader) const noexcept {
        if (single_) {
            return single_symbol_;
        }
        std::uint32_t code = 0U;
        std::uint32_t first = 0U;
        std::uint32_t index = 0U;
        for (std::uint32_t length = 1U; length <= max_code_length; ++length) {
            code |= reader.read(1U);
            if (reader.failed()) {
                return 0U;
            }
            const std::uint32_t count = counts_[length];
            if (code - first < count) {
                return symbols_[index + (code - first)];
            }
            index += count;
            first = (first + count) << 1U;
            code <<= 1U;
        }
        reader.fail();
        return 0U;
    }

private:
    std::array<std::uint32_t, max_code_length + 1U> counts_{};
    std::vector<std::uint16_t> symbols_{};
    std::uint32_t single_symbol_{0U};
    bool single_{false};
};

constexpr std::array<std::uint8_t, code_length_codes> code_length_order{
    17U, 18U, 0U, 1U, 2U, 3U, 4U, 5U, 16U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U};

[[nodiscard]] bool read_prefix_code(BitReader& reader, std::uint32_t alphabet_size, PrefixCode& code) {
    if (reader.read(1U) != 0U) {
        // Simple code: one or two symbols listed literally.
        const std::uint32_t symbol_count = reader.read(1U) + 1U;
        const std::uint32_t first_bits = reader.read(1U) == 0U ? 1U : 8U;
        std::array<std::uint32_t, 2U> symbols{};
        symbols[0U] = reader.read(first_bits);
        if (symbol_count == 2U) {
            symbols[1U] = reader.read(8U);
        }
        if (reader.failed()) {
            return false;
        }
        for (std::uint32_t i = 0U; i < symbol_count; ++i) {
            if (symbols[i] >= alphabet_size) {
                return false;
            }
        }
        if (symbol_count == 1U) {
            code.set_single(symbols[0U]);
            return true;
        }
        if (symbols[0U] == symbols[1U]) {
            return false;
        }
        std::vector<std::uint8_t> lengths(alphabet_size, 0U);
        lengths[symbols[0U]] = 1U;
        lengths[symbols[1U]] = 1U;
        return code.build(lengths);
    }

    std::array<std::uint8_t, code_length_codes> meta_lengths{};
    const std::uint32_t present = reader.read(4U) + 4U;
    if (reader.failed() || present > code_length_codes) {
        return false;
    }
    for (std::uint32_t i = 0U; i < present; ++i) {
        meta_lengths[code_length_order[i]] = static_cast<std::uint8_t>(reader.read(3U));
    }
    if (reader.failed()) {
        return false;
    }
    PrefixCode meta_code;
    if (!meta_code.build(meta_lengths)) {
        return false;
    }

    std::uint32_t max_symbol = alphabet_size;
    if (reader.read(1U) != 0U) {
        const std::uint32_t length_bits = 2U + 2U * reader.read(3U);
        max_symbol = 2U + reader.read(length_bits);
        if (reader.failed() || max_symbol > alphabet_size) {
            return false;
        }
    }
    if (reader.failed()) {
        return false;
    }

    constexpr std::array<std::uint32_t, 3U> repeat_bits{2U, 3U, 7U};
    constexpr std::array<std::uint32_t, 3U> repeat_offsets{3U, 3U, 11U};
    std::vector<std::uint8_t> lengths(alphabet_size, 0U);
    std::uint32_t previous = 8U;
    std::uint32_t symbol = 0U;
    while (symbol < alphabet_size && max_symbol != 0U) {
        --max_symbol;
        const std::uint32_t value = meta_code.read(reader);
        if (reader.failed()) {
            return false;
        }
        if (value < 16U) {
            lengths[symbol++] = static_cast<std::uint8_t>(value);
            if (value != 0U) {
                previous = value;
            }
            continue;
        }
        const std::uint32_t slot = value - 16U;
        if (slot >= repeat_bits.size()) {
            return false;
        }
        const std::uint32_t repeat = reader.read(repeat_bits[slot]) + repeat_offsets[slot];
        if (reader.failed() || repeat > alphabet_size - symbol) {
            return false;
        }
        const auto filler = static_cast<std::uint8_t>(slot == 0U ? previous : 0U);
        for (std::uint32_t i = 0U; i < repeat; ++i) {
            lengths[symbol++] = filler;
        }
    }
    return code.build(lengths);
}

// Hash-indexed cache of recently emitted pixels, addressed by the encoder
// through dedicated green-channel symbols.
class ColorCache {
public:
    void reset(std::uint32_t bits) {
        bits_ = bits;
        entries_.assign(std::size_t{1U} << bits, 0U);
    }

    [[nodiscard]] bool active() const noexcept {
        return bits_ != 0U;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return entries_.size();
    }

    void insert(std::uint32_t argb) noexcept {
        entries_[key(argb)] = argb;
    }

    [[nodiscard]] std::uint32_t lookup(std::uint32_t index) const noexcept {
        return entries_[index];
    }

private:
    [[nodiscard]] std::size_t key(std::uint32_t argb) const noexcept {
        constexpr std::uint32_t hash_multiplier = 0x1e35a7bdU;
        return static_cast<std::size_t>((argb * hash_multiplier) >> (32U - bits_));
    }

    std::vector<std::uint32_t> entries_{};
    std::uint32_t bits_{0U};
};

// Maps the 120 short distance codes onto (x, y) pixel offsets.
constexpr std::array<std::uint8_t, plane_codes> code_to_plane{
    0x18U, 0x07U, 0x17U, 0x19U, 0x28U, 0x06U, 0x27U, 0x29U, 0x16U, 0x1aU,
    0x26U, 0x2aU, 0x38U, 0x05U, 0x37U, 0x39U, 0x15U, 0x1bU, 0x36U, 0x3aU,
    0x25U, 0x2bU, 0x48U, 0x04U, 0x47U, 0x49U, 0x14U, 0x1cU, 0x35U, 0x3bU,
    0x46U, 0x4aU, 0x24U, 0x2cU, 0x58U, 0x45U, 0x4bU, 0x34U, 0x3cU, 0x03U,
    0x57U, 0x59U, 0x13U, 0x1dU, 0x56U, 0x5aU, 0x23U, 0x2dU, 0x44U, 0x4cU,
    0x55U, 0x5bU, 0x33U, 0x3dU, 0x68U, 0x02U, 0x67U, 0x69U, 0x12U, 0x1eU,
    0x66U, 0x6aU, 0x22U, 0x2eU, 0x54U, 0x5cU, 0x43U, 0x4dU, 0x65U, 0x6bU,
    0x32U, 0x3eU, 0x78U, 0x01U, 0x77U, 0x79U, 0x53U, 0x5dU, 0x11U, 0x1fU,
    0x64U, 0x6cU, 0x42U, 0x4eU, 0x76U, 0x7aU, 0x21U, 0x2fU, 0x75U, 0x7bU,
    0x31U, 0x3fU, 0x63U, 0x6dU, 0x52U, 0x5eU, 0x00U, 0x74U, 0x7cU, 0x41U,
    0x4fU, 0x10U, 0x20U, 0x62U, 0x6eU, 0x30U, 0x73U, 0x7dU, 0x51U, 0x5fU,
    0x40U, 0x72U, 0x7eU, 0x61U, 0x6fU, 0x50U, 0x71U, 0x7fU, 0x60U, 0x70U};

// Length and distance share one prefix-code-plus-extra-bits encoding.
[[nodiscard]] std::uint32_t read_copy_value(BitReader& reader, std::uint32_t symbol) noexcept {
    if (symbol < 4U) {
        return symbol + 1U;
    }
    const std::uint32_t extra = (symbol - 2U) >> 1U;
    const std::uint32_t offset = (2U + (symbol & 1U)) << extra;
    return offset + reader.read(extra) + 1U;
}

[[nodiscard]] std::uint32_t plane_code_to_distance(std::uint32_t width, std::uint32_t plane_code) noexcept {
    if (plane_code > plane_codes) {
        return plane_code - plane_codes;
    }
    if (plane_code == 0U) {
        return 1U;
    }
    const std::uint32_t plane = code_to_plane[plane_code - 1U];
    const std::uint32_t y_offset = plane >> 4U;
    const auto x_offset = static_cast<std::int64_t>(8U - (plane & 0x0fU));
    const std::int64_t distance =
        static_cast<std::int64_t>(y_offset) * static_cast<std::int64_t>(width) + x_offset;
    return distance >= 1 ? static_cast<std::uint32_t>(distance) : 1U;
}

// Per-channel byte-wise addition of two ARGB pixels, which is how every VP8L
// transform recombines a residual with its prediction.
[[nodiscard]] std::uint32_t add_pixels(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t alpha_green = (a & 0xff00ff00U) + (b & 0xff00ff00U);
    const std::uint32_t red_blue = (a & 0x00ff00ffU) + (b & 0x00ff00ffU);
    return (alpha_green & 0xff00ff00U) | (red_blue & 0x00ff00ffU);
}

[[nodiscard]] std::uint32_t average2(std::uint32_t a, std::uint32_t b) noexcept {
    return (((a ^ b) & 0xfefefefeU) >> 1U) + (a & b);
}

[[nodiscard]] std::int32_t clip255(std::int32_t value) noexcept {
    return std::clamp(value, 0, 255);
}

[[nodiscard]] std::int32_t channel(std::uint32_t pixel, std::uint32_t shift) noexcept {
    return static_cast<std::int32_t>((pixel >> shift) & 0xffU);
}

[[nodiscard]] std::uint32_t pack(std::int32_t a, std::int32_t r, std::int32_t g, std::int32_t b) noexcept {
    return (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(r) << 16U) |
           (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(b);
}

[[nodiscard]] std::int32_t gradient_bias(std::int32_t a, std::int32_t b, std::int32_t c) noexcept {
    return std::abs(b - c) - std::abs(a - c);
}

// Picks whichever of the two neighbours the gradient through the corner favours.
[[nodiscard]] std::uint32_t select(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    const std::int32_t bias = gradient_bias(channel(a, 24U), channel(b, 24U), channel(c, 24U)) +
                              gradient_bias(channel(a, 16U), channel(b, 16U), channel(c, 16U)) +
                              gradient_bias(channel(a, 8U), channel(b, 8U), channel(c, 8U)) +
                              gradient_bias(channel(a, 0U), channel(b, 0U), channel(c, 0U));
    return bias <= 0 ? a : b;
}

[[nodiscard]] std::uint32_t clamped_add_subtract_full(std::uint32_t c0, std::uint32_t c1, std::uint32_t c2) noexcept {
    return pack(
        clip255(channel(c0, 24U) + channel(c1, 24U) - channel(c2, 24U)),
        clip255(channel(c0, 16U) + channel(c1, 16U) - channel(c2, 16U)),
        clip255(channel(c0, 8U) + channel(c1, 8U) - channel(c2, 8U)),
        clip255(channel(c0, 0U) + channel(c1, 0U) - channel(c2, 0U)));
}

[[nodiscard]] std::int32_t add_subtract_half(std::int32_t a, std::int32_t b) noexcept {
    return clip255(a + (a - b) / 2);
}

[[nodiscard]] std::uint32_t clamped_add_subtract_half(std::uint32_t c0, std::uint32_t c1, std::uint32_t c2) noexcept {
    const std::uint32_t mean = average2(c0, c1);
    return pack(
        add_subtract_half(channel(mean, 24U), channel(c2, 24U)),
        add_subtract_half(channel(mean, 16U), channel(c2, 16U)),
        add_subtract_half(channel(mean, 8U), channel(c2, 8U)),
        add_subtract_half(channel(mean, 0U), channel(c2, 0U)));
}

struct Neighbourhood {
    std::uint32_t left{};
    std::uint32_t top{};
    std::uint32_t top_left{};
    std::uint32_t top_right{};
};

[[nodiscard]] std::uint32_t predict(std::uint32_t mode, const Neighbourhood& n) noexcept {
    switch (mode) {
        case 1U: return n.left;
        case 2U: return n.top;
        case 3U: return n.top_right;
        case 4U: return n.top_left;
        case 5U: return average2(average2(n.left, n.top_right), n.top);
        case 6U: return average2(n.left, n.top_left);
        case 7U: return average2(n.left, n.top);
        case 8U: return average2(n.top_left, n.top);
        case 9U: return average2(n.top, n.top_right);
        case 10U: return average2(average2(n.left, n.top_left), average2(n.top, n.top_right));
        case 11U: return select(n.top, n.left, n.top_left);
        case 12U: return clamped_add_subtract_full(n.left, n.top, n.top_left);
        case 13U: return clamped_add_subtract_half(n.left, n.top, n.top_left);
        default: return opaque_black;
    }
}

struct Transform {
    TransformType type{TransformType::Predictor};
    std::uint32_t bits{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<std::uint32_t> data{};
};

void apply_predictor_inverse(
    const Transform& transform,
    std::uint32_t width,
    std::uint32_t height,
    std::vector<std::uint32_t>& pixels) {
    const std::uint32_t bits = transform.bits;
    const std::uint32_t tiles = subsample_size(width, bits);

    pixels[0U] = add_pixels(pixels[0U], opaque_black);
    for (std::uint32_t x = 1U; x < width; ++x) {
        pixels[x] = add_pixels(pixels[x], pixels[x - 1U]);
    }
    for (std::uint32_t y = 1U; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        const std::size_t above = row - width;
        pixels[row] = add_pixels(pixels[row], pixels[above]);
        for (std::uint32_t x = 1U; x < width; ++x) {
            const std::uint32_t mode =
                (transform.data[static_cast<std::size_t>(y >> bits) * tiles + (x >> bits)] >> 8U) & 0x0fU;
            // The top-right neighbour of the last column is the pixel that
            // follows it in the flat buffer, which is this row's first pixel.
            const Neighbourhood n{
                pixels[row + x - 1U],
                pixels[above + x],
                pixels[above + x - 1U],
                pixels[above + x + 1U]};
            pixels[row + x] = add_pixels(pixels[row + x], predict(mode < predictor_modes ? mode : 0U, n));
        }
    }
}

void apply_cross_color_inverse(
    const Transform& transform,
    std::uint32_t width,
    std::uint32_t height,
    std::vector<std::uint32_t>& pixels) {
    const std::uint32_t bits = transform.bits;
    const std::uint32_t tiles = subsample_size(width, bits);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::uint32_t code =
                transform.data[static_cast<std::size_t>(y >> bits) * tiles + (x >> bits)];
            const auto green_to_red = static_cast<std::int32_t>(static_cast<std::int8_t>(code & 0xffU));
            const auto green_to_blue = static_cast<std::int32_t>(static_cast<std::int8_t>((code >> 8U) & 0xffU));
            const auto red_to_blue = static_cast<std::int32_t>(static_cast<std::int8_t>((code >> 16U) & 0xffU));

            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            const std::uint32_t argb = pixels[index];
            const auto green = static_cast<std::int32_t>(static_cast<std::int8_t>((argb >> 8U) & 0xffU));

            std::int32_t red = channel(argb, 16U) + ((green_to_red * green) >> 5U);
            red &= 0xff;
            std::int32_t blue = channel(argb, 0U) + ((green_to_blue * green) >> 5U);
            blue += (red_to_blue * static_cast<std::int32_t>(static_cast<std::int8_t>(red))) >> 5U;
            blue &= 0xff;

            pixels[index] = (argb & 0xff00ff00U) | (static_cast<std::uint32_t>(red) << 16U) |
                            static_cast<std::uint32_t>(blue);
        }
    }
}

void apply_subtract_green_inverse(std::vector<std::uint32_t>& pixels) {
    for (std::uint32_t& pixel : pixels) {
        const std::uint32_t green = (pixel >> 8U) & 0xffU;
        const std::uint32_t red_blue = ((pixel & 0x00ff00ffU) + ((green << 16U) | green)) & 0x00ff00ffU;
        pixel = (pixel & 0xff00ff00U) | red_blue;
    }
}

// Rebuilds full-width pixels from palette indices, unpacking the several
// indices that share one byte in the narrow palette modes.
void apply_color_index_inverse(
    const Transform& transform,
    std::uint32_t& width,
    std::uint32_t height,
    std::vector<std::uint32_t>& pixels) {
    const std::uint32_t bits = transform.bits;
    if (bits == 0U) {
        for (std::uint32_t& pixel : pixels) {
            pixel = transform.data[(pixel >> 8U) & 0xffU];
        }
        return;
    }

    const std::uint32_t out_width = transform.width;
    const std::uint32_t per_byte = 1U << bits;
    const std::uint32_t bits_per_pixel = 8U >> bits;
    const std::uint32_t mask = (1U << bits_per_pixel) - 1U;
    std::vector<std::uint32_t> expanded(static_cast<std::size_t>(out_width) * height, 0U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        const std::size_t source_row = static_cast<std::size_t>(y) * width;
        const std::size_t target_row = static_cast<std::size_t>(y) * out_width;
        std::uint32_t packed = 0U;
        for (std::uint32_t x = 0U; x < out_width; ++x) {
            if ((x & (per_byte - 1U)) == 0U) {
                packed = (pixels[source_row + (x >> bits)] >> 8U) & 0xffU;
            }
            expanded[target_row + x] = transform.data[packed & mask];
            packed >>= bits_per_pixel;
        }
    }
    pixels = std::move(expanded);
    width = out_width;
}

// The palette is stored delta-coded and is padded out to the full index range
// the packed pixel layout can address, so a decoded index is always in bounds.
[[nodiscard]] bool expand_color_map(std::uint32_t colors, Transform& transform) {
    const std::uint32_t bits_per_pixel = 8U >> transform.bits;
    const std::size_t final_colors = std::size_t{1U} << bits_per_pixel;
    if (colors == 0U || static_cast<std::size_t>(colors) > final_colors ||
        transform.data.size() != colors) {
        return false;
    }
    std::vector<std::uint32_t> map(final_colors, 0U);
    map[0U] = transform.data[0U];
    for (std::uint32_t i = 1U; i < colors; ++i) {
        map[i] = add_pixels(transform.data[i], map[i - 1U]);
    }
    transform.data = std::move(map);
    return true;
}

struct HuffmanGroup {
    PrefixCode green{};
    PrefixCode red{};
    PrefixCode blue{};
    PrefixCode alpha{};
    PrefixCode distance{};
};

[[nodiscard]] RasterDecodeError decode_image_stream(
    BitReader& reader,
    std::uint32_t width,
    std::uint32_t height,
    bool top_level,
    unsigned depth,
    std::vector<std::uint32_t>& output);

// Reads one transform descriptor. Only the colour-indexing transform changes
// the width of the pixels that follow it, so `stream_width` is threaded through.
[[nodiscard]] RasterDecodeError read_transform(
    BitReader& reader,
    std::uint32_t& stream_width,
    std::uint32_t height,
    unsigned depth,
    std::uint32_t& seen_mask,
    std::vector<Transform>& transforms) {
    const std::uint32_t raw_type = reader.read(2U);
    if (reader.failed()) {
        return RasterDecodeError::MalformedContainer;
    }
    const std::uint32_t flag = 1U << raw_type;
    if ((seen_mask & flag) != 0U) {
        // Each transform type may appear at most once.
        return RasterDecodeError::MalformedContainer;
    }
    seen_mask |= flag;

    Transform transform{};
    transform.type = static_cast<TransformType>(raw_type);
    transform.width = stream_width;
    transform.height = height;

    switch (transform.type) {
        case TransformType::Predictor:
        case TransformType::CrossColor: {
            transform.bits = reader.read(3U) + 2U;
            if (reader.failed()) {
                return RasterDecodeError::MalformedContainer;
            }
            const std::uint32_t tile_width = subsample_size(stream_width, transform.bits);
            const std::uint32_t tile_height = subsample_size(height, transform.bits);
            const RasterDecodeError error =
                decode_image_stream(reader, tile_width, tile_height, false, depth + 1U, transform.data);
            if (error != RasterDecodeError::None) {
                return error;
            }
            if (transform.data.size() != static_cast<std::size_t>(tile_width) * tile_height) {
                return RasterDecodeError::MalformedContainer;
            }
            break;
        }
        case TransformType::SubtractGreen:
            break;
        case TransformType::ColorIndexing: {
            const std::uint32_t colors = reader.read(8U) + 1U;
            if (reader.failed()) {
                return RasterDecodeError::MalformedContainer;
            }
            transform.bits = colors > 16U ? 0U : (colors > 4U ? 1U : (colors > 2U ? 2U : 3U));
            const RasterDecodeError error =
                decode_image_stream(reader, colors, 1U, false, depth + 1U, transform.data);
            if (error != RasterDecodeError::None) {
                return error;
            }
            if (!expand_color_map(colors, transform)) {
                return RasterDecodeError::MalformedContainer;
            }
            stream_width = subsample_size(stream_width, transform.bits);
            break;
        }
    }
    transforms.push_back(std::move(transform));
    return RasterDecodeError::None;
}

RasterDecodeError decode_image_stream(
    BitReader& reader,
    std::uint32_t width,
    std::uint32_t height,
    bool top_level,
    unsigned depth,
    std::vector<std::uint32_t>& output) {
    if (depth > max_stream_depth) {
        return RasterDecodeError::UnsupportedFeature;
    }
    if (width == 0U || height == 0U) {
        return RasterDecodeError::MalformedContainer;
    }
    if (static_cast<std::size_t>(width) > raster_decode_max_pixels / height) {
        return RasterDecodeError::PixelBudgetExceeded;
    }

    std::vector<Transform> transforms;
    std::uint32_t seen_mask = 0U;
    std::uint32_t stream_width = width;
    while (top_level) {
        const std::uint32_t present = reader.read(1U);
        if (reader.failed()) {
            return RasterDecodeError::MalformedContainer;
        }
        if (present == 0U) {
            break;
        }
        const RasterDecodeError error =
            read_transform(reader, stream_width, height, depth, seen_mask, transforms);
        if (error != RasterDecodeError::None) {
            return error;
        }
    }

    std::uint32_t cache_bits = 0U;
    if (reader.read(1U) != 0U) {
        cache_bits = reader.read(4U);
        if (reader.failed() || cache_bits < 1U || cache_bits > max_cache_bits) {
            return RasterDecodeError::MalformedContainer;
        }
    }
    if (reader.failed()) {
        return RasterDecodeError::MalformedContainer;
    }

    // An entropy image assigns a different prefix-code group to each block of
    // the image; without one the whole image shares a single group.
    std::vector<std::uint32_t> entropy;
    std::uint32_t entropy_bits = 0U;
    std::uint32_t entropy_width = 0U;
    std::uint32_t group_count = 1U;
    if (top_level && reader.read(1U) != 0U) {
        entropy_bits = reader.read(3U) + 2U;
        if (reader.failed()) {
            return RasterDecodeError::MalformedContainer;
        }
        entropy_width = subsample_size(stream_width, entropy_bits);
        const std::uint32_t entropy_height = subsample_size(height, entropy_bits);
        const RasterDecodeError error =
            decode_image_stream(reader, entropy_width, entropy_height, false, depth + 1U, entropy);
        if (error != RasterDecodeError::None) {
            return error;
        }
        if (entropy.size() != static_cast<std::size_t>(entropy_width) * entropy_height) {
            return RasterDecodeError::MalformedContainer;
        }
        std::uint32_t highest = 0U;
        for (std::uint32_t& pixel : entropy) {
            pixel = (pixel >> 8U) & 0xffffU;
            highest = std::max(highest, pixel);
        }
        group_count = highest + 1U;
    }
    if (reader.failed()) {
        return RasterDecodeError::MalformedContainer;
    }
    // Every group costs bits to describe, so the claimed group count cannot
    // exceed what is left of the bounded input.
    if (static_cast<std::size_t>(group_count) > reader.remaining_bits() / 5U) {
        return RasterDecodeError::MalformedContainer;
    }

    const std::uint32_t cache_size = cache_bits > 0U ? (1U << cache_bits) : 0U;
    const std::uint32_t green_alphabet = literal_codes + length_codes + cache_size;
    std::vector<HuffmanGroup> groups(group_count);
    for (HuffmanGroup& group : groups) {
        if (!read_prefix_code(reader, green_alphabet, group.green) ||
            !read_prefix_code(reader, literal_codes, group.red) ||
            !read_prefix_code(reader, literal_codes, group.blue) ||
            !read_prefix_code(reader, literal_codes, group.alpha) ||
            !read_prefix_code(reader, distance_codes, group.distance)) {
            return RasterDecodeError::MalformedContainer;
        }
    }

    const std::size_t pixel_count = static_cast<std::size_t>(stream_width) * height;
    std::vector<std::uint32_t> pixels(pixel_count, 0U);
    ColorCache cache;
    if (cache_bits > 0U) {
        cache.reset(cache_bits);
    }

    std::size_t position = 0U;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    while (position < pixel_count) {
        const std::size_t group_index =
            entropy.empty()
                ? 0U
                : entropy[static_cast<std::size_t>(y >> entropy_bits) * entropy_width + (x >> entropy_bits)];
        const HuffmanGroup& group = groups[group_index];
        const std::uint32_t code = group.green.read(reader);
        if (reader.failed()) {
            return RasterDecodeError::TruncatedPixelData;
        }

        if (code < literal_codes) {
            const std::uint32_t red = group.red.read(reader);
            const std::uint32_t blue = group.blue.read(reader);
            const std::uint32_t alpha = group.alpha.read(reader);
            if (reader.failed()) {
                return RasterDecodeError::TruncatedPixelData;
            }
            const std::uint32_t argb = (alpha << 24U) | (red << 16U) | (code << 8U) | blue;
            pixels[position++] = argb;
            if (cache.active()) {
                cache.insert(argb);
            }
            if (++x == stream_width) {
                x = 0U;
                ++y;
            }
        } else if (code < literal_codes + length_codes) {
            const std::uint32_t length = read_copy_value(reader, code - literal_codes);
            const std::uint32_t distance_symbol = group.distance.read(reader);
            if (reader.failed()) {
                return RasterDecodeError::TruncatedPixelData;
            }
            const std::uint32_t plane_code = read_copy_value(reader, distance_symbol);
            if (reader.failed()) {
                return RasterDecodeError::TruncatedPixelData;
            }
            const std::uint32_t distance = plane_code_to_distance(stream_width, plane_code);
            if (static_cast<std::size_t>(distance) > position ||
                static_cast<std::size_t>(length) > pixel_count - position) {
                return RasterDecodeError::MalformedContainer;
            }
            for (std::uint32_t i = 0U; i < length; ++i) {
                const std::uint32_t argb = pixels[position - distance];
                pixels[position++] = argb;
                if (cache.active()) {
                    cache.insert(argb);
                }
            }
            x = static_cast<std::uint32_t>(position % stream_width);
            y = static_cast<std::uint32_t>(position / stream_width);
        } else {
            const std::uint32_t index = code - literal_codes - length_codes;
            if (!cache.active() || static_cast<std::size_t>(index) >= cache.size()) {
                return RasterDecodeError::MalformedContainer;
            }
            const std::uint32_t argb = cache.lookup(index);
            pixels[position++] = argb;
            cache.insert(argb);
            if (++x == stream_width) {
                x = 0U;
                ++y;
            }
        }
    }

    // Transforms are applied in the reverse of the order they were read.
    std::uint32_t current_width = stream_width;
    for (std::size_t i = transforms.size(); i-- > 0U;) {
        const Transform& transform = transforms[i];
        if (transform.height != height) {
            return RasterDecodeError::MalformedContainer;
        }
        switch (transform.type) {
            case TransformType::Predictor:
                if (transform.width != current_width) {
                    return RasterDecodeError::MalformedContainer;
                }
                apply_predictor_inverse(transform, current_width, height, pixels);
                break;
            case TransformType::CrossColor:
                if (transform.width != current_width) {
                    return RasterDecodeError::MalformedContainer;
                }
                apply_cross_color_inverse(transform, current_width, height, pixels);
                break;
            case TransformType::SubtractGreen:
                apply_subtract_green_inverse(pixels);
                break;
            case TransformType::ColorIndexing:
                if (subsample_size(transform.width, transform.bits) != current_width) {
                    return RasterDecodeError::MalformedContainer;
                }
                apply_color_index_inverse(transform, current_width, height, pixels);
                break;
        }
    }
    if (current_width != width || pixels.size() != static_cast<std::size_t>(width) * height) {
        return RasterDecodeError::MalformedContainer;
    }

    output = std::move(pixels);
    return RasterDecodeError::None;
}

[[nodiscard]] RasterDecodeResult fail(RasterDecodeError error) noexcept {
    return {error, {}};
}

[[nodiscard]] RasterDecodeResult decode_vp8l(std::span<const std::uint8_t> payload) {
    if (payload.empty() || payload[0U] != vp8l_signature) {
        return fail(RasterDecodeError::MalformedContainer);
    }

    BitReader reader(payload.subspan(1U));
    const std::uint32_t width = reader.read(14U) + 1U;
    const std::uint32_t height = reader.read(14U) + 1U;
    // The alpha hint is advisory; the decoded alpha channel is authoritative.
    static_cast<void>(reader.read(1U));
    const std::uint32_t version = reader.read(3U);
    if (reader.failed()) {
        return fail(RasterDecodeError::MalformedContainer);
    }
    if (version != 0U) {
        return fail(RasterDecodeError::UnsupportedFeature);
    }
    if (width > raster_decode_max_dimension || height > raster_decode_max_dimension) {
        return fail(RasterDecodeError::DimensionLimitExceeded);
    }
    if (static_cast<std::size_t>(width) > raster_decode_max_pixels / height) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    }

    std::vector<std::uint32_t> argb;
    const RasterDecodeError error = decode_image_stream(reader, width, height, true, 0U, argb);
    if (error != RasterDecodeError::None) {
        return fail(error);
    }

    DecodedRaster decoded{};
    decoded.spec.width = width;
    decoded.spec.height = height;
    decoded.spec.layout = core::PixelLayout::RGBA;
    decoded.spec.channel_type = core::ChannelType::UInt8;
    decoded.spec.transfer = core::TransferFunction::SRGB;
    decoded.spec.primaries = core::ColorPrimaries::SRGB;
    // VP8L stores straight, non-premultiplied alpha.
    decoded.spec.alpha = core::AlphaMode::Straight;
    decoded.rgba8.resize(argb.size() * 4U);
    for (std::size_t i = 0U; i < argb.size(); ++i) {
        const std::uint32_t pixel = argb[i];
        const std::size_t target = i * 4U;
        decoded.rgba8[target] = static_cast<std::uint8_t>((pixel >> 16U) & 0xffU);
        decoded.rgba8[target + 1U] = static_cast<std::uint8_t>((pixel >> 8U) & 0xffU);
        decoded.rgba8[target + 2U] = static_cast<std::uint8_t>(pixel & 0xffU);
        decoded.rgba8[target + 3U] = static_cast<std::uint8_t>((pixel >> 24U) & 0xffU);
    }
    return {RasterDecodeError::None, std::move(decoded)};
}

[[nodiscard]] std::uint32_t read_le32(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

[[nodiscard]] bool fourcc(std::span<const std::uint8_t> bytes, std::size_t offset, const char* tag) noexcept {
    if (bytes.size() < offset + 4U) {
        return false;
    }
    for (std::size_t i = 0U; i < 4U; ++i) {
        if (bytes[offset + i] != static_cast<std::uint8_t>(tag[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

RasterDecodeResult decode_webp(std::span<const std::uint8_t> bytes) noexcept {
    try {
        constexpr std::size_t riff_header_size = 12U;
        if (bytes.size() < riff_header_size || !fourcc(bytes, 0U, "RIFF") || !fourcc(bytes, 8U, "WEBP")) {
            return fail(RasterDecodeError::MalformedContainer);
        }
        const std::uint32_t riff_size = read_le32(bytes, 4U);
        if (riff_size < 4U || static_cast<std::size_t>(riff_size) > bytes.size() - 8U) {
            return fail(RasterDecodeError::MalformedContainer);
        }

        const std::size_t end = 8U + static_cast<std::size_t>(riff_size);
        std::size_t offset = riff_header_size;
        std::span<const std::uint8_t> lossless{};
        bool unsupported = false;
        while (offset + 8U <= end) {
            const std::size_t size = static_cast<std::size_t>(read_le32(bytes, offset + 4U));
            const std::size_t payload = offset + 8U;
            if (size > end - payload) {
                return fail(RasterDecodeError::MalformedContainer);
            }
            if (fourcc(bytes, offset, "VP8L")) {
                if (!lossless.empty()) {
                    return fail(RasterDecodeError::MalformedContainer);
                }
                lossless = bytes.subspan(payload, size);
            } else if (fourcc(bytes, offset, "VP8 ") || fourcc(bytes, offset, "ALPH") ||
                       fourcc(bytes, offset, "ANIM") || fourcc(bytes, offset, "ANMF") ||
                       fourcc(bytes, offset, "ICCP")) {
                // Lossy VP8, animation and embedded ICC profiles are not decoded
                // here, and guessing at them would produce wrong pixels or
                // mislabelled colour.
                unsupported = true;
            }
            offset = payload + size + (size & 1U);
        }
        if (unsupported) {
            return fail(RasterDecodeError::UnsupportedFeature);
        }
        if (lossless.empty()) {
            return fail(RasterDecodeError::UnsupportedFeature);
        }
        return decode_vp8l(lossless);
    } catch (const std::bad_alloc&) {
        return fail(RasterDecodeError::PixelBudgetExceeded);
    } catch (...) {
        return fail(RasterDecodeError::MalformedContainer);
    }
}

}  // namespace vektoryum::io::detail
