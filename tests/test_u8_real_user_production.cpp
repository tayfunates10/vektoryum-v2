#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

[[nodiscard]] std::vector<std::uint8_t> make_rgba_png(
    std::uint32_t width,
    std::uint32_t height,
    std::span<const std::uint8_t> rgba8) {
    require(rgba8.size() == static_cast<std::size_t>(width) * height * 4U,
            "PNG fixture size must match RGBA dimensions");

    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};
    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, width);
    append_u32_be(ihdr, height);
    ihdr.insert(ihdr.end(), {8U, 6U, 0U, 0U, 0U});
    append_png_chunk(png, {0x49U, 0x48U, 0x44U, 0x52U}, ihdr);

    std::vector<std::uint8_t> filtered;
    filtered.reserve(static_cast<std::size_t>(height) * (1U + static_cast<std::size_t>(width) * 4U));
    for (std::uint32_t y = 0U; y < height; ++y) {
        filtered.push_back(0U);
        const std::size_t row = static_cast<std::size_t>(y) * width * 4U;
        filtered.insert(filtered.end(), rgba8.begin() + static_cast<std::ptrdiff_t>(row),
                        rgba8.begin() + static_cast<std::ptrdiff_t>(row + width * 4U));
    }
    require(filtered.size() <= 65535U, "stored DEFLATE fixture must fit one block");

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

[[nodiscard]] std::vector<std::uint8_t> make_canvas(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4U, 255U);
    return rgba;
}

void set_pixel(
    std::vector<std::uint8_t>& rgba,
    std::uint32_t width,
    std::uint32_t x,
    std::uint32_t y,
    std::array<std::uint8_t, 3U> rgb) {
    const std::size_t base = (static_cast<std::size_t>(y) * width + x) * 4U;
    rgba[base] = rgb[0];
    rgba[base + 1U] = rgb[1];
    rgba[base + 2U] = rgb[2];
    rgba[base + 3U] = 255U;
}

void fill_rect(
    std::vector<std::uint8_t>& rgba,
    std::uint32_t width,
    std::uint32_t x0,
    std::uint32_t y0,
    std::uint32_t x1,
    std::uint32_t y1,
    std::array<std::uint8_t, 3U> rgb) {
    for (std::uint32_t y = y0; y < y1; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            set_pixel(rgba, width, x, y, rgb);
        }
    }
}

void write_binary(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "fixture file must open");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "fixture file must write completely");
}

[[nodiscard]] std::vector<std::uint8_t> read_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(static_cast<bool>(in), "artifact must exist");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string quote(const std::filesystem::path& path) {
    return std::string{"\""} + path.string() + "\"";
}

[[nodiscard]] int run_command(const std::string& command) {
#ifdef _WIN32
    return std::system((std::string{"\""} + command + "\"").c_str());
#else
    return std::system(command.c_str());
#endif
}

void run_fixture(
    const std::filesystem::path& cli,
    const std::filesystem::path& dir,
    std::string_view name,
    std::uint32_t width,
    std::uint32_t height,
    const std::vector<std::uint8_t>& rgba) {
    const auto input = dir / (std::string(name) + ".png");
    const auto output = dir / (std::string(name) + ".svg");
    const auto certificate = std::filesystem::path(output.string() + ".quality-certificate");
    write_binary(input, make_rgba_png(width, height, rgba));

    const std::string command = quote(cli) + " --certified-convert " + quote(input) + " " + quote(output) + " svg";
    require(run_command(command) == 0, "U8 real-user production fixture must pass certified SVG conversion");
    require(std::filesystem::exists(output), "U8 production fixture must emit SVG");
    require(std::filesystem::exists(certificate), "U8 production fixture must emit quality certificate");

    const auto svg_a = read_binary(output);
    const auto cert_a = read_binary(certificate);
    require(!svg_a.empty() && !cert_a.empty(), "U8 production artifacts must be non-empty");
    const std::string svg_text(svg_a.begin(), svg_a.end());
    require(svg_text.find("<svg") != std::string::npos && svg_text.find("<path") != std::string::npos,
            "U8 production output must contain serialized SVG geometry");

    std::filesystem::remove(output);
    std::filesystem::remove(certificate);
    require(run_command(command) == 0, "U8 production fixture repeat must pass");
    require(read_binary(output) == svg_a, "U8 production SVG must be deterministic");
    require(read_binary(certificate) == cert_a, "U8 production certificate must be deterministic");
}

}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "test requires path to vektoryum_cli");
    const std::filesystem::path cli = argv[1];
    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-u8-real-user-production";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    require(std::filesystem::create_directories(dir, ec), "U8 fixture directory must be created");

    constexpr std::uint32_t width = 32U;
    constexpr std::uint32_t height = 32U;
    constexpr std::array<std::uint8_t, 3U> black{0U, 0U, 0U};
    constexpr std::array<std::uint8_t, 3U> red{180U, 20U, 20U};
    constexpr std::array<std::uint8_t, 3U> blue{20U, 40U, 160U};

    auto logo_block = make_canvas(width, height);
    fill_rect(logo_block, width, 6U, 6U, 26U, 26U, black);
    run_fixture(cli, dir, "real_user_01_logo_block", width, height, logo_block);

    auto logo_hole = make_canvas(width, height);
    fill_rect(logo_hole, width, 5U, 5U, 27U, 27U, black);
    fill_rect(logo_hole, width, 11U, 11U, 21U, 21U, {255U, 255U, 255U});
    run_fixture(cli, dir, "real_user_02_logo_hole", width, height, logo_hole);

    auto wordmark = make_canvas(width, height);
    fill_rect(wordmark, width, 3U, 9U, 8U, 23U, black);
    fill_rect(wordmark, width, 11U, 9U, 16U, 23U, black);
    fill_rect(wordmark, width, 19U, 9U, 29U, 14U, black);
    fill_rect(wordmark, width, 19U, 18U, 29U, 23U, black);
    run_fixture(cli, dir, "real_user_03_wordmark", width, height, wordmark);

    auto badge = make_canvas(width, height);
    fill_rect(badge, width, 6U, 6U, 26U, 26U, red);
    fill_rect(badge, width, 10U, 10U, 22U, 22U, {255U, 255U, 255U});
    fill_rect(badge, width, 13U, 13U, 19U, 19U, red);
    run_fixture(cli, dir, "real_user_04_badge", width, height, badge);

    auto icon_cross = make_canvas(width, height);
    fill_rect(icon_cross, width, 13U, 5U, 19U, 27U, blue);
    fill_rect(icon_cross, width, 5U, 13U, 27U, 19U, blue);
    run_fixture(cli, dir, "real_user_05_icon_cross", width, height, icon_cross);

    auto two_color = make_canvas(width, height);
    fill_rect(two_color, width, 4U, 6U, 15U, 26U, red);
    fill_rect(two_color, width, 17U, 6U, 28U, 26U, blue);
    run_fixture(cli, dir, "real_user_06_two_color_mark", width, height, two_color);

    auto separated = make_canvas(width, height);
    fill_rect(separated, width, 4U, 5U, 12U, 13U, black);
    fill_rect(separated, width, 20U, 5U, 28U, 13U, black);
    fill_rect(separated, width, 4U, 19U, 12U, 27U, black);
    fill_rect(separated, width, 20U, 19U, 28U, 27U, black);
    run_fixture(cli, dir, "real_user_07_separated_marks", width, height, separated);

    auto stepped = make_canvas(width, height);
    fill_rect(stepped, width, 5U, 20U, 11U, 27U, black);
    fill_rect(stepped, width, 11U, 15U, 17U, 27U, black);
    fill_rect(stepped, width, 17U, 10U, 23U, 27U, black);
    fill_rect(stepped, width, 23U, 5U, 28U, 27U, black);
    run_fixture(cli, dir, "real_user_08_stepped_symbol", width, height, stepped);

    std::filesystem::remove_all(dir, ec);
    std::cout << "U8 eight-fixture production regression pack passed\n";
    return 0;
}
