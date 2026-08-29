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
        {0.0F, 0.05F, 0.10F, 0.1F,
         0.3F, 0.2F, 0.1F, 0.3F,
         0.1F, 0.3F, 0.5F, 0.7F,
         0.9F, 0.7F, 0.5F, 1.0F}};
    const auto rgba_restored = restoration::restore_photo(rgba, {1.0F, 1.0F, 1.0F});
    expect_true(rgba_restored.ok(), "RGBA restoration succeeds");
    bool alpha_exact = true;
    bool bounded = true;
    bool premultiplied = true;
    for (std::uint32_t y = 0U; y < rgba.height; ++y) {
        for (std::uint32_t x = 0U; x < rgba.width; ++x) {
            const auto a = rgba.index(x, y, 3U);
            alpha_exact = alpha_exact && rgba_restored.image.pixels[a] == rgba.pixels[a];
            for (std::uint8_t c = 0U; c < 3U; ++c) {
                premultiplied = premultiplied && rgba_restored.image.pixels[rgba.index(x, y, c)] <=
                    rgba_restored.image.pixels[a];
            }
        }
    }
    for (const float sample : rgba_restored.image.pixels) {
        bounded = bounded && sample >= 0.0F && sample <= 1.0F;
    }
    expect_true(alpha_exact, "restoration preserves alpha exactly");
    expect_true(bounded, "restoration output remains normalized");
    expect_true(premultiplied, "restoration preserves premultiplied-alpha invariant");

    resample::FloatImage premult_edge{2U, 1U, 4U,
        {0.0F, 0.0F, 0.0F, 0.0F,
         1.0F, 0.0F, 0.0F, 1.0F}};
    const auto premult_restored = restoration::restore_photo(premult_edge, {1.0F, 1.0F, 1.0F});
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

    resample::FloatImage step{9U, 1U, 1U,
        {0.40F, 0.40F, 0.40F, 0.40F, 0.40F, 0.40F, 0.40F, 0.40F, 0.55F}};
    const auto deblocked = restoration::restore_photo(step, {0.0F, 1.0F, 0.0F});
    expect_true(deblocked.ok(), "artifact-aware deblock succeeds");
    expect_true(deblocked.image.pixels[7] > 0.40F && deblocked.image.pixels[8] < 0.55F,
                "bounded deblock softens modest 8-pixel block seam");

    resample::FloatImage hard_edge{9U, 1U, 1U,
        {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}};
    const auto protected_edge = restoration::restore_photo(hard_edge, {0.0F, 1.0F, 0.0F});
    expect_true(protected_edge.ok() && protected_edge.image.pixels == hard_edge.pixels,
                "deblock preserves strong real edge instead of blurring it");

    resample::FloatImage impulse{3U, 3U, 1U,
        {0.2F, 0.2F, 0.2F,
         0.2F, 0.8F, 0.2F,
         0.2F, 0.2F, 0.2F}};
    const auto sharpened = restoration::restore_photo(impulse, {0.0F, 0.0F, 1.0F});
    bool no_overshoot = sharpened.ok();
    for (const float sample : sharpened.image.pixels) {
        no_overshoot = no_overshoot && sample >= 0.2F && sample <= 0.8F;
    }
    expect_true(no_overshoot, "sharpening cannot ring outside local source envelope");

    resample::FloatImage sr_source{2U, 2U, 3U,
        {0.0F, 0.0F, 0.0F,
         1.0F, 0.0F, 0.0F,
         0.0F, 1.0F, 0.0F,
         0.0F, 0.0F, 1.0F}};
    const auto sr2 = restoration::super_resolve_photo(sr_source, {2U, 64U});
    const auto sr2_repeat = restoration::super_resolve_photo(sr_source, {2U, 64U});
    expect_true(sr2.ok() && sr2.image.width == 4U && sr2.image.height == 4U,
                "non-ML SR produces deterministic 2x geometry");
    expect_true(sr2_repeat.ok() && sr2_repeat.image.pixels == sr2.image.pixels,
                "non-ML SR is deterministic");
    bool sr_bounded = true;
    for (const float sample : sr2.image.pixels) {
        sr_bounded = sr_bounded && sample >= 0.0F && sample <= 1.0F;
    }
    expect_true(sr_bounded, "non-ML SR has no interpolation overshoot");

    resample::FloatImage sr_alpha{2U, 1U, 4U,
        {0.0F, 0.0F, 0.0F, 0.0F,
         1.0F, 0.0F, 0.0F, 1.0F}};
    const auto sr_alpha_out = restoration::super_resolve_photo(sr_alpha, {4U, 64U});
    bool sr_alpha_safe = sr_alpha_out.ok();
    for (std::uint32_t x = 0U; sr_alpha_safe && x < sr_alpha_out.image.width; ++x) {
        const float alpha = sr_alpha_out.image.pixels[sr_alpha_out.image.index(x, 0U, 3U)];
        for (std::uint8_t c = 0U; c < 3U; ++c) {
            sr_alpha_safe = sr_alpha_safe &&
                sr_alpha_out.image.pixels[sr_alpha_out.image.index(x, 0U, c)] <= alpha;
        }
    }
    expect_true(sr_alpha_safe, "non-ML SR preserves premultiplied-alpha bounds");

    auto non_finite = constant;
    non_finite.pixels[0] = std::numeric_limits<float>::quiet_NaN();
    expect_true(restoration::restore_photo(non_finite).error == restoration::RestorationError::NonFiniteSample,
                "non-finite restoration sample is rejected");
    expect_true(restoration::super_resolve_photo(non_finite).error == restoration::RestorationError::NonFiniteSample,
                "non-finite SR sample is rejected");

    auto out_of_range = constant;
    out_of_range.pixels[0] = 1.1F;
    expect_true(restoration::restore_photo(out_of_range).error == restoration::RestorationError::SampleOutOfRange,
                "out-of-range restoration sample is rejected");

    expect_true(restoration::restore_photo(constant, {-0.1F, 0.2F, 0.2F}).error ==
                    restoration::RestorationError::InvalidOption,
                "invalid restoration option is rejected");
    expect_true(restoration::super_resolve_photo(constant, {3U, 1024U}).error ==
                    restoration::RestorationError::InvalidOption,
                "unsupported SR scale is rejected");
    expect_true(restoration::super_resolve_photo(constant, {4U, 16U}).error ==
                    restoration::RestorationError::OutputTooLarge,
                "SR output pixel budget fails closed");

    resample::FloatImage invalid_channels{2U, 2U, 2U, std::vector<float>(8U, 0.5F)};
    expect_true(restoration::restore_photo(invalid_channels).error ==
                    restoration::RestorationError::InvalidChannelCount,
                "unsupported restoration channel count is rejected");

    return failures;
}
