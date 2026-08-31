#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    require(std::system(command.c_str()) == 0, "CLI --convert must succeed for accepted raster fixture");
    const auto actual = read_binary(output_path);
    const std::string header = "P7\nWIDTH " + std::to_string(width) + "\nHEIGHT " + std::to_string(height) +
                               "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    require(actual.size() == header.size() + expected_pixels.size(), "PAM output size must match canonical header plus RGBA8 pixels");
    require(std::equal(header.begin(), header.end(), actual.begin()), "PAM output header must be canonical and deterministic");
    require(std::equal(expected_pixels.begin(), expected_pixels.end(), actual.begin() + static_cast<std::ptrdiff_t>(header.size())),
            "PAM output pixels must match canonical decoded RGBA8 bytes");
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

    std::filesystem::remove_all(dir, ec);
    std::cout << "R2 CLI convert matrix fixtures passed\n";
    return 0;
}
