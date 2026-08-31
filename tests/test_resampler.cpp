#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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

double mean_squared_error(const std::vector<float>& actual, const std::vector<float>& expected) {
    if (actual.size() != expected.size() || actual.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    double squared_error = 0.0;
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        const double delta = static_cast<double>(actual[index]) - static_cast<double>(expected[index]);
        squared_error += delta * delta;
    }
    return squared_error / static_cast<double>(actual.size());
}

double psnr_unit_range(const std::vector<float>& actual, const std::vector<float>& expected) {
    const double mse = mean_squared_error(actual, expected);
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return 10.0 * std::log10(1.0 / mse);
}

double global_ssim_unit_range(const std::vector<float>& actual, const std::vector<float>& expected) {
    if (actual.size() != expected.size() || actual.empty()) {
        return -1.0;
    }

    double actual_mean = 0.0;
    double expected_mean = 0.0;
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        actual_mean += static_cast<double>(actual[index]);
        expected_mean += static_cast<double>(expected[index]);
    }
    const double sample_count = static_cast<double>(actual.size());
    actual_mean /= sample_count;
    expected_mean /= sample_count;

    double actual_variance = 0.0;
    double expected_variance = 0.0;
    double covariance = 0.0;
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        const double actual_delta = static_cast<double>(actual[index]) - actual_mean;
        const double expected_delta = static_cast<double>(expected[index]) - expected_mean;
        actual_variance += actual_delta * actual_delta;
        expected_variance += expected_delta * expected_delta;
        covariance += actual_delta * expected_delta;
    }
    actual_variance /= sample_count;
    expected_variance /= sample_count;
    covariance /= sample_count;

    constexpr double c1 = 0.01 * 0.01;
    constexpr double c2 = 0.03 * 0.03;
    const double luminance = (2.0 * actual_mean * expected_mean + c1) /
                             (actual_mean * actual_mean + expected_mean * expected_mean + c1);
    const double structure = (2.0 * covariance + c2) /
                             (actual_variance + expected_variance + c2);
    return luminance * structure;
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

void test_measured_roundtrip_fidelity() {
    using namespace vektoryum::resample;
    constexpr std::uint32_t width = 24U;
    constexpr std::uint32_t height = 16U;
    constexpr std::uint8_t channels = 3U;
    FloatImage source{width, height, channels, std::vector<float>(static_cast<std::size_t>(width) * height * channels, 0.0F)};

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(width - 1U);
            const float fy = static_cast<float>(y) / static_cast<float>(height - 1U);
            source.pixels[source.index(x, y, 0U)] = fx;
            source.pixels[source.index(x, y, 1U)] = fy;
            source.pixels[source.index(x, y, 2U)] = (0.65F * fx) + (0.35F * fy);
        }
    }

    const ResampleResult upscaled = resize(source, width * 4U, height * 4U);
    expect(upscaled.ok(), "quality fixture 4x upscale succeeds");
    if (!upscaled.ok()) {
        return;
    }
    const ResampleResult restored = resize(upscaled.image, width, height);
    expect(restored.ok(), "quality fixture downscale succeeds");
    if (!restored.ok()) {
        return;
    }

    const double psnr = psnr_unit_range(restored.image.pixels, source.pixels);
    const double ssim = global_ssim_unit_range(restored.image.pixels, source.pixels);
    expect(psnr >= 35.0, "PSNR regression gate: smooth RGB roundtrip >= 35 dB");
    expect(ssim >= 0.98, "SSIM regression gate: smooth RGB roundtrip >= 0.98");
}

void test_premultiplied_alpha_edge_does_not_leak_hidden_rgb() {
    using namespace vektoryum::resample;
    FloatImage source{
        4U,
        1U,
        4U,
        {
            0.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 0.0F,
            1.0F, 0.0F, 0.0F, 1.0F,
            1.0F, 0.0F, 0.0F, 1.0F,
        },
    };
    const ResampleResult result = resize(source, 64U, 1U);
    expect(result.ok(), "premultiplied-alpha edge upscale succeeds");
    if (!result.ok()) {
        return;
    }

    bool no_hidden_green_blue = true;
    bool premultiplied_constraint = true;
    for (std::uint32_t x = 0U; x < result.image.width; ++x) {
        const float red = result.image.pixels[result.image.index(x, 0U, 0U)];
        const float green = result.image.pixels[result.image.index(x, 0U, 1U)];
        const float blue = result.image.pixels[result.image.index(x, 0U, 2U)];
        const float alpha = result.image.pixels[result.image.index(x, 0U, 3U)];
        no_hidden_green_blue = no_hidden_green_blue && std::abs(green) <= 1.0e-7F && std::abs(blue) <= 1.0e-7F;
        premultiplied_constraint = premultiplied_constraint && red <= alpha + 1.0e-6F;
    }
    expect(no_hidden_green_blue, "alpha-fringe regression gate: transparent edge cannot introduce hidden RGB");
    expect(premultiplied_constraint, "alpha-fringe regression gate: RGB remains bounded by alpha");
}

void test_visual_regression_is_axis_neutral() {
    using namespace vektoryum::resample;
    constexpr std::uint32_t width = 7U;
    constexpr std::uint32_t height = 5U;
    constexpr std::uint8_t channels = 3U;
    FloatImage source{
        width,
        height,
        channels,
        std::vector<float>(static_cast<std::size_t>(width) * height * channels, 0.0F),
    };

    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const bool foreground = (x >= 2U && x <= 5U && y >= 1U && y <= 3U) || (x == y + 1U);
            source.pixels[source.index(x, y, 0U)] = foreground ? 1.0F : 0.05F;
            source.pixels[source.index(x, y, 1U)] = foreground ? 0.20F : 0.75F;
            source.pixels[source.index(x, y, 2U)] = foreground ? 0.05F : 0.30F;
        }
    }

    FloatImage transposed{
        height,
        width,
        channels,
        std::vector<float>(static_cast<std::size_t>(width) * height * channels, 0.0F),
    };
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            for (std::uint8_t channel = 0U; channel < channels; ++channel) {
                transposed.pixels[transposed.index(y, x, channel)] = source.pixels[source.index(x, y, channel)];
            }
        }
    }

    const ResampleResult direct = resize(source, width * 8U, height * 8U, {Filter::Lanczos3, true});
    const ResampleResult swapped = resize(transposed, height * 8U, width * 8U, {Filter::Lanczos3, true});
    expect(direct.ok() && swapped.ok(), "visual-regression fixture 8x upscales succeed");
    if (!direct.ok() || !swapped.ok()) {
        return;
    }

    double maximum_axis_delta = 0.0;
    for (std::uint32_t y = 0U; y < direct.image.height; ++y) {
        for (std::uint32_t x = 0U; x < direct.image.width; ++x) {
            for (std::uint8_t channel = 0U; channel < channels; ++channel) {
                const double direct_value = static_cast<double>(direct.image.pixels[direct.image.index(x, y, channel)]);
                const double transposed_value = static_cast<double>(swapped.image.pixels[swapped.image.index(y, x, channel)]);
                maximum_axis_delta = std::max(maximum_axis_delta, std::abs(direct_value - transposed_value));
            }
        }
    }

    expect(
        maximum_axis_delta <= 1.0e-5,
        "visual regression gate: 8x reconstruction is axis-neutral without horizontal/vertical stripe bias");
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
    test_measured_roundtrip_fidelity();
    test_premultiplied_alpha_edge_does_not_leak_hidden_rgb();
    test_visual_regression_is_axis_neutral();
    test_determinism_and_validation();
    return resampler_failures;
}
