#include "vektoryum/io/raster_decode.hpp"

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

void append_u16_be(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_segment(
    std::vector<std::uint8_t>& jpeg,
    std::uint8_t marker,
    std::span<const std::uint8_t> payload) {
    jpeg.push_back(0xffU);
    jpeg.push_back(marker);
    append_u16_be(jpeg, static_cast<std::uint16_t>(payload.size() + 2U));
    jpeg.insert(jpeg.end(), payload.begin(), payload.end());
}

[[nodiscard]] std::vector<std::uint8_t> make_color_jpeg(std::uint8_t y_sampling) {
    std::vector<std::uint8_t> jpeg{0xffU, 0xd8U};

    std::vector<std::uint8_t> dqt(65U, 1U);
    dqt[0U] = 0U;
    append_segment(jpeg, 0xdbU, dqt);

    const std::vector<std::uint8_t> sof0{
        8U,
        0U, 1U,
        0U, 1U,
        3U,
        1U, y_sampling, 0U,
        2U, 0x11U, 0U,
        3U, 0x11U, 0U,
    };
    append_segment(jpeg, 0xc0U, sof0);

    std::vector<std::uint8_t> dc(18U, 0U);
    dc[0U] = 0x00U;
    dc[1U] = 1U;
    dc[17U] = 0U;
    append_segment(jpeg, 0xc4U, dc);

    std::vector<std::uint8_t> ac(18U, 0U);
    ac[0U] = 0x10U;
    ac[1U] = 1U;
    ac[17U] = 0U;
    append_segment(jpeg, 0xc4U, ac);

    const std::vector<std::uint8_t> sos{
        3U,
        1U, 0x00U,
        2U, 0x00U,
        3U, 0x00U,
        0U, 63U, 0U,
    };
    append_segment(jpeg, 0xdaU, sos);

    if (y_sampling == 0x11U) {
        // Three zero-coefficient blocks: (DC category 0 + AC EOB) * 3 = 6 zero bits,
        // followed by JPEG's required one-bit entropy padding.
        jpeg.push_back(0x03U);
    } else {
        // 4:2:0 uses four Y blocks plus one Cb and one Cr block = 12 zero bits,
        // padded with four one bits to the next byte boundary.
        jpeg.push_back(0x00U);
        jpeg.push_back(0x0fU);
    }

    jpeg.push_back(0xffU);
    jpeg.push_back(0xd9U);
    return jpeg;
}

void verify_neutral_color_decode(std::uint8_t sampling, std::string_view label) {
    const auto bytes = make_color_jpeg(sampling);
    const auto result = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, bytes);
    require(result.ok(), label);
    require(result.image.spec.width == 1U && result.image.spec.height == 1U,
            "color JPEG dimensions must match SOF0");
    require(result.image.rgba8 == std::vector<std::uint8_t>({128U, 128U, 128U, 255U}),
            "neutral YCbCr color JPEG must normalize to opaque neutral RGBA8");
}

}  // namespace

int main() {
    verify_neutral_color_decode(0x11U, "baseline 3-component 4:4:4 JPEG must decode");
    verify_neutral_color_decode(0x22U, "baseline 3-component 4:2:0 JPEG must decode");
    return 0;
}
