#include "vektoryum/io/raster_decode.hpp"

#include "png_baseline_fixtures.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] std::uint8_t byte_of(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value & 0xffU);
}

void append_u32_be(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

[[nodiscard]] std::uint32_t crc32_of(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t value : data) {
        crc ^= static_cast<std::uint32_t>(value);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
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

void append_chunk(std::vector<std::uint8_t>& png, std::string_view type, std::span<const std::uint8_t> data) {
    append_u32_be(png, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> body;
    for (const char c : type) {
        body.push_back(static_cast<std::uint8_t>(c));
    }
    body.insert(body.end(), data.begin(), data.end());
    png.insert(png.end(), body.begin(), body.end());
    append_u32_be(png, crc32_of(body));
}

// A zlib stream of stored DEFLATE blocks, which lets the test choose exactly
// what filtered bytes reach the decoder without needing a compressor.
[[nodiscard]] std::vector<std::uint8_t> stored_zlib(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> stream{0x78U, 0x01U};
    std::size_t offset = 0U;
    do {
        const std::size_t block = std::min<std::size_t>(data.size() - offset, 0xffffU);
        const bool final_block = offset + block == data.size();
        stream.push_back(final_block ? 0x01U : 0x00U);
        stream.push_back(static_cast<std::uint8_t>(block & 0xffU));
        stream.push_back(static_cast<std::uint8_t>((block >> 8U) & 0xffU));
        stream.push_back(static_cast<std::uint8_t>(~block & 0xffU));
        stream.push_back(static_cast<std::uint8_t>((~block >> 8U) & 0xffU));
        stream.insert(stream.end(), data.begin() + static_cast<std::ptrdiff_t>(offset),
                      data.begin() + static_cast<std::ptrdiff_t>(offset + block));
        offset += block;
    } while (offset < data.size());
    stream.reserve(stream.size() + 4U);
    append_u32_be(stream, adler32(data));
    return stream;
}

[[nodiscard]] std::uint8_t paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c) noexcept {
    const int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    const int pa = std::abs(p - static_cast<int>(a));
    const int pb = std::abs(p - static_cast<int>(b));
    const int pc = std::abs(p - static_cast<int>(c));
    if (pa <= pb && pa <= pc) {
        return a;
    }
    return pb <= pc ? b : c;
}

// Applies one PNG filter to a scanline, which the decoder then has to invert.
void filter_row(
    std::uint8_t filter,
    std::span<const std::uint8_t> row,
    std::span<const std::uint8_t> previous,
    std::size_t bytes_per_pixel,
    std::vector<std::uint8_t>& out) {
    out.push_back(filter);
    for (std::size_t i = 0U; i < row.size(); ++i) {
        const std::uint8_t left = i >= bytes_per_pixel ? row[i - bytes_per_pixel] : 0U;
        const std::uint8_t up = previous.empty() ? 0U : previous[i];
        const std::uint8_t up_left =
            (previous.empty() || i < bytes_per_pixel) ? 0U : previous[i - bytes_per_pixel];
        unsigned predictor = 0U;
        switch (filter) {
            case 1U: predictor = left; break;
            case 2U: predictor = up; break;
            case 3U: predictor = (static_cast<unsigned>(left) + static_cast<unsigned>(up)) / 2U; break;
            case 4U: predictor = paeth(left, up, up_left); break;
            default: predictor = 0U; break;
        }
        out.push_back(static_cast<std::uint8_t>((static_cast<unsigned>(row[i]) - predictor) & 0xffU));
    }
}

[[nodiscard]] std::size_t samples_for(std::uint8_t color_type) noexcept {
    switch (color_type) {
        case 0U: return 1U;
        case 2U: return 3U;
        case 4U: return 2U;
        default: return 4U;
    }
}

// Builds a PNG whose scanlines use the requested filters in rotation.
[[nodiscard]] std::vector<std::uint8_t> build_png(
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t color_type,
    std::span<const std::uint8_t> filters,
    const std::function<std::uint8_t(std::uint32_t, std::uint32_t, std::size_t)>& sample) {
    const std::size_t channels = samples_for(color_type);
    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};

    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, width);
    append_u32_be(ihdr, height);
    ihdr.insert(ihdr.end(), {8U, color_type, 0U, 0U, 0U});
    append_chunk(png, "IHDR", ihdr);

    std::vector<std::uint8_t> filtered;
    std::vector<std::uint8_t> previous;
    for (std::uint32_t y = 0U; y < height; ++y) {
        std::vector<std::uint8_t> row;
        row.reserve(static_cast<std::size_t>(width) * channels);
        for (std::uint32_t x = 0U; x < width; ++x) {
            for (std::size_t channel = 0U; channel < channels; ++channel) {
                row.push_back(sample(x, y, channel));
            }
        }
        filter_row(filters[y % filters.size()], row, previous, channels, filtered);
        previous = row;
    }

    append_chunk(png, "IDAT", stored_zlib(filtered));
    append_chunk(png, "IEND", std::span<const std::uint8_t>{});
    return png;
}

[[nodiscard]] std::vector<std::uint8_t> expected_rgba(
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t color_type,
    const std::function<std::uint8_t(std::uint32_t, std::uint32_t, std::size_t)>& sample) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            if (color_type == 0U) {
                const std::uint8_t v = sample(x, y, 0U);
                rgba.insert(rgba.end(), {v, v, v, 255U});
            } else if (color_type == 4U) {
                const std::uint8_t v = sample(x, y, 0U);
                rgba.insert(rgba.end(), {v, v, v, sample(x, y, 1U)});
            } else if (color_type == 2U) {
                rgba.insert(rgba.end(),
                            {sample(x, y, 0U), sample(x, y, 1U), sample(x, y, 2U), 255U});
            } else {
                rgba.insert(rgba.end(), {sample(x, y, 0U), sample(x, y, 1U), sample(x, y, 2U),
                                         sample(x, y, 3U)});
            }
        }
    }
    return rgba;
}

void check_fixture(
    std::string_view name,
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>& pixel) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, encoded);
    if (!decoded.ok()) {
        std::cerr << "FAIL: " << name << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    if (decoded.image.spec.width != width || decoded.image.spec.height != height) {
        std::cerr << "FAIL: " << name << " dimensions must match IHDR\n";
        ++failures;
        return;
    }
    expect(decoded.image.spec.alpha == vektoryum::core::AlphaMode::Straight,
           "PNG alpha must be straight");
    expect(decoded.image.spec.transfer == vektoryum::core::TransferFunction::SRGB,
           "PNG must normalize to the sRGB transfer function");

    std::vector<std::uint8_t> expected;
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::array<std::uint8_t, 4U> value = pixel(x, y);
            expected.insert(expected.end(), value.begin(), value.end());
        }
    }
    if (decoded.image.rgba8 != expected) {
        std::cerr << "FAIL: " << name << " pixels differ from the encoded source\n";
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace vektoryum_test_fixtures;

    // Every filter type against every supported colour type. libpng's heuristic
    // never picks the Average filter, so these frames are built here.
    const auto sample = [](std::uint32_t x, std::uint32_t y, std::size_t channel) {
        return byte_of(x * 23U + y * 41U + static_cast<std::uint32_t>(channel) * 67U + 11U);
    };
    for (const std::uint8_t color_type : std::array<std::uint8_t, 4U>{0U, 2U, 4U, 6U}) {
        for (std::uint8_t filter = 0U; filter <= 4U; ++filter) {
            const std::array<std::uint8_t, 1U> single{filter};
            const auto png = build_png(7U, 5U, color_type, single, sample);
            const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, png);
            const std::string label =
                "colour type " + std::to_string(color_type) + " with filter " + std::to_string(filter);
            if (!decoded.ok()) {
                std::cerr << "FAIL: " << label << " must decode, got "
                          << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
                ++failures;
                continue;
            }
            if (decoded.image.rgba8 != expected_rgba(7U, 5U, color_type, sample)) {
                std::cerr << "FAIL: " << label << " must reconstruct the unfiltered scanlines\n";
                ++failures;
            }
        }

        // Filters mixed within one image, which is what encoders actually emit.
        const std::array<std::uint8_t, 5U> mixed{0U, 1U, 2U, 3U, 4U};
        const auto png = build_png(9U, 10U, color_type, mixed, sample);
        const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, png);
        if (!decoded.ok() || decoded.image.rgba8 != expected_rgba(9U, 10U, color_type, sample)) {
            std::cerr << "FAIL: colour type " << static_cast<unsigned>(color_type)
                      << " with per-row filter changes must reconstruct exactly\n";
            ++failures;
        }
    }

    // An undefined filter type must be refused rather than treated as None.
    {
        const std::array<std::uint8_t, 1U> bad{5U};
        const auto png = build_png(7U, 5U, 2U, bad, sample);
        expect(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, png).ok(),
               "a PNG scanline filter above 4 must fail closed");
    }

    // Real libpng output for each colour type.
    check_fixture("greyscale 13x9", png_gray_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        const std::uint8_t v = byte_of(x * 11U + y * 7U);
        return std::array<std::uint8_t, 4U>{v, v, v, 255U};
    });
    check_fixture("greyscale+alpha 13x9", png_gray_alpha_13x9, 13U, 9U,
                  [](std::uint32_t x, std::uint32_t y) {
                      const std::uint8_t v = byte_of(x * 11U + y * 7U);
                      return std::array<std::uint8_t, 4U>{v, v, v, byte_of((y * 255U) / 8U)};
                  });
    check_fixture("RGB 13x9", png_rgb_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), 255U};
    });
    check_fixture("RGBA 13x9", png_rgba_13x9, 13U, 9U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), byte_of((x * 255U) / 12U)};
    });
    check_fixture("RGBA 64x48", png_rgba_64x48, 64U, 48U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), byte_of((x * 255U) / 63U)};
    });

    // Corruption anywhere in a real file must be caught, not decoded around.
    {
        std::vector<std::uint8_t> corrupt(png_rgba_13x9.begin(), png_rgba_13x9.end());
        corrupt[corrupt.size() - 10U] ^= 0xffU;
        expect(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, corrupt).ok(),
               "a PNG with a corrupted IDAT must fail closed");
    }
    {
        std::vector<std::uint8_t> truncated(png_rgba_64x48.begin(), png_rgba_64x48.end() - 20);
        expect(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, truncated).ok(),
               "a truncated PNG must fail closed");
    }

    return failures == 0 ? 0 : 1;
}
