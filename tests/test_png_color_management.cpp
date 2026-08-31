#include "vektoryum/io/raster_decode.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
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

void append_common_rgb_body(std::vector<std::uint8_t>& png) {
    // zlib stream containing one unfiltered RGB scanline: {filter=0, R=10, G=20, B=30}.
    const std::array<std::uint8_t, 15U> zlib{
        0x78U, 0x01U, 0x01U, 0x04U, 0x00U, 0xfbU, 0xffU, 0x00U,
        0x0aU, 0x14U, 0x1eU, 0x00U, 0x68U, 0x00U, 0x3dU};
    append_png_chunk(png, {0x49U, 0x44U, 0x41U, 0x54U}, zlib);
    append_png_chunk(png, {0x49U, 0x45U, 0x4eU, 0x44U}, std::span<const std::uint8_t>{});
}

[[nodiscard]] std::vector<std::uint8_t> make_rgb_png_with_srgb(std::uint8_t rendering_intent) {
    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};

    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, 1U);
    append_u32_be(ihdr, 1U);
    ihdr.insert(ihdr.end(), {8U, 2U, 0U, 0U, 0U});
    append_png_chunk(png, {0x49U, 0x48U, 0x44U, 0x52U}, ihdr);

    const std::array<std::uint8_t, 1U> srgb{rendering_intent};
    append_png_chunk(png, {0x73U, 0x52U, 0x47U, 0x42U}, srgb);
    append_common_rgb_body(png);
    return png;
}

[[nodiscard]] std::vector<std::uint8_t> make_rgb_png_with_gamma(std::uint32_t gamma_times_100000) {
    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};

    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, 1U);
    append_u32_be(ihdr, 1U);
    ihdr.insert(ihdr.end(), {8U, 2U, 0U, 0U, 0U});
    append_png_chunk(png, {0x49U, 0x48U, 0x44U, 0x52U}, ihdr);

    std::vector<std::uint8_t> gamma;
    append_u32_be(gamma, gamma_times_100000);
    append_png_chunk(png, {0x67U, 0x41U, 0x4dU, 0x41U}, gamma);
    append_common_rgb_body(png);
    return png;
}

}  // namespace

int main() {
    using namespace vektoryum;

    const auto valid = make_rgb_png_with_srgb(0U);
    const io::RasterDecodeResult decoded = io::decode_raster(io::RasterFormat::Png, valid);
    require(decoded.ok(), "PNG with valid sRGB perceptual rendering intent must decode");
    require(decoded.image.spec.width == 1U && decoded.image.spec.height == 1U,
            "sRGB PNG dimensions must match IHDR");
    require(decoded.image.rgba8 == std::vector<std::uint8_t>({10U, 20U, 30U, 255U}),
            "sRGB PNG must preserve canonical RGBA8 samples");
    require(decoded.image.spec.transfer == core::TransferFunction::SRGB,
            "sRGB PNG must normalize to the canonical sRGB transfer function");
    require(decoded.image.spec.primaries == core::ColorPrimaries::SRGB,
            "sRGB PNG must normalize to canonical sRGB primaries");

    const auto invalid = make_rgb_png_with_srgb(4U);
    const io::RasterDecodeResult rejected = io::decode_raster(io::RasterFormat::Png, invalid);
    require(!rejected.ok(), "PNG sRGB rendering intent outside 0..3 must fail closed");

    const auto canonical_gamma = make_rgb_png_with_gamma(45455U);
    const io::RasterDecodeResult gamma_decoded = io::decode_raster(io::RasterFormat::Png, canonical_gamma);
    require(gamma_decoded.ok(), "PNG with canonical sRGB-compatible gAMA must decode");
    require(gamma_decoded.image.rgba8 == std::vector<std::uint8_t>({10U, 20U, 30U, 255U}),
            "canonical gAMA PNG must preserve canonical RGBA8 samples");
    require(gamma_decoded.image.spec.transfer == core::TransferFunction::SRGB,
            "canonical gAMA PNG must normalize to sRGB transfer");

    const auto noncanonical_gamma = make_rgb_png_with_gamma(100000U);
    const io::RasterDecodeResult gamma_rejected = io::decode_raster(io::RasterFormat::Png, noncanonical_gamma);
    require(!gamma_rejected.ok(),
            "PNG with non-sRGB gAMA must fail closed until explicit transfer conversion is implemented");

    return 0;
}
