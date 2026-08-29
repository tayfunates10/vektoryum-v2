#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#include "vektoryum/hybrid/alpha_composite.hpp"

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

}  // namespace

int main() {
    using namespace vektoryum::hybrid;

    {
        const std::array<RgbaSample, 2> layers{{
            {1.0, 0.0, 0.0, 0.5},
            {0.0, 0.0, 1.0, 0.5},
        }};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.ok(), "valid layers composite successfully");
        expect_near(result.output.a, 0.75, 1e-12, "source-over alpha is deterministic");
        expect_near(result.output.r, 1.0 / 3.0, 1e-12, "source-over red is deterministic");
        expect_near(result.output.g, 0.0, 1e-12, "source-over green is deterministic");
        expect_near(result.output.b, 2.0 / 3.0, 1e-12, "source-over blue is deterministic");
    }

    {
        const std::array<RgbaSample, 1> layers{{{0.0, 0.0, 0.0, 0.0}}};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.ok(), "fully transparent black is valid");
        expect_true(result.output.a == 0.0 && result.output.r == 0.0 && result.output.g == 0.0 &&
                        result.output.b == 0.0,
                    "transparent output has no hidden RGB");
    }

    {
        const std::array<RgbaSample, 1> layers{{{1.0, 0.25, 0.5, 0.0}}};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.error == AlphaCompositeError::HiddenRgbInTransparentPixel,
                    "hidden RGB in transparent source fails closed");
    }

    {
        const std::array<RgbaSample, 1> layers{{
            {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 1.0},
        }};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.error == AlphaCompositeError::InvalidChannel,
                    "non-finite color channel fails closed");
    }

    {
        const std::array<RgbaSample, 1> layers{{{0.0, 0.0, 0.0, 1.01}}};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.error == AlphaCompositeError::InvalidChannel,
                    "out-of-range alpha fails closed");
    }

    {
        const std::array<RgbaSample, 0> layers{};
        const AlphaCompositeResult result = composite_alpha_safe(layers);
        expect_true(result.error == AlphaCompositeError::EmptyLayers,
                    "empty composition fails closed");
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
