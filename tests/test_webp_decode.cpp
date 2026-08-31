#include "vektoryum/io/raster_decode.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
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
    // Real 1x1 lossless WebP (VP8L) with one RGBA pixel: (12, 34, 56, 78).
    constexpr std::array<std::uint8_t, 38U> webp{{
        0x52U, 0x49U, 0x46U, 0x46U, 0x1eU, 0x00U, 0x00U, 0x00U,
        0x57U, 0x45U, 0x42U, 0x50U, 0x56U, 0x50U, 0x38U, 0x4cU,
        0x11U, 0x00U, 0x00U, 0x00U, 0x2fU, 0x00U, 0x00U, 0x00U,
        0x10U, 0x07U, 0x50U, 0x91U, 0x32U, 0x14U, 0xa7U, 0x4eU,
        0x81U, 0x88U, 0xe8U, 0x7fU, 0x00U, 0x00U,
    }};

    const auto first = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, webp);
    if (!expect(first.ok(), "real VP8L WebP must decode")) {
        return 1;
    }
    if (!expect(first.image.spec.width == 1U && first.image.spec.height == 1U,
                "WebP dimensions must be preserved")) {
        return 1;
    }
    if (!expect(first.image.rgba8.size() == 4U,
                "1x1 WebP must normalize to exactly one RGBA8 pixel")) {
        return 1;
    }
    if (!expect(first.image.rgba8[0U] == 12U && first.image.rgba8[1U] == 34U &&
                    first.image.rgba8[2U] == 56U && first.image.rgba8[3U] == 78U,
                "WebP RGBA pixel must preserve color and straight alpha semantics")) {
        return 1;
    }

    const auto second = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, webp);
    if (!expect(second.ok() && second.image.rgba8 == first.image.rgba8,
                "WebP decode must be deterministic")) {
        return 1;
    }

    if (!expect(argc >= 1, "test executable path must be available")) {
        return 1;
    }
    std::filesystem::path cli = std::filesystem::path(argv[0]).parent_path() / "vektoryum_cli";
#ifdef _WIN32
    cli += ".exe";
#endif
    if (!expect(std::filesystem::exists(cli), "CLI executable must exist beside WebP acceptance test")) {
        return 1;
    }

    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-r2-webp-cli";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    if (!expect(std::filesystem::create_directories(dir, ec), "temporary WebP CLI directory must be created")) {
        return 1;
    }
    const auto input_path = dir / "alpha.webp";
    const auto output_path = dir / "alpha.pam";
    {
        std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
        if (!expect(static_cast<bool>(out), "WebP CLI fixture must open")) {
            return 1;
        }
        out.write(reinterpret_cast<const char*>(webp.data()), static_cast<std::streamsize>(webp.size()));
        if (!expect(static_cast<bool>(out), "WebP CLI fixture must write completely")) {
            return 1;
        }
    }

    const std::string command = quote(cli) + " --convert " + quote(input_path) + " " + quote(output_path);
    if (!expect(run_command(command) == 0, "CLI --convert must accept real WebP")) {
        return 1;
    }
    std::ifstream in(output_path, std::ios::binary);
    if (!expect(static_cast<bool>(in), "WebP CLI PAM output must exist")) {
        return 1;
    }
    const std::vector<std::uint8_t> actual(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
    const std::string header = "P7\nWIDTH 1\nHEIGHT 1\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    if (!expect(actual.size() == header.size() + 4U, "WebP CLI PAM size must be canonical")) {
        return 1;
    }
    if (!expect(std::equal(header.begin(), header.end(), actual.begin()), "WebP CLI PAM header must be canonical")) {
        return 1;
    }
    if (!expect(actual[header.size()] == 12U && actual[header.size() + 1U] == 34U &&
                    actual[header.size() + 2U] == 56U && actual[header.size() + 3U] == 78U,
                "WebP CLI PAM pixels must preserve RGBA and alpha")) {
        return 1;
    }

    std::filesystem::remove_all(dir, ec);
    return 0;
}
