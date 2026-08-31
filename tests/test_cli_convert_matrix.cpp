#include "jpeg_baseline_fixtures.hpp"
#include "tiff_baseline_fixtures.hpp"
#include "webp_lossless_fixtures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void write_u16_le(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void write_u32_le(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

void append_u16_be(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u32_be(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

[[nodiscard]] std::uint32_t crc32_update(std::uint32_t crc, std::uint8_t value) noexcept {
    crc ^= static_cast<std::uint32_t>(value);
    for (unsigned bit = 0U; bit < 8U; ++bit) {
        const std::uint32_t mask = 0U - (crc & 1U);
        crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
    return crc;
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

void append_png_chunk(
    std::vector<std::uint8_t>& png,
    const std::array<std::uint8_t, 4U>& type,
    std::span<const std::uint8_t> data) {
    append_u32_be(png, static_cast<std::uint32_t>(data.size()));
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t value : type) {
        png.push_back(value);
        crc = crc32_update(crc, value);
    }
    for (const std::uint8_t value : data) {
        png.push_back(value);
        crc = crc32_update(crc, value);
    }
    append_u32_be(png, crc ^ 0xffffffffU);
}

[[nodiscard]] std::vector<std::uint8_t> make_rgba_png() {
    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};
    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, 2U);
    append_u32_be(ihdr, 1U);
    ihdr.insert(ihdr.end(), {8U, 6U, 0U, 0U, 0U});
    append_png_chunk(png, {0x49U, 0x48U, 0x44U, 0x52U}, ihdr);

    const std::vector<std::uint8_t> filtered{0U, 12U, 34U, 56U, 78U, 90U, 123U, 210U, 255U};
    std::vector<std::uint8_t> zlib{0x78U, 0x01U, 0x01U};
    const std::uint16_t length = static_cast<std::uint16_t>(filtered.size());
    const std::uint16_t inverse = static_cast<std::uint16_t>(length ^ 0xffffU);
    zlib.push_back(static_cast<std::uint8_t>(length & 0xffU));
    zlib.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xffU));
    zlib.push_back(static_cast<std::uint8_t>(inverse & 0xffU));
    zlib.push_back(static_cast<std::uint8_t>((inverse >> 8U) & 0xffU));
    zlib.insert(zlib.end(), filtered.begin(), filtered.end());
    append_u32_be(zlib, adler32(filtered));
    append_png_chunk(png, {0x49U, 0x44U, 0x41U, 0x54U}, zlib);
    append_png_chunk(png, {0x49U, 0x45U, 0x4eU, 0x44U}, std::span<const std::uint8_t>{});
    return png;
}

void append_jpeg_segment(
    std::vector<std::uint8_t>& jpeg,
    std::uint8_t marker,
    std::span<const std::uint8_t> payload) {
    jpeg.push_back(0xffU);
    jpeg.push_back(marker);
    append_u16_be(jpeg, static_cast<std::uint16_t>(payload.size() + 2U));
    jpeg.insert(jpeg.end(), payload.begin(), payload.end());
}

[[nodiscard]] std::vector<std::uint8_t> make_grayscale_jpeg() {
    std::vector<std::uint8_t> jpeg{0xffU, 0xd8U};
    std::vector<std::uint8_t> dqt(65U, 1U);
    dqt[0U] = 0U;
    append_jpeg_segment(jpeg, 0xdbU, dqt);
    const std::vector<std::uint8_t> sof0{8U, 0U, 1U, 0U, 1U, 1U, 1U, 0x11U, 0U};
    append_jpeg_segment(jpeg, 0xc0U, sof0);
    std::vector<std::uint8_t> dc(18U, 0U);
    dc[0U] = 0x00U;
    dc[1U] = 1U;
    dc[17U] = 0U;
    append_jpeg_segment(jpeg, 0xc4U, dc);
    std::vector<std::uint8_t> ac(18U, 0U);
    ac[0U] = 0x10U;
    ac[1U] = 1U;
    ac[17U] = 0U;
    append_jpeg_segment(jpeg, 0xc4U, ac);
    const std::vector<std::uint8_t> sos{1U, 1U, 0x00U, 0U, 63U, 0U};
    append_jpeg_segment(jpeg, 0xdaU, sos);
    jpeg.push_back(0x3fU);
    jpeg.push_back(0xffU);
    jpeg.push_back(0xd9U);
    return jpeg;
}

void write_ifd_entry(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t tag,
    std::uint16_t type,
    std::uint32_t count,
    std::uint32_t value) {
    write_u16_le(bytes, offset, tag);
    write_u16_le(bytes, offset + 2U, type);
    write_u32_le(bytes, offset + 4U, count);
    write_u32_le(bytes, offset + 8U, value);
}

[[nodiscard]] std::vector<std::uint8_t> make_rgba_tiff() {
    constexpr std::uint16_t entry_count = 10U;
    constexpr std::size_t ifd_offset = 8U;
    constexpr std::size_t entries_start = ifd_offset + 2U;
    constexpr std::size_t ifd_end = entries_start + static_cast<std::size_t>(entry_count) * 12U + 4U;
    constexpr std::size_t bits_offset = ifd_end;
    constexpr std::size_t pixels_offset = bits_offset + 8U;
    constexpr std::size_t pixel_bytes = 8U;
    std::vector<std::uint8_t> bytes(pixels_offset + pixel_bytes, 0U);
    bytes[0U] = 0x49U;
    bytes[1U] = 0x49U;
    write_u16_le(bytes, 2U, 42U);
    write_u32_le(bytes, 4U, static_cast<std::uint32_t>(ifd_offset));
    write_u16_le(bytes, ifd_offset, entry_count);
    std::size_t entry = entries_start;
    write_ifd_entry(bytes, entry, 256U, 4U, 1U, 2U); entry += 12U;
    write_ifd_entry(bytes, entry, 257U, 4U, 1U, 1U); entry += 12U;
    write_ifd_entry(bytes, entry, 258U, 3U, 4U, static_cast<std::uint32_t>(bits_offset)); entry += 12U;
    write_ifd_entry(bytes, entry, 259U, 3U, 1U, 1U); entry += 12U;
    write_ifd_entry(bytes, entry, 262U, 3U, 1U, 2U); entry += 12U;
    write_ifd_entry(bytes, entry, 273U, 4U, 1U, static_cast<std::uint32_t>(pixels_offset)); entry += 12U;
    write_ifd_entry(bytes, entry, 277U, 3U, 1U, 4U); entry += 12U;
    write_ifd_entry(bytes, entry, 279U, 4U, 1U, static_cast<std::uint32_t>(pixel_bytes)); entry += 12U;
    write_ifd_entry(bytes, entry, 284U, 3U, 1U, 1U); entry += 12U;
    write_ifd_entry(bytes, entry, 338U, 3U, 1U, 2U);
    for (std::size_t i = 0U; i < 4U; ++i) {
        write_u16_le(bytes, bits_offset + i * 2U, 8U);
    }
    const std::array<std::uint8_t, 8U> pixels{64U, 32U, 16U, 128U, 10U, 20U, 30U, 255U};
    for (std::size_t i = 0U; i < pixels.size(); ++i) {
        bytes[pixels_offset + i] = pixels[i];
    }
    return bytes;
}

void write_binary(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "fixture output must open");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "fixture output must write completely");
}

[[nodiscard]] std::vector<std::uint8_t> read_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(static_cast<bool>(in), "converted PAM must exist");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string quote(const std::filesystem::path& path) {
    return std::string{"\""} + path.string() + "\"";
}

[[nodiscard]] int run_command(const std::string& command) {
#ifdef _WIN32
    const std::string wrapped = std::string{"\""} + command + "\"";
    return std::system(wrapped.c_str());
#else
    return std::system(command.c_str());
#endif
}

void run_case(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view stem,
    std::string_view extension,
    std::span<const std::uint8_t> input,
    std::span<const std::uint8_t> expected_pixels,
    std::size_t width,
    std::size_t height) {
    const auto input_path = dir / (std::string(stem) + std::string(extension));
    const auto output_path = dir / (std::string(stem) + ".pam");
    write_binary(input_path, input);
    const std::string command = quote(cli) + " --convert " + quote(input_path) + " " + quote(output_path);
    require(run_command(command) == 0, "CLI --convert must succeed for accepted raster fixture");
    const auto actual = read_binary(output_path);
    const std::string header = "P7\nWIDTH " + std::to_string(width) + "\nHEIGHT " + std::to_string(height) +
                               "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    require(actual.size() == header.size() + expected_pixels.size(), "PAM output size must match canonical header plus RGBA8 pixels");
    require(std::equal(header.begin(), header.end(), actual.begin()), "PAM output header must be canonical and deterministic");
    require(std::equal(expected_pixels.begin(), expected_pixels.end(), actual.begin() + static_cast<std::ptrdiff_t>(header.size())),
            "PAM output pixels must match canonical decoded RGBA8 bytes");
}


[[nodiscard]] std::uint8_t byte_of(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value & 0xffU);
}

using PixelSource = std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>;

[[nodiscard]] std::vector<std::uint8_t> render(std::uint32_t width, std::uint32_t height, const PixelSource& pixel) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::array<std::uint8_t, 4U> value = pixel(x, y);
            rgba.insert(rgba.end(), value.begin(), value.end());
        }
    }
    return rgba;
}

[[nodiscard]] std::string canonical_header(std::uint32_t width, std::uint32_t height) {
    return "P7\nWIDTH " + std::to_string(width) + "\nHEIGHT " + std::to_string(height) +
           "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
}

// Runs the real CLI over a file written by a reference encoder and returns the
// PAM pixel payload after checking the canonical header, the size, and that a
// second run produces byte-identical output.
[[nodiscard]] std::vector<std::uint8_t> convert_and_read(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view stem,
    std::string_view extension,
    std::span<const std::uint8_t> input,
    std::uint32_t width,
    std::uint32_t height) {
    const auto input_path = dir / (std::string(stem) + std::string(extension));
    const auto output_path = dir / (std::string(stem) + ".pam");
    write_binary(input_path, input);
    const std::string command = quote(cli) + " --convert " + quote(input_path) + " " + quote(output_path);
    require(run_command(command) == 0, "CLI --convert must succeed for an accepted raster fixture");

    const auto actual = read_binary(output_path);
    const std::string header = canonical_header(width, height);
    const std::size_t expected_bytes = static_cast<std::size_t>(width) * height * 4U;
    require(actual.size() == header.size() + expected_bytes,
            "PAM output size must match the canonical header plus RGBA8 pixels");
    require(std::equal(header.begin(), header.end(), actual.begin()),
            "PAM output header must be canonical and deterministic");

    const auto repeat_path = dir / (std::string(stem) + ".repeat.pam");
    const std::string repeat = quote(cli) + " --convert " + quote(input_path) + " " + quote(repeat_path);
    require(run_command(repeat) == 0, "CLI --convert must succeed on a repeated run");
    require(read_binary(repeat_path) == actual, "CLI --convert must be deterministic across runs");

    return std::vector<std::uint8_t>(actual.begin() + static_cast<std::ptrdiff_t>(header.size()), actual.end());
}

// Lossless formats must come back out of the CLI exactly as they went in.
void run_lossless_case(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view stem,
    std::string_view extension,
    std::span<const std::uint8_t> input,
    std::uint32_t width,
    std::uint32_t height,
    const PixelSource& pixel) {
    const auto payload = convert_and_read(cli, dir, stem, extension, input, width, height);
    require(payload == render(width, height, pixel),
            "PAM pixels must reproduce the image the encoder was given");
}

// JPEG is lossy, so its CLI output is held to the same measured fidelity gate
// the decoder acceptance uses: a PSNR floor plus a per-channel bound.
void run_lossy_case(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view stem,
    std::string_view extension,
    std::span<const std::uint8_t> input,
    std::uint32_t width,
    std::uint32_t height,
    double psnr_floor,
    int channel_bound,
    const PixelSource& pixel) {
    const auto payload = convert_and_read(cli, dir, stem, extension, input, width, height);
    const auto expected = render(width, height, pixel);
    double squared_error = 0.0;
    int worst = 0;
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    for (std::size_t i = 0U; i < pixel_count; ++i) {
        require(payload[i * 4U + 3U] == 255U, "a JPEG conversion must stay opaque");
        for (std::size_t channel = 0U; channel < 3U; ++channel) {
            const int difference = static_cast<int>(payload[i * 4U + channel]) -
                                   static_cast<int>(expected[i * 4U + channel]);
            squared_error += static_cast<double>(difference) * static_cast<double>(difference);
            worst = std::max(worst, std::abs(difference));
        }
    }
    const double mean_squared_error = squared_error / static_cast<double>(pixel_count * 3U);
    const double psnr = mean_squared_error == 0.0
                            ? std::numeric_limits<double>::infinity()
                            : 10.0 * std::log10(255.0 * 255.0 / mean_squared_error);
    require(psnr >= psnr_floor, "converted JPEG PAM must meet its PSNR floor against the source image");
    require(worst <= channel_bound, "converted JPEG PAM must stay inside its per-channel bound");
}

void run_rejected_case(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view stem,
    std::string_view extension,
    std::span<const std::uint8_t> input,
    std::string_view message) {
    const auto input_path = dir / (std::string(stem) + std::string(extension));
    const auto output_path = dir / (std::string(stem) + ".pam");
    write_binary(input_path, input);
    const std::string command = quote(cli) + " --convert " + quote(input_path) + " " + quote(output_path);
    require(run_command(command) != 0, message);
    require(!std::filesystem::exists(output_path), "a rejected conversion must not leave an output file");
}

}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "test requires path to vektoryum_cli");
    const std::filesystem::path cli = argv[1];
    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-r2-cli-convert-matrix";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    require(std::filesystem::create_directories(dir, ec), "temporary fixture directory must be created");

    const auto png = make_rgba_png();
    const std::array<std::uint8_t, 8U> png_pixels{12U, 34U, 56U, 78U, 90U, 123U, 210U, 255U};
    run_case(cli, dir, "rgba", ".png", png, png_pixels, 2U, 1U);

    const auto jpeg = make_grayscale_jpeg();
    const std::array<std::uint8_t, 4U> jpeg_pixels{128U, 128U, 128U, 255U};
    run_case(cli, dir, "gray", ".jpg", jpeg, jpeg_pixels, 1U, 1U);

    const auto tiff = make_rgba_tiff();
    const std::array<std::uint8_t, 8U> tiff_pixels{64U, 32U, 16U, 128U, 10U, 20U, 30U, 255U};
    run_case(cli, dir, "rgba", ".tiff", tiff, tiff_pixels, 2U, 1U);

    // Every supported format, driven end to end through the real CLI with
    // inputs produced by reference encoders.
    using namespace vektoryum_test_fixtures;

    run_lossless_case(cli, dir, "real_alpha", ".webp", webp_alpha_fade, 24U, 16U,
                      [](std::uint32_t x, std::uint32_t y) {
                          return std::array<std::uint8_t, 4U>{
                              byte_of(x * 9U), byte_of(y * 17U), byte_of((x + y) * 5U),
                              byte_of((x * 255U) / 23U)};
                      });
    run_lossless_case(cli, dir, "real_palette", ".webp", webp_two_color, 40U, 8U,
                      [](std::uint32_t x, std::uint32_t y) {
                          return ((x ^ y) & 1U) == 0U
                                     ? std::array<std::uint8_t, 4U>{255U, 0U, 0U, 255U}
                                     : std::array<std::uint8_t, 4U>{0U, 0U, 255U, 128U};
                      });
    run_lossless_case(cli, dir, "real_gray", ".tiff", tiff_gray_13x9, 13U, 9U,
                      [](std::uint32_t x, std::uint32_t y) {
                          const std::uint8_t v = byte_of(x * 11U + y * 7U);
                          return std::array<std::uint8_t, 4U>{v, v, v, 255U};
                      });
    run_lossless_case(cli, dir, "real_rgb", ".tiff", tiff_rgb_13x9, 13U, 9U,
                      [](std::uint32_t x, std::uint32_t y) {
                          return std::array<std::uint8_t, 4U>{
                              byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U), 255U};
                      });
    run_lossless_case(cli, dir, "real_rgba", ".tiff", tiff_rgba_13x9, 13U, 9U,
                      [](std::uint32_t x, std::uint32_t y) {
                          return std::array<std::uint8_t, 4U>{
                              byte_of(x * 9U), byte_of(y * 13U), byte_of((x + y) * 5U),
                              byte_of((x * 255U) / 12U)};
                      });

    const auto gray_ramp = [](std::uint32_t x, std::uint32_t y) {
        const auto v = static_cast<std::uint8_t>(std::min<std::uint32_t>(16U + x * 6U + y * 4U, 255U));
        return std::array<std::uint8_t, 4U>{v, v, v, 255U};
    };
    const auto smooth_color = [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            static_cast<std::uint8_t>(std::min<std::uint32_t>(40U + x * 5U, 255U)),
            static_cast<std::uint8_t>(std::min<std::uint32_t>(30U + y * 7U, 255U)),
            static_cast<std::uint8_t>(std::min<std::uint32_t>(20U + (x + y) * 3U, 255U)),
            255U};
    };
    run_lossy_case(cli, dir, "real_gray", ".jpg", jpeg_gray_ramp_grayscale, 24U, 16U, 60.0, 0, gray_ramp);
    run_lossy_case(cli, dir, "real_444", ".jpg", jpeg_smooth_color_444, 32U, 24U, 46.0, 5, smooth_color);
    run_lossy_case(cli, dir, "real_422", ".jpg", jpeg_smooth_color_422, 32U, 24U, 42.0, 6, smooth_color);
    run_lossy_case(cli, dir, "real_420", ".jpg", jpeg_smooth_color_420, 32U, 24U, 39.0, 8, smooth_color);

    // Inputs the pipeline cannot honour must exit non-zero and write nothing.
    {
        std::vector<std::uint8_t> truncated(png.begin(), png.end() - 6);
        run_rejected_case(cli, dir, "truncated", ".png", truncated,
                          "CLI --convert must reject a truncated PNG");
    }
    {
        const std::string text = "this is not an image";
        const std::vector<std::uint8_t> bytes(text.begin(), text.end());
        run_rejected_case(cli, dir, "not_an_image", ".png", bytes,
                          "CLI --convert must reject an unrecognised format");
    }
    {
        const std::vector<std::uint8_t> lossy{
            'R', 'I', 'F', 'F', 0x14U, 0x00U, 0x00U, 0x00U, 'W', 'E', 'B', 'P',
            'V', 'P', '8', ' ', 0x08U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x9dU, 0x01U, 0x2aU, 0x01U, 0x00U};
        run_rejected_case(cli, dir, "lossy", ".webp", lossy,
                          "CLI --convert must reject a lossy WebP rather than guess at it");
    }
    {
        const auto missing = dir / "does_not_exist.png";
        const auto output_path = dir / "missing.pam";
        const std::string command = quote(cli) + " --convert " + quote(missing) + " " + quote(output_path);
        require(run_command(command) != 0, "CLI --convert must reject a missing input file");
    }

    std::filesystem::remove_all(dir, ec);
    std::cout << "R2 CLI convert matrix fixtures passed\n";
    return 0;
}
