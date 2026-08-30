#include "vektoryum/io/raster_decode.hpp"

#include <cstddef>
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

[[nodiscard]] std::vector<std::uint8_t> make_rgba_tiff(std::uint16_t compression, std::uint16_t extra_sample) {
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
    write_ifd_entry(bytes, entry, 259U, 3U, 1U, compression); entry += 12U;
    write_ifd_entry(bytes, entry, 262U, 3U, 1U, 2U); entry += 12U;
    write_ifd_entry(bytes, entry, 273U, 4U, 1U, static_cast<std::uint32_t>(pixels_offset)); entry += 12U;
    write_ifd_entry(bytes, entry, 277U, 3U, 1U, 4U); entry += 12U;
    write_ifd_entry(bytes, entry, 279U, 4U, 1U, static_cast<std::uint32_t>(pixel_bytes)); entry += 12U;
    write_ifd_entry(bytes, entry, 284U, 3U, 1U, 1U); entry += 12U;
    write_ifd_entry(bytes, entry, 338U, 3U, 1U, extra_sample);

    write_u16_le(bytes, bits_offset, 8U);
    write_u16_le(bytes, bits_offset + 2U, 8U);
    write_u16_le(bytes, bits_offset + 4U, 8U);
    write_u16_le(bytes, bits_offset + 6U, 8U);

    bytes[pixels_offset] = 64U;
    bytes[pixels_offset + 1U] = 32U;
    bytes[pixels_offset + 2U] = 16U;
    bytes[pixels_offset + 3U] = 128U;
    bytes[pixels_offset + 4U] = 10U;
    bytes[pixels_offset + 5U] = 20U;
    bytes[pixels_offset + 6U] = 30U;
    bytes[pixels_offset + 7U] = 255U;
    return bytes;
}

}  // namespace

int main() {
    using namespace vektoryum;

    const auto associated_bytes = make_rgba_tiff(1U, 1U);
    const io::RasterDecodeResult associated = io::decode_raster(io::RasterFormat::Tiff, associated_bytes);
    require(associated.ok(), "baseline uncompressed TIFF must decode");
    require(associated.image.spec.width == 2U && associated.image.spec.height == 1U, "decoded dimensions must match TIFF");
    require(associated.image.spec.layout == core::PixelLayout::RGBA, "decoded layout must be canonical RGBA");
    require(associated.image.spec.channel_type == core::ChannelType::UInt8, "decoded channels must be UInt8");
    require(associated.image.spec.transfer == core::TransferFunction::SRGB, "decoded transfer must be canonical sRGB");
    require(associated.image.spec.alpha == core::AlphaMode::Straight, "decoded alpha must be normalized to straight");
    require(associated.image.rgba8.size() == 8U, "2x1 canonical RGBA8 must contain 8 bytes");
    require(associated.image.rgba8[0U] == 128U, "associated red must be unpremultiplied deterministically");
    require(associated.image.rgba8[1U] == 64U, "associated green must be unpremultiplied deterministically");
    require(associated.image.rgba8[2U] == 32U, "associated blue must be unpremultiplied deterministically");
    require(associated.image.rgba8[3U] == 128U, "alpha must be preserved");
    require(associated.image.rgba8[4U] == 10U && associated.image.rgba8[5U] == 20U &&
            associated.image.rgba8[6U] == 30U && associated.image.rgba8[7U] == 255U,
            "opaque pixel must be preserved");

    const auto straight_bytes = make_rgba_tiff(1U, 2U);
    const io::RasterDecodeResult straight = io::decode_raster(io::RasterFormat::Tiff, straight_bytes);
    require(straight.ok(), "unassociated-alpha TIFF must decode");
    require(straight.image.rgba8[0U] == 64U && straight.image.rgba8[1U] == 32U &&
            straight.image.rgba8[2U] == 16U && straight.image.rgba8[3U] == 128U,
            "straight alpha color bytes must not be altered");

    const auto compressed_bytes = make_rgba_tiff(5U, 2U);
    require(io::decode_raster(io::RasterFormat::Tiff, compressed_bytes).error == io::RasterDecodeError::UnsupportedFeature,
            "unsupported TIFF compression must fail closed");

    auto truncated = make_rgba_tiff(1U, 2U);
    truncated.pop_back();
    require(io::decode_raster(io::RasterFormat::Tiff, truncated).error == io::RasterDecodeError::TruncatedPixelData,
            "truncated TIFF pixel payload must fail closed");

    auto invalid_extra = make_rgba_tiff(1U, 0U);
    require(io::decode_raster(io::RasterFormat::Tiff, invalid_extra).error == io::RasterDecodeError::UnsupportedFeature,
            "ambiguous fourth TIFF sample must fail closed");

    const std::vector<std::uint8_t> malformed{0x49U, 0x49U, 0x2aU, 0x00U};
    require(io::decode_raster(io::RasterFormat::Tiff, malformed).error == io::RasterDecodeError::MalformedContainer,
            "short TIFF container must fail closed");

    require(io::decode_raster(io::RasterFormat::Png, std::span<const std::uint8_t>{}).error == io::RasterDecodeError::UnsupportedFormat,
            "formats without a pixel decoder must not be falsely accepted");
    require(std::string_view(io::raster_decode_error_name(io::RasterDecodeError::UnsupportedFeature)) == "unsupported_feature",
            "decode error names must be stable");

    return 0;
}
