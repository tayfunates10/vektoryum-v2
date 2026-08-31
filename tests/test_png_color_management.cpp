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

struct Chunk {
    std::array<std::uint8_t, 4U> type{};
    std::vector<std::uint8_t> data{};
};

constexpr std::array<std::uint8_t, 4U> srgb_type{0x73U, 0x52U, 0x47U, 0x42U};
constexpr std::array<std::uint8_t, 4U> gama_type{0x67U, 0x41U, 0x4dU, 0x41U};
constexpr std::array<std::uint8_t, 4U> chrm_type{0x63U, 0x48U, 0x52U, 0x4dU};
constexpr std::array<std::uint8_t, 4U> iccp_type{0x69U, 0x43U, 0x43U, 0x50U};
constexpr std::array<std::uint8_t, 4U> ihdr_type{0x49U, 0x48U, 0x44U, 0x52U};
constexpr std::array<std::uint8_t, 4U> idat_type{0x49U, 0x44U, 0x41U, 0x54U};
constexpr std::array<std::uint8_t, 4U> iend_type{0x49U, 0x45U, 0x4eU, 0x44U};

// One 1x1 RGB PNG carrying the colour-space chunks under test. Chunks listed in
// `before_idat` precede the image data; chunks in `after_idat` follow it, which
// lets the test cover the ordering rule for colour-space chunks.
[[nodiscard]] std::vector<std::uint8_t> make_rgb_png(
    const std::vector<Chunk>& before_idat,
    const std::vector<Chunk>& after_idat = {}) {
    std::vector<std::uint8_t> png{0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU};

    std::vector<std::uint8_t> ihdr;
    append_u32_be(ihdr, 1U);
    append_u32_be(ihdr, 1U);
    ihdr.insert(ihdr.end(), {8U, 2U, 0U, 0U, 0U});
    append_png_chunk(png, ihdr_type, ihdr);

    for (const Chunk& chunk : before_idat) {
        append_png_chunk(png, chunk.type, chunk.data);
    }

    // zlib stream containing one unfiltered RGB scanline: {filter=0, R=10, G=20, B=30}.
    const std::array<std::uint8_t, 15U> zlib{
        0x78U, 0x01U, 0x01U, 0x04U, 0x00U, 0xfbU, 0xffU, 0x00U,
        0x0aU, 0x14U, 0x1eU, 0x00U, 0x68U, 0x00U, 0x3dU};
    append_png_chunk(png, idat_type, zlib);

    for (const Chunk& chunk : after_idat) {
        append_png_chunk(png, chunk.type, chunk.data);
    }

    append_png_chunk(png, iend_type, std::span<const std::uint8_t>{});
    return png;
}

[[nodiscard]] Chunk srgb_chunk(std::uint8_t rendering_intent) {
    return Chunk{srgb_type, std::vector<std::uint8_t>{rendering_intent}};
}

[[nodiscard]] Chunk gama_chunk(std::uint32_t gamma_times_100000) {
    std::vector<std::uint8_t> data;
    append_u32_be(data, gamma_times_100000);
    return Chunk{gama_type, std::move(data)};
}

[[nodiscard]] Chunk chrm_chunk(const std::array<std::uint32_t, 8U>& chromaticities) {
    std::vector<std::uint8_t> data;
    for (const std::uint32_t value : chromaticities) {
        append_u32_be(data, value);
    }
    return Chunk{chrm_type, std::move(data)};
}

[[nodiscard]] bool decodes(const std::vector<std::uint8_t>& png) {
    return vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Png, png).ok();
}

constexpr std::array<std::uint32_t, 8U> srgb_chromaticities{
    31270U, 32900U, 64000U, 33000U, 30000U, 60000U, 15000U, 6000U};

// Adobe RGB (1998) primaries: a different colour space this decoder must not
// silently relabel as sRGB.
constexpr std::array<std::uint32_t, 8U> adobe_rgb_chromaticities{
    31270U, 32900U, 64000U, 33000U, 21000U, 71000U, 15000U, 6000U};

}  // namespace

int main() {
    using namespace vektoryum;

    const auto valid = make_rgb_png({srgb_chunk(0U)});
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

    for (std::uint8_t intent = 0U; intent < 4U; ++intent) {
        require(decodes(make_rgb_png({srgb_chunk(intent)})),
                "every PNG sRGB rendering intent in 0..3 must decode");
    }
    require(!decodes(make_rgb_png({srgb_chunk(4U)})),
            "PNG sRGB rendering intent outside 0..3 must fail closed");
    require(!decodes(make_rgb_png({Chunk{srgb_type, {0U, 0U}}})),
            "PNG sRGB chunk with a non-unit length must fail closed");
    require(!decodes(make_rgb_png({srgb_chunk(0U), srgb_chunk(0U)})),
            "duplicate PNG sRGB chunks must fail closed");

    const auto canonical_gamma = make_rgb_png({gama_chunk(45455U)});
    const io::RasterDecodeResult gamma_decoded = io::decode_raster(io::RasterFormat::Png, canonical_gamma);
    require(gamma_decoded.ok(), "PNG with canonical sRGB-compatible gAMA must decode");
    require(gamma_decoded.image.rgba8 == std::vector<std::uint8_t>({10U, 20U, 30U, 255U}),
            "canonical gAMA PNG must preserve canonical RGBA8 samples");
    require(gamma_decoded.image.spec.transfer == core::TransferFunction::SRGB,
            "canonical gAMA PNG must normalize to sRGB transfer");

    require(!decodes(make_rgb_png({gama_chunk(100000U)})),
            "PNG with non-sRGB gAMA must fail closed until explicit transfer conversion is implemented");
    require(!decodes(make_rgb_png({gama_chunk(50000U)})),
            "PNG with a gamma 2.0 gAMA must fail closed instead of being relabelled sRGB");
    require(!decodes(make_rgb_png({gama_chunk(45000U)})),
            "PNG with a near-sRGB but non-canonical gAMA must still fail closed");
    require(!decodes(make_rgb_png({Chunk{gama_type, {0U, 0U, 0U}}})),
            "PNG gAMA chunk with a truncated length must fail closed");
    require(!decodes(make_rgb_png({gama_chunk(45455U), gama_chunk(45455U)})),
            "duplicate PNG gAMA chunks must fail closed");

    require(decodes(make_rgb_png({srgb_chunk(0U), gama_chunk(45455U)})),
            "PNG carrying sRGB plus the matching canonical gAMA must decode");
    require(!decodes(make_rgb_png({srgb_chunk(0U), gama_chunk(100000U)})),
            "PNG whose gAMA contradicts its sRGB chunk must fail closed");

    require(decodes(make_rgb_png({chrm_chunk(srgb_chromaticities)})),
            "PNG cHRM carrying the sRGB chromaticities must decode");
    require(!decodes(make_rgb_png({chrm_chunk(adobe_rgb_chromaticities)})),
            "PNG cHRM carrying non-sRGB primaries must fail closed");
    require(!decodes(make_rgb_png({Chunk{chrm_type, std::vector<std::uint8_t>(16U, 0U)}})),
            "PNG cHRM chunk with a truncated length must fail closed");

    require(!decodes(make_rgb_png({Chunk{iccp_type, {0x73U, 0x00U, 0x00U, 0x78U, 0x01U, 0x01U}}})),
            "PNG carrying an embedded ICC profile must fail closed rather than assume sRGB");

    require(!decodes(make_rgb_png({}, {gama_chunk(45455U)})),
            "PNG colour-space chunks after IDAT must fail closed");

    require(decodes(make_rgb_png({})),
            "PNG without colour-space chunks must decode as canonical sRGB");

    return 0;
}
