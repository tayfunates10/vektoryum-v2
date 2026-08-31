#include "vektoryum/io/raster_decode.hpp"

#include "jpeg_baseline_fixtures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
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

    // Each block costs two zero bits with these tables: DC category 0 followed by
    // an AC end-of-block. One minimum coded unit holds the luma blocks the
    // sampling factors ask for plus one Cb and one Cr block, and the last byte is
    // padded with the one bits JPEG requires.
    const std::size_t luma_blocks =
        static_cast<std::size_t>(y_sampling >> 4U) * static_cast<std::size_t>(y_sampling & 0x0fU);
    std::size_t remaining = (luma_blocks + 2U) * 2U;
    while (remaining >= 8U) {
        jpeg.push_back(0x00U);
        remaining -= 8U;
    }
    if (remaining != 0U) {
        jpeg.push_back(static_cast<std::uint8_t>((1U << (8U - remaining)) - 1U));
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


// Measured fidelity of a decoded fixture against the source image the encoder
// was given. `psnr_floor` is in decibels and `channel_bound` is the largest
// per-channel deviation tolerated anywhere in the image.
struct FidelityGate {
    double psnr_floor;
    int channel_bound;
};

[[nodiscard]] std::vector<std::uint8_t> render_source(
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 3U>(std::uint32_t, std::uint32_t)>& pixel) {
    std::vector<std::uint8_t> rgb;
    rgb.reserve(static_cast<std::size_t>(width) * height * 3U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::array<std::uint8_t, 3U> value = pixel(x, y);
            rgb.insert(rgb.end(), value.begin(), value.end());
        }
    }
    return rgb;
}

void check_fidelity(
    std::string_view name,
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    const FidelityGate& gate,
    const std::function<std::array<std::uint8_t, 3U>(std::uint32_t, std::uint32_t)>& pixel) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, encoded);
    if (!decoded.ok()) {
        std::cerr << "FAIL: " << name << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    if (decoded.image.spec.width != width || decoded.image.spec.height != height) {
        std::cerr << "FAIL: " << name << " dimensions must match SOF0\n";
        ++failures;
        return;
    }
    require(decoded.image.spec.transfer == vektoryum::core::TransferFunction::SRGB,
            "baseline JPEG must normalize to the sRGB transfer function");
    require(decoded.image.spec.alpha == vektoryum::core::AlphaMode::Straight,
            "baseline JPEG must produce opaque straight-alpha RGBA8");

    const std::vector<std::uint8_t> source = render_source(width, height, pixel);
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    double squared_error = 0.0;
    int worst = 0;
    for (std::size_t i = 0U; i < pixel_count; ++i) {
        require(decoded.image.rgba8[i * 4U + 3U] == 255U, "baseline JPEG has no transparency");
        for (std::size_t channel = 0U; channel < 3U; ++channel) {
            const int difference = static_cast<int>(decoded.image.rgba8[i * 4U + channel]) -
                                   static_cast<int>(source[i * 3U + channel]);
            squared_error += static_cast<double>(difference) * static_cast<double>(difference);
            worst = std::max(worst, std::abs(difference));
        }
    }
    const double mean_squared_error = squared_error / static_cast<double>(pixel_count * 3U);
    const double psnr = mean_squared_error == 0.0
                            ? std::numeric_limits<double>::infinity()
                            : 10.0 * std::log10(255.0 * 255.0 / mean_squared_error);
    if (psnr < gate.psnr_floor) {
        std::cerr << "FAIL: " << name << " PSNR " << psnr << " dB is below the " << gate.psnr_floor
                  << " dB floor\n";
        ++failures;
    }
    if (worst > gate.channel_bound) {
        std::cerr << "FAIL: " << name << " deviates by " << worst << ", above the bound of "
                  << gate.channel_bound << '\n';
        ++failures;
    }

    const auto again = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, encoded);
    require(again.ok() && again.image.rgba8 == decoded.image.rgba8,
            "repeated baseline JPEG decodes must be deterministic");
}

[[nodiscard]] std::uint8_t ramp(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(value, 255U));
}

}  // namespace

int main() {
    using namespace vektoryum_test_fixtures;

    // Hand-built minimal frames keep the marker-parsing paths covered.
    verify_neutral_color_decode(0x11U, "baseline 3-component 4:4:4 JPEG must decode");
    verify_neutral_color_decode(0x21U, "baseline 3-component 4:2:2 JPEG must decode");
    verify_neutral_color_decode(0x22U, "baseline 3-component 4:2:0 JPEG must decode");

    const auto gray_ramp = [](std::uint32_t x, std::uint32_t y) {
        const std::uint8_t v = ramp(16U + x * 6U + y * 4U);
        return std::array<std::uint8_t, 3U>{v, v, v};
    };
    const auto smooth_color = [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 3U>{
            ramp(40U + x * 5U), ramp(30U + y * 7U), ramp(20U + (x + y) * 3U)};
    };
    const auto soft_blocks = [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 3U>{
            static_cast<std::uint8_t>(60U + (x / 8U) * 30U),
            static_cast<std::uint8_t>(90U + (y / 6U) * 25U),
            static_cast<std::uint8_t>(120U + ((x / 8U + y / 6U) % 4U) * 20U)};
    };

    // Grey content survives the encoder exactly, so these two are held to a
    // bit-exact reconstruction of the source.
    check_fidelity("grayscale ramp", jpeg_gray_ramp_grayscale, 24U, 16U, {60.0, 0}, gray_ramp);
    check_fidelity("4:4:4 grey ramp", jpeg_gray_ramp_444, 24U, 16U, {60.0, 0}, gray_ramp);

    check_fidelity("4:4:4 smooth colour", jpeg_smooth_color_444, 32U, 24U, {46.0, 5}, smooth_color);
    check_fidelity("4:2:2 smooth colour", jpeg_smooth_color_422, 32U, 24U, {42.0, 6}, smooth_color);
    check_fidelity("4:2:0 smooth colour", jpeg_smooth_color_420, 32U, 24U, {39.0, 8}, smooth_color);
    check_fidelity("4:2:2 blocky colour", jpeg_soft_blocks_422, 32U, 16U, {40.0, 13}, soft_blocks);

    // Flat, fully saturated primaries survive 4:4:4 encoding almost intact, so
    // this fixture is held to a tight bound that a wrong YCbCr matrix would miss.
    const auto saturated = [](std::uint32_t x, std::uint32_t y) {
        constexpr std::array<std::array<std::uint8_t, 3U>, 6U> table{{
            {255U, 0U, 0U}, {0U, 255U, 0U}, {0U, 0U, 255U},
            {255U, 255U, 0U}, {0U, 255U, 255U}, {255U, 0U, 255U}}};
        return table[(x / 8U + (y / 8U) * 3U) % 6U];
    };
    check_fidelity("4:4:4 saturated primaries", jpeg_saturated_444, 48U, 16U, {50.0, 2}, saturated);

    // Features the decoder does not implement must be refused, not approximated.
    {
        auto progressive = make_color_jpeg(0x11U);
        for (std::size_t i = 0U; i + 1U < progressive.size(); ++i) {
            if (progressive[i] == 0xffU && progressive[i + 1U] == 0xc0U) {
                progressive[i + 1U] = 0xc2U;  // SOF2: progressive
                break;
            }
        }
        require(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, progressive).ok(),
                "progressive JPEG must fail closed");
    }
    {
        const auto complete = make_color_jpeg(0x22U);
        std::vector<std::uint8_t> truncated(complete.begin(), complete.end() - 3);
        require(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, truncated).ok(),
                "truncated JPEG entropy data must fail closed");
    }
    {
        std::vector<std::uint8_t> corrupt(
            jpeg_smooth_color_420.begin(), jpeg_smooth_color_420.end());
        corrupt.resize(corrupt.size() / 2U);
        require(!vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Jpeg, corrupt).ok(),
                "a JPEG cut in half must fail closed");
    }

    return failures == 0 ? 0 : 1;
}
