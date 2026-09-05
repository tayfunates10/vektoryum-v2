#include <array>
#include <cstddef>
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

[[nodiscard]] std::vector<std::uint8_t> make_vector_logo_tiff() {
    constexpr std::uint32_t width = 24U;
    constexpr std::uint32_t height = 24U;
    constexpr std::uint16_t entry_count = 10U;
    constexpr std::size_t ifd_offset = 8U;
    constexpr std::size_t entries_start = ifd_offset + 2U;
    constexpr std::size_t ifd_end = entries_start + static_cast<std::size_t>(entry_count) * 12U + 4U;
    constexpr std::size_t bits_offset = ifd_end;
    constexpr std::size_t pixels_offset = bits_offset + 8U;
    constexpr std::size_t pixel_bytes = static_cast<std::size_t>(width) * height * 4U;

    std::vector<std::uint8_t> bytes(pixels_offset + pixel_bytes, 0U);
    bytes[0U] = 0x49U;
    bytes[1U] = 0x49U;
    write_u16_le(bytes, 2U, 42U);
    write_u32_le(bytes, 4U, static_cast<std::uint32_t>(ifd_offset));
    write_u16_le(bytes, ifd_offset, entry_count);

    std::size_t entry = entries_start;
    write_ifd_entry(bytes, entry, 256U, 4U, 1U, width); entry += 12U;
    write_ifd_entry(bytes, entry, 257U, 4U, 1U, height); entry += 12U;
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

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::int32_t dx = static_cast<std::int32_t>(x) - 12;
            const std::int32_t dy = static_cast<std::int32_t>(y) - 12;
            const bool foreground = (dx * dx + dy * dy) <= 64;
            const std::size_t pixel = (static_cast<std::size_t>(y) * width + x) * 4U;
            bytes[pixels_offset + pixel] = 32U;
            bytes[pixels_offset + pixel + 1U] = 96U;
            bytes[pixels_offset + pixel + 2U] = 224U;
            bytes[pixels_offset + pixel + 3U] = foreground ? 255U : 0U;
        }
    }
    return bytes;
}

void write_binary(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "fixture output must open");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "fixture output must write completely");
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(static_cast<bool>(in), "production SVG output must exist");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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

}  // namespace

int main(int argc, char** argv) {
    require(argc == 2, "test requires path to vektoryum_cli");
    const std::filesystem::path cli = argv[1];
    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-u7-cli-cubic";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    require(std::filesystem::create_directories(dir, ec), "temporary fixture directory must be created");

    const auto input_path = dir / "vector-logo.tiff";
    const auto output_path = dir / "vector-logo.svg";
    const auto fixture = make_vector_logo_tiff();
    write_binary(input_path, fixture);

    const std::string command = quote(cli) + " --certified-convert " +
                                quote(input_path) + " " + quote(output_path) + " svg";
    require(run_command(command) == 0,
            "real certified CLI production path must accept the vector-routed logo fixture");

    const std::string svg = read_text(output_path);
    require(svg.find("<path") != std::string::npos,
            "production SVG must contain reconstructed path geometry");
    require(svg.find(" C") != std::string::npos,
            "production SVG must contain a certified cubic command");

    const auto certificate_path = std::filesystem::path(output_path.string() + ".quality-certificate");
    require(std::filesystem::exists(certificate_path),
            "production cubic output must retain the quality certificate boundary");

    std::filesystem::remove_all(dir, ec);
    std::cout << "U7 production CLI cubic regression passed\n";
    return 0;
}
