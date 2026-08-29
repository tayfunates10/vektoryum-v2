#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

#include "vektoryum/restoration/photo_restoration.hpp"

namespace {
int failures = 0;
void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    } else {
        std::cout << "[PASS] " << name << '\n';
    }
}
}

int run_photo_restoration_tests() {
    using namespace vektoryum;

    resample::FloatImage constant{4U, 4U, 3U, std::vector<float>(4U * 4U * 3U, 0.5F)};
    const auto restored = restoration::restore_photo(constant);
    expect_true(restored.ok(), "photo restoration succeeds for normalized RGB");
    bool constant_preserved = restored.image.pixels.size() == constant.pixels.size();
    for (const float sample : restored.image.pixels) {
        constant_preserved = constant_preserved && std::abs(sample - 0.5F) <= 1.0e-6F;
    }
    expect_true(constant_preserved, "constant field is preserved exactly enough");

    const auto repeat = restoration::restore_photo(constant);
    expect_true(repeat.ok() && repeat.image.pixels == restored.image.pixels,
                "photo restoration is deterministic");

    resample::FloatImage rgba{2U, 2U, 4U,
        {0.0F, 0.2F, 0.4F, 0.1F,
         1.0F, 0.8F, 0.6F, 0.3F,
         0.1F, 0.3F, 0.5F, 0.7F,
         0.9F, 0.7F, 0.5F, 1.0F}};
    const auto rgba_restored = restoration::restore_photo(rgba, {1.0F, 1.0F});
    expect_true(rgba_restored.ok(), "RGBA restoration succeeds");
    bool alpha_exact = true;
    bool bounded = true;
    for (std::uint32_t y = 0U; y < rgba.height; ++y) {
        for (std::uint32_t x = 0U; x < rgba.width; ++x) {
            const auto a = rgba.index(x, y, 3U);
            alpha_exact = alpha_exact && rgba_restored.image.pixels[a] == rgba.pixels[a];
        }
    }
    for (const float sample : rgba_restored.image.pixels) {
        bounded = bounded && sample >= 0.0F && sample <= 1.0F;
    }
    expect_true(alpha_exact, "restoration preserves alpha exactly");
    expect_true(bounded, "restoration output remains normalized");

    resample::FloatImage premult_edge{2U, 1U, 4U,
        {0.0F, 0.0F, 0.0F, 0.0F,
         1.0F, 0.0F, 0.0F, 1.0F}};
    const auto premult_restored = restoration::restore_photo(premult_edge, {1.0F, 1.0F});
    expect_true(premult_restored.ok(), "premultiplied RGBA edge restoration succeeds");
    bool premultiplied_invariant = true;
    for (std::uint32_t x = 0U; x < premult_edge.width; ++x) {
        const float alpha = premult_restored.image.pixels[premult_edge.index(x, 0U, 3U)];
        for (std::uint8_t c = 0U; c < 3U; ++c) {
            premultiplied_invariant = premultiplied_invariant &&
                premult_restored.image.pixels[premult_edge.index(x, 0U, c)] <= alpha;
        }
    }
    expect_true(premultiplied_invariant,
                "premultiplied RGBA filtering never creates RGB above alpha");
    expect_true(premult_restored.image.pixels[premult_edge.index(0U, 0U, 0U)] == 0.0F,
                "transparent pixel remains colorless across opaque edge");

    auto non_finite = constant;
    non_finite.pixels[0] = std::numeric_limits<float>::quiet_NaN();
    expect_true(restoration::restore_photo(non_finite).error == restoration::RestorationError::NonFiniteSample,
                "non-finite restoration sample is rejected");

    auto out_of_range = constant;
    out_of_range.pixels[0] = 1.1F;
    expect_true(restoration::restore_photo(out_of_range).error == restoration::RestorationError::SampleOutOfRange,
                "out-of-range restoration sample is rejected");

    expect_true(restoration::restore_photo(constant, {-0.1F, 0.2F}).error ==
                    restoration::RestorationError::InvalidOption,
                "invalid restoration option is rejected");

    resample::FloatImage invalid_channels{2U, 2U, 2U, std::vector<float>(8U, 0.5F)};
    expect_true(restoration::restore_photo(invalid_channels).error ==
                    restoration::RestorationError::InvalidChannelCount,
                "unsupported restoration channel count is rejected");

    return failures;
}
