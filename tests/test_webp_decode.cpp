#include "vektoryum/io/raster_decode.hpp"

#include "webp_lossless_fixtures.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

// The source images the reference encoder was fed. Every expression here is
// integer-only so the expectation is identical on every platform.
[[nodiscard]] std::vector<std::uint8_t> render(
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>& pixel) {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::array<std::uint8_t, 4U> value = pixel(x, y);
            rgba.insert(rgba.end(), value.begin(), value.end());
        }
    }
    return rgba;
}

[[nodiscard]] std::uint8_t byte_of(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(value & 0xffU);
}

void check_fixture(
    std::string_view name,
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    const std::function<std::array<std::uint8_t, 4U>(std::uint32_t, std::uint32_t)>& pixel) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, encoded);
    if (!decoded.ok()) {
        std::cerr << "FAIL: " << name << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    if (decoded.image.spec.width != width || decoded.image.spec.height != height) {
        std::cerr << "FAIL: " << name << " dimensions must match the encoded image\n";
        ++failures;
        return;
    }
    expect(decoded.image.spec.layout == vektoryum::core::PixelLayout::RGBA,
           "lossless WebP must normalize to the canonical RGBA layout");
    expect(decoded.image.spec.transfer == vektoryum::core::TransferFunction::SRGB,
           "lossless WebP must normalize to the sRGB transfer function");
    expect(decoded.image.spec.primaries == vektoryum::core::ColorPrimaries::SRGB,
           "lossless WebP must normalize to sRGB primaries");
    expect(decoded.image.spec.alpha == vektoryum::core::AlphaMode::Straight,
           "lossless WebP carries straight, non-premultiplied alpha");

    const std::vector<std::uint8_t> expected = render(width, height, pixel);
    if (decoded.image.rgba8 != expected) {
        std::size_t index = 0U;
        while (index < expected.size() && index < decoded.image.rgba8.size() &&
               decoded.image.rgba8[index] == expected[index]) {
            ++index;
        }
        std::cerr << "FAIL: " << name << " pixels differ from the encoded source at byte " << index
                  << '\n';
        ++failures;
        return;
    }

    const auto again = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, encoded);
    expect(again.ok() && again.image.rgba8 == decoded.image.rgba8,
           "repeated WebP decodes must be deterministic");
}

void check_rejected(std::string_view name, std::span<const std::uint8_t> encoded) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, encoded);
    if (decoded.ok()) {
        std::cerr << "FAIL: " << name << " must fail closed\n";
        ++failures;
    }
}

// The predictor-coverage source images, regenerated with the same integer
// arithmetic (and the same linear congruential sequence) the generator used.
class Lcg {
public:
    explicit Lcg(std::uint32_t seed) noexcept : state_(seed * 2654435761U + 1U) {}

    [[nodiscard]] std::uint32_t next() noexcept {
        state_ = state_ * 1103515245U + 12345U;
        return state_;
    }

    [[nodiscard]] std::int32_t draw(std::int32_t low, std::int32_t high) noexcept {
        const auto span = static_cast<std::uint32_t>(high - low);
        return low + static_cast<std::int32_t>((next() >> 16U) % span);
    }

private:
    std::uint32_t state_;
};

[[nodiscard]] std::uint8_t clamp_byte(std::int32_t value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

[[nodiscard]] std::vector<std::uint8_t> render_predictor_source(
    std::uint32_t seed,
    std::uint32_t width,
    std::uint32_t height) {
    Lcg random(seed);
    const std::int32_t kind = random.draw(0, 6);
    const std::int32_t ax = random.draw(-9, 10);
    const std::int32_t ay = random.draw(-9, 10);
    const std::int32_t bx = random.draw(-9, 10);
    const std::int32_t by = random.draw(-9, 10);
    const std::int32_t b0 = random.draw(0, 256);
    const std::int32_t b1 = random.draw(0, 256);
    const std::int32_t b2 = random.draw(0, 256);
    const std::int32_t b3 = random.draw(0, 256);
    const std::int32_t period = random.draw(2, 13);
    const std::int32_t amp = random.draw(1, 90);

    std::vector<std::uint8_t> rgba;
    rgba.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (std::uint32_t row = 0U; row < height; ++row) {
        for (std::uint32_t column = 0U; column < width; ++column) {
            const auto x = static_cast<std::int32_t>(column);
            const auto y = static_cast<std::int32_t>(row);
            std::array<std::uint8_t, 4U> pixel{};
            switch (kind) {
                case 0:
                    pixel = {clamp_byte(b0 + ax * x + ay * y), clamp_byte(b1 + bx * x + by * y),
                             clamp_byte(b2 + ax * y - by * x), 255U};
                    break;
                case 1: {
                    const std::int32_t d = ((x * std::abs(ax) + y * std::abs(ay)) / period) * amp;
                    pixel = {clamp_byte(b0 + d), clamp_byte(b1 - d), clamp_byte(b2 + d / 2), 255U};
                    break;
                }
                case 2:
                    pixel = {clamp_byte(b0 + amp * ((x / period + y / period) % 2)),
                             clamp_byte(b1 + ax * x), clamp_byte(b2 + ay * y), 255U};
                    break;
                case 3: {
                    const std::int32_t red = random.draw(0, 2 * amp + 1) - amp;
                    const std::int32_t green = random.draw(0, 2 * amp + 1) - amp;
                    const std::int32_t blue = random.draw(0, 2 * amp + 1) - amp;
                    pixel = {clamp_byte(b0 + red), clamp_byte(b1 + green), clamp_byte(b2 + blue), 255U};
                    break;
                }
                case 4: {
                    const std::uint8_t v = clamp_byte(b0 + amp * ((x * y) % period));
                    pixel = {v, clamp_byte(static_cast<std::int32_t>(v) + ax * x),
                             clamp_byte(static_cast<std::int32_t>(v) + ay * y), 255U};
                    break;
                }
                default:
                    pixel = {clamp_byte(b0 + ax * x + ay * y), clamp_byte(b1 + bx * x + by * y),
                             clamp_byte(b2 + ax * x - ay * y), clamp_byte(b3 + bx * y)};
                    break;
            }
            rgba.insert(rgba.end(), pixel.begin(), pixel.end());
        }
    }
    return rgba;
}

void check_predictor_fixture(
    std::uint32_t seed,
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height) {
    const auto decoded = vektoryum::io::decode_raster(vektoryum::io::RasterFormat::Webp, encoded);
    if (!decoded.ok()) {
        std::cerr << "FAIL: predictor fixture " << seed << " must decode, got "
                  << vektoryum::io::raster_decode_error_name(decoded.error) << '\n';
        ++failures;
        return;
    }
    if (decoded.image.spec.width != width || decoded.image.spec.height != height) {
        std::cerr << "FAIL: predictor fixture " << seed << " dimensions must match\n";
        ++failures;
        return;
    }
    if (decoded.image.rgba8 != render_predictor_source(seed, width, height)) {
        std::cerr << "FAIL: predictor fixture " << seed << " pixels differ from the encoded source\n";
        ++failures;
    }
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

void check_cli_convert(const std::filesystem::path& cli) {
    using namespace vektoryum_test_fixtures;

    const auto dir = std::filesystem::temp_directory_path() / "vektoryum-r2-webp-cli";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    ec.clear();
    if (!std::filesystem::create_directories(dir, ec)) {
        expect(false, "temporary WebP CLI directory must be created");
        return;
    }

    const auto input_path = dir / "alpha_fade.webp";
    const auto output_path = dir / "alpha_fade.pam";
    {
        std::ofstream out(input_path, std::ios::binary | std::ios::trunc);
        out.write(
            reinterpret_cast<const char*>(webp_alpha_fade.data()),
            static_cast<std::streamsize>(webp_alpha_fade.size()));
        if (!out) {
            expect(false, "WebP CLI fixture must write completely");
            return;
        }
    }

    const std::string command = quote(cli) + " --convert " + quote(input_path) + " " + quote(output_path);
    if (run_command(command) != 0) {
        expect(false, "CLI --convert must accept a real multi-pixel WebP");
        return;
    }

    std::ifstream in(output_path, std::ios::binary);
    if (!in) {
        expect(false, "WebP CLI PAM output must exist");
        return;
    }
    const std::vector<std::uint8_t> actual{
        std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const std::string header = "P7\nWIDTH 24\nHEIGHT 16\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    const std::vector<std::uint8_t> expected = render(24U, 16U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 17U), byte_of((x + y) * 5U), byte_of((x * 255U) / 23U)};
    });
    if (actual.size() != header.size() + expected.size()) {
        expect(false, "WebP CLI PAM size must be canonical");
        return;
    }
    expect(std::equal(header.begin(), header.end(), actual.begin()),
           "WebP CLI PAM header must be canonical");
    expect(std::equal(expected.begin(), expected.end(), actual.begin() + static_cast<std::ptrdiff_t>(header.size())),
           "WebP CLI PAM pixels must match the decoded source image");

    std::filesystem::remove_all(dir, ec);
}

}  // namespace

int main(int argc, char** argv) {
    using namespace vektoryum_test_fixtures;

    check_fixture("gradient 19x11", webp_gradient, 19U, 11U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 13U + y * 7U), byte_of(x * 3U + y * 29U), byte_of(x * 47U + y * 11U), 255U};
    });

    check_fixture("alpha_fade 24x16", webp_alpha_fade, 24U, 16U, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4U>{
            byte_of(x * 9U), byte_of(y * 17U), byte_of((x + y) * 5U), byte_of((x * 255U) / 23U)};
    });

    check_fixture("palette16 17x13", webp_palette16, 17U, 13U, [](std::uint32_t x, std::uint32_t y) {
        const std::uint32_t v = (x * y) % 16U;
        return std::array<std::uint8_t, 4U>{
            byte_of(v * 16U), byte_of(255U - v * 16U), byte_of(v * 7U), byte_of(v % 3U != 0U ? 255U : 40U)};
    });

    check_fixture("two_color 40x8", webp_two_color, 40U, 8U, [](std::uint32_t x, std::uint32_t y) {
        return ((x ^ y) & 1U) == 0U ? std::array<std::uint8_t, 4U>{255U, 0U, 0U, 255U}
                                    : std::array<std::uint8_t, 4U>{0U, 0U, 255U, 128U};
    });

    check_fixture("tiles 48x48", webp_tiles, 48U, 48U, [](std::uint32_t x, std::uint32_t y) {
        const std::uint32_t v = (x % 8U) * 32U;
        const std::uint32_t u = (y % 8U) * 32U;
        return std::array<std::uint8_t, 4U>{byte_of(v), byte_of(u), byte_of(v ^ u), 255U};
    });

    check_fixture("transparent 16x16", webp_transparent, 16U, 16U, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 4U>{0U, 0U, 0U, 0U};
    });

    check_fixture("regions 96x64", webp_regions, 96U, 64U, [](std::uint32_t x, std::uint32_t y) {
        constexpr std::uint32_t half_width = 48U;
        constexpr std::uint32_t half_height = 32U;
        if (x < half_width && y < half_height) {
            return std::array<std::uint8_t, 4U>{
                byte_of(x * 3U), byte_of(y * 5U), byte_of((x + y) * 2U), 255U};
        }
        if (x >= half_width && y < half_height) {
            const std::uint32_t s = (x * 2654435761U) ^ (y * 40503U);
            return std::array<std::uint8_t, 4U>{byte_of(s >> 16U), byte_of(s >> 8U), byte_of(s), 255U};
        }
        if (x < half_width) {
            return (x / 4U) % 2U == 0U ? std::array<std::uint8_t, 4U>{250U, 250U, 250U, 255U}
                                       : std::array<std::uint8_t, 4U>{5U, 5U, 5U, 255U};
        }
        return std::array<std::uint8_t, 4U>{64U, 128U, 192U, byte_of(y * 4U)};
    });

    // Every spatial predictor mode the format defines is exercised by at least
    // one of these reference encodings.
    check_predictor_fixture(1296U, webp_predictor_1296, 12U, 12U);
    check_predictor_fixture(2451U, webp_predictor_2451, 24U, 12U);
    check_predictor_fixture(369U, webp_predictor_369, 24U, 16U);
    check_predictor_fixture(1288U, webp_predictor_1288, 28U, 28U);
    check_predictor_fixture(2003U, webp_predictor_2003, 32U, 24U);
    check_predictor_fixture(2196U, webp_predictor_2196, 12U, 12U);

    // Malformed and unsupported containers must fail closed rather than
    // producing approximate pixels.
    check_rejected("empty input", std::span<const std::uint8_t>{});
    {
        std::vector<std::uint8_t> truncated(webp_gradient.begin(), webp_gradient.end() - 8);
        check_rejected("truncated VP8L payload", truncated);
    }
    {
        std::vector<std::uint8_t> broken(webp_gradient.begin(), webp_gradient.end());
        broken[20U] = 0x30U;  // wrong VP8L signature byte
        check_rejected("bad VP8L signature", broken);
    }
    {
        std::vector<std::uint8_t> broken(webp_gradient.begin(), webp_gradient.end());
        broken[9U] = 'X';  // corrupt the WEBP fourcc
        check_rejected("bad WEBP fourcc", broken);
    }
    {
        std::vector<std::uint8_t> broken(webp_gradient.begin(), webp_gradient.end());
        broken[4U] = 0xffU;
        broken[5U] = 0xffU;  // RIFF size larger than the file
        check_rejected("RIFF size past end of file", broken);
    }
    {
        std::vector<std::uint8_t> broken(webp_gradient.begin(), webp_gradient.end());
        broken[16U] = 0xffU;
        broken[17U] = 0xffU;  // VP8L chunk size larger than the container
        check_rejected("VP8L chunk size past end of container", broken);
    }
    {
        // A lossy VP8 bitstream: recognised, and refused rather than guessed at.
        std::vector<std::uint8_t> lossy{
            'R', 'I', 'F', 'F', 0x14U, 0x00U, 0x00U, 0x00U, 'W', 'E', 'B', 'P',
            'V', 'P', '8', ' ', 0x08U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U, 0x00U, 0x9dU, 0x01U, 0x2aU, 0x01U, 0x00U};
        check_rejected("lossy VP8 bitstream", lossy);
    }

    if (argc >= 1) {
        std::filesystem::path cli = std::filesystem::path(argv[0]).parent_path() / "vektoryum_cli";
#ifdef _WIN32
        cli += ".exe";
#endif
        if (std::filesystem::exists(cli)) {
            check_cli_convert(cli);
        } else {
            expect(false, "CLI executable must exist beside the WebP acceptance test");
        }
    }

    return failures == 0 ? 0 : 1;
}
