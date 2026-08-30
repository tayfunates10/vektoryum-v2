#include "vektoryum/io/raster_input.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test failure: " << message << '\n';
        std::exit(1);
    }
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "fixture file must open");
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "fixture file must write");
}

}  // namespace

int main() {
    using vektoryum::io::RasterFormat;
    using vektoryum::io::RasterInputError;
    using vektoryum::io::detect_raster_format;
    using vektoryum::io::load_raster_input;

    const std::array<std::uint8_t, 8U> png{{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU}};
    const std::array<std::uint8_t, 4U> jpeg{{0xffU, 0xd8U, 0xffU, 0xe0U}};
    const std::array<std::uint8_t, 12U> webp{{0x52U, 0x49U, 0x46U, 0x46U, 0x04U, 0x00U, 0x00U, 0x00U, 0x57U, 0x45U, 0x42U, 0x50U}};
    const std::array<std::uint8_t, 4U> tiff_le{{0x49U, 0x49U, 0x2aU, 0x00U}};
    const std::array<std::uint8_t, 4U> tiff_be{{0x4dU, 0x4dU, 0x00U, 0x2aU}};

    require(detect_raster_format(png) == RasterFormat::Png, "PNG magic must be detected");
    require(detect_raster_format(jpeg) == RasterFormat::Jpeg, "JPEG magic must be detected");
    require(detect_raster_format(webp) == RasterFormat::Webp, "WebP RIFF magic must be detected");
    require(detect_raster_format(tiff_le) == RasterFormat::Tiff, "little-endian TIFF magic must be detected");
    require(detect_raster_format(tiff_be) == RasterFormat::Tiff, "big-endian TIFF magic must be detected");

    const std::array<std::uint8_t, 4U> hostile{{0x4dU, 0x5aU, 0x90U, 0x00U}};
    require(detect_raster_format(hostile) == RasterFormat::Unknown, "non-raster payload must fail closed");

    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto good_path = temp_dir / "vektoryum-r2-input.png";
    const auto bad_path = temp_dir / "vektoryum-r2-input.bin";
    write_bytes(good_path, std::vector<std::uint8_t>(png.begin(), png.end()));
    write_bytes(bad_path, std::vector<std::uint8_t>(hostile.begin(), hostile.end()));

    const auto good = load_raster_input(good_path);
    require(good.ok(), "recognized raster file must load");
    require(good.input.format == RasterFormat::Png, "loaded format must match content");
    require(good.input.bytes.size() == png.size(), "loaded bytes must be exact");

    const auto too_small_budget = load_raster_input(good_path, png.size() - 1U);
    require(too_small_budget.error == RasterInputError::TooLarge, "input budget must fail closed");

    const auto unsupported = load_raster_input(bad_path);
    require(unsupported.error == RasterInputError::UnsupportedFormat, "unsupported content must fail closed");

    const auto missing = load_raster_input(temp_dir / "vektoryum-r2-does-not-exist.png");
    require(missing.error == RasterInputError::OpenFailed, "missing file must fail closed");

    std::filesystem::remove(good_path);
    std::filesystem::remove(bad_path);
    return 0;
}
