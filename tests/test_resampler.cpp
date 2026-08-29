#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "vektoryum/resample/resampler.hpp"

namespace {

int resampler_failures = 0;

void expect(bool condition, std::string_view name) {
    if (!condition) {
        ++resampler_failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

void expect_near(double actual, double expected, double tolerance, std::string_view name) {
    expect(std::abs(actual - expected) <= tolerance, name);
}

vektoryum::resample::FloatImage constant_image(
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t channels,
    float value) {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                              static_cast<std::size_t>(channels);
    return {width, height, channels, std::vector<float>(count, value)};
}

void test_identity_is_exact() {
    using namespace vektoryum::resample;
    FloatImage source{
        2U,
        2U,
        3U,
        {0.0F, 0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F, 0.9F, 1.0F, 0.25F},
    };
    const ResampleResult result = resize(source, 2U, 2U);
    expect(result.ok(), "resampler identity succeeds");
    expect(result.image.pixels == source.pixels, "resampler identity is bit-exact");
}

void test_required_upscale_factors() {
    using namespace vektoryum::resample;
    const FloatImage source = constant_image(3U, 2U, 4U, 0.375F);

    for (const std::uint32_t scale : {2U, 4U, 8U}) {
        const ResampleResult result = resize(source, source.width * scale, source.height * scale);
        expect(result.ok(), "2x/4x/8x Lanczos upscale succeeds");
        expect(
            result.image.width == source.width * scale && result.image.height == source.height * scale,
            "required upscale factor has exact dimensions");
        const bool constant_preserved = std::all_of(
            result.image.pixels.begin(),
            result.image.pixels.end(),
            [](float value) { return std::abs(value - 0.375F) <= 1.0e-6F; });
        expect(constant_preserved, "constant field remains constant after upscale");
    }
}

void test_step_has_no_overshoot() {
    using namespace vektoryum::resample;
    const FloatImage source{4U, 1U, 1U, {0.0F, 0.0F, 1.0F, 1.0F}};
    const ResampleResult result = resize(source, 64U, 1U, {Filter::Lanczos3, true});
    expect(result.ok(), "step-edge upscale succeeds");

    const auto [minimum, maximum] = std::minmax_element(result.image.pixels.begin(), result.image.pixels.end());
    expect(*minimum >= -1.0e-7F, "ringing gate: no negative undershoot");
    expect(*maximum <= 1.0F + 1.0e-7F, "ringing gate: no positive overshoot");
}

void test_linear_ramp_shape() {
    using namespace vektoryum::resample;
    FloatImage source{8U, 1U, 1U, {}};
    source.pixels.reserve(8U);
    for (std::uint32_t x = 0U; x < 8U; ++x) {
        source.pixels.push_back(static_cast<float>(x) / 7.0F);
    }

    const ResampleResult result = resize(source, 64U, 1U);
    expect(result.ok(), "linear-ramp upscale succeeds");
    expect(result.image.pixels.front() >= 0.0F && result.image.pixels.back() <= 1.0F, "ramp stays bounded");

    double maximum_error = 0.0;
    for (std::uint32_t x = 8U; x < 56U; ++x) {
        const double source_position = (static_cast<double>(x) + 0.5) / 8.0 - 0.5;
        const double expected = std::clamp(source_position / 7.0, 0.0, 1.0);
        maximum_error = std::max(
            maximum_error,
            std::abs(static_cast<double>(result.image.pixels[static_cast<std::size_t>(x)]) - expected));
    }
    expect(maximum_error <= 0.02, "interior linear-ramp reconstruction error <= 0.02");
}

void test_downscale_antialiasing() {
    using namespace vektoryum::resample;
    FloatImage checker{64U, 64U, 1U, std::vector<float>(64U * 64U, 0.0F)};
    for (std::uint32_t y = 0U; y < checker.height; ++y) {
        for (std::uint32_t x = 0U; x < checker.width; ++x) {
            checker.pixels[checker.index(x, y, 0U)] = ((x + y) % 2U == 0U) ? 0.0F : 1.0F;
        }
    }

    const ResampleResult result = resize(checker, 8U, 8U);
    expect(result.ok(), "checkerboard downscale succeeds");
    double mean = 0.0;
    for (float value : result.image.pixels) {
        mean += static_cast<double>(value);
    }
    mean /= static_cast<double>(result.image.pixels.size());
    expect_near(mean, 0.5, 0.01, "anti-alias gate preserves checkerboard mean");

    const auto [minimum, maximum] = std::minmax_element(result.image.pixels.begin(), result.image.pixels.end());
    expect(*minimum >= 0.40F && *maximum <= 0.60F, "anti-alias gate suppresses high-frequency checker pattern");
}

void test_determinism_and_validation() {
    using namespace vektoryum::resample;
    const FloatImage source{3U, 2U, 1U, {0.0F, 0.2F, 0.8F, 1.0F, 0.4F, 0.6F}};
    const ResampleResult first = resize(source, 19U, 11U);
    const ResampleResult second = resize(source, 19U, 11U);
    expect(first.ok() && second.ok(), "determinism inputs succeed");
    expect(first.image.pixels == second.image.pixels, "same input/options are deterministic");

    FloatImage mismatch = source;
    mismatch.pixels.pop_back();
    expect(
        resize(mismatch, 6U, 4U).error == ResampleError::SourceSizeMismatch,
        "source sample-count mismatch rejected");
    expect(
        resize(source, 0U, 4U).error == ResampleError::ZeroTargetDimension,
        "zero target dimension rejected");

    FloatImage bad_channels = source;
    bad_channels.channels = 0U;
    expect(
        resize(bad_channels, 6U, 4U).error == ResampleError::InvalidChannelCount,
        "zero channel count rejected");
}

}  // namespace

int run_resampler_tests() {
    test_identity_is_exact();
    test_required_upscale_factors();
    test_step_has_no_overshoot();
    test_linear_ramp_shape();
    test_downscale_antialiasing();
    test_determinism_and_validation();
    return resampler_failures;
}
