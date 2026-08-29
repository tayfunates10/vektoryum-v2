#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#include "vektoryum/core/color.hpp"
#include "vektoryum/core/image.hpp"
#include "vektoryum/core/tile.hpp"
#include "vektoryum/io/limits.hpp"
#include "vektoryum/version.hpp"

int run_resampler_tests();
int run_content_analyzer_tests();
int run_photo_restoration_tests();
int run_ml_runtime_tests();
int run_dataset_contract_tests();
int run_vector_reconstruction_tests();
int run_svg_path_tests();

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

void expect_near(double actual, double expected, double tolerance, std::string_view name) {
    expect_true(std::abs(actual - expected) <= tolerance, name);
}

void test_version() {
    expect_true(vektoryum::version_major == 0, "version major is stable");
    expect_true(vektoryum::version_minor == 1, "version minor is stable");
    expect_true(vektoryum::version_patch == 0, "version patch is stable");
    expect_true(vektoryum::version_string() == "0.1.0", "version string matches components");
}

void test_image_spec() {
    using namespace vektoryum::core;
    expect_true(channel_count(PixelLayout::Gray) == 1U, "gray has one channel");
    expect_true(channel_count(PixelLayout::RGBA) == 4U, "rgba has four channels");
    expect_true(bits_per_channel(ChannelType::UInt8) == 8U, "u8 is 8-bit");
    expect_true(bits_per_channel(ChannelType::UInt16) == 16U, "u16 is 16-bit");
    expect_true(bits_per_channel(ChannelType::Float32) == 32U, "f32 is 32-bit");

    const ImageSpec rgba16{4U, 3U, PixelLayout::RGBA, ChannelType::UInt16,
                           TransferFunction::SRGB, ColorPrimaries::SRGB, AlphaMode::Straight};
    const auto valid = validate_image_spec(rgba16);
    expect_true(valid.ok(), "valid rgba16 image spec accepted");
    expect_true(valid.byte_size == 96U, "rgba16 byte size is exact");

    auto invalid = rgba16;
    invalid.width = 0U;
    expect_true(validate_image_spec(invalid).error == ImageSpecError::ZeroWidth,
                "zero width rejected");
    invalid = rgba16;
    invalid.layout = PixelLayout::RGB;
    invalid.alpha = AlphaMode::Straight;
    expect_true(validate_image_spec(invalid).error == ImageSpecError::AlphaModeWithoutAlphaChannel,
                "alpha mode without alpha channel rejected");
    invalid = rgba16;
    invalid.alpha = AlphaMode::None;
    expect_true(validate_image_spec(invalid).error == ImageSpecError::MissingAlphaMode,
                "alpha-capable layout requires alpha mode");

    const ImageSpec huge{std::numeric_limits<std::uint32_t>::max(),
                         std::numeric_limits<std::uint32_t>::max(), PixelLayout::RGBA,
                         ChannelType::Float32, TransferFunction::Linear,
                         ColorPrimaries::SRGB, AlphaMode::Straight};
    expect_true(validate_image_spec(huge).error == ImageSpecError::SizeOverflow,
                "byte-size overflow is rejected");
}

void test_color_and_alpha() {
    using namespace vektoryum::core;
    expect_near(srgb_to_linear(0.0), 0.0, 1e-12, "sRGB black maps to linear black");
    expect_near(srgb_to_linear(1.0), 1.0, 1e-12, "sRGB white maps to linear white");
    expect_near(linear_to_srgb(0.0), 0.0, 1e-12, "linear black maps to sRGB black");
    expect_near(linear_to_srgb(1.0), 1.0, 1e-12, "linear white maps to sRGB white");
    for (const double value : {0.0, 0.003, 0.018, 0.18, 0.5, 0.75, 1.0}) {
        expect_near(linear_to_srgb(srgb_to_linear(value)), value, 1e-12,
                    "sRGB linear round-trip is stable");
    }
    const Rgba64 straight{0.8, 0.4, 0.2, 0.25};
    const Rgba64 premultiplied = premultiply_alpha(straight);
    const Rgba64 restored = unpremultiply_alpha(premultiplied);
    expect_near(restored.r, straight.r, 1e-12, "unpremultiply red round-trip");
    expect_near(restored.g, straight.g, 1e-12, "unpremultiply green round-trip");
    expect_near(restored.b, straight.b, 1e-12, "unpremultiply blue round-trip");
    expect_near(restored.a, straight.a, 1e-12, "unpremultiply alpha round-trip");
    const Rgba64 transparent = unpremultiply_alpha({0.9, 0.8, 0.7, 0.0});
    expect_true(transparent.r == 0.0 && transparent.g == 0.0 && transparent.b == 0.0 &&
                    transparent.a == 0.0,
                "transparent premultiplied pixel cannot leak hidden RGB");
}

void test_tiles() {
    using namespace vektoryum::core;
    const TilePlan plan = plan_tiles(1000U, 750U, 256U, 32U);
    expect_true(plan.ok(), "tile plan succeeds");
    expect_true(plan.tiles.size() == 12U, "tile plan has expected 4x3 grid");
    std::uint64_t core_area = 0U;
    bool all_bounded = true;
    for (const TileRegion& tile : plan.tiles) {
        core_area += static_cast<std::uint64_t>(tile.core.width) * tile.core.height;
        const std::uint64_t core_right = static_cast<std::uint64_t>(tile.core.x) + tile.core.width;
        const std::uint64_t core_bottom = static_cast<std::uint64_t>(tile.core.y) + tile.core.height;
        const std::uint64_t expanded_right = static_cast<std::uint64_t>(tile.expanded.x) + tile.expanded.width;
        const std::uint64_t expanded_bottom = static_cast<std::uint64_t>(tile.expanded.y) + tile.expanded.height;
        all_bounded = all_bounded && core_right <= 1000U && core_bottom <= 750U &&
                      expanded_right <= 1000U && expanded_bottom <= 750U &&
                      tile.expanded.x <= tile.core.x && tile.expanded.y <= tile.core.y &&
                      expanded_right >= core_right && expanded_bottom >= core_bottom;
    }
    expect_true(core_area == 750'000U, "tile cores cover image exactly once");
    expect_true(all_bounded, "expanded tiles stay bounded and contain their cores");
    expect_true(plan_tiles(0U, 64U, 32U, 4U).error == TilePlanError::ZeroImageDimension,
                "zero image dimension rejected by tile planner");
    expect_true(plan_tiles(64U, 64U, 0U, 4U).error == TilePlanError::ZeroTileExtent,
                "zero tile extent rejected");
}

void test_decode_limits() {
    using namespace vektoryum;
    const core::ImageSpec image{1024U, 1024U, core::PixelLayout::RGBA, core::ChannelType::UInt8,
                                core::TransferFunction::SRGB, core::ColorPrimaries::SRGB,
                                core::AlphaMode::Straight};
    const auto accepted = io::validate_decode_limits(image);
    expect_true(accepted.ok(), "normal image passes default decode limits");
    expect_true(accepted.decoded_bytes == 4U * 1024U * 1024U, "decode byte budget is exact");
}

}  // namespace

int main() {
    test_version();
    test_image_spec();
    test_color_and_alpha();
    test_tiles();
    test_decode_limits();
    failures += run_resampler_tests();
    failures += run_content_analyzer_tests();
    failures += run_photo_restoration_tests();
    failures += run_ml_runtime_tests();
    failures += run_dataset_contract_tests();
    failures += run_vector_reconstruction_tests();
    failures += run_svg_path_tests();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed\n";
    return EXIT_SUCCESS;
}
