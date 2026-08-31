#include "vektoryum/io/raster_decode.hpp"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
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

    return 0;
}
