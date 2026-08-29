#include "vektoryum/restoration/photo_restoration.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace vektoryum::restoration {
namespace {

[[nodiscard]] float clamp01(float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] bool valid_image_shape(const resample::FloatImage& image) noexcept {
    if (image.width == 0U || image.height == 0U || image.channels == 0U) {
        return false;
    }
    const auto width = static_cast<std::size_t>(image.width);
    const auto height = static_cast<std::size_t>(image.height);
    const auto channels = static_cast<std::size_t>(image.channels);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (width > maximum / height) {
        return false;
    }
    const std::size_t pixels = width * height;
    if (pixels > maximum / channels) {
        return false;
    }
    return pixels * channels == image.pixels.size();
}

[[nodiscard]] float neighborhood_mean(
    const resample::FloatImage& image,
    std::uint32_t x,
    std::uint32_t y,
    std::uint8_t channel) noexcept {
    double sum = 0.0;
    std::uint32_t count = 0U;
    const std::uint32_t x0 = x == 0U ? 0U : x - 1U;
    const std::uint32_t y0 = y == 0U ? 0U : y - 1U;
    const std::uint32_t x1 = std::min<std::uint32_t>(image.width - 1U, x + 1U);
    const std::uint32_t y1 = std::min<std::uint32_t>(image.height - 1U, y + 1U);
    for (std::uint32_t yy = y0; yy <= y1; ++yy) {
        for (std::uint32_t xx = x0; xx <= x1; ++xx) {
            sum += static_cast<double>(image.pixels[image.index(xx, yy, channel)]);
            ++count;
        }
    }
    return static_cast<float>(sum / static_cast<double>(count));
}

[[nodiscard]] float premultiplied_neighborhood_mean(
    const resample::FloatImage& image,
    std::uint32_t x,
    std::uint32_t y,
    std::uint8_t channel) noexcept {
    double rgb_sum = 0.0;
    double alpha_sum = 0.0;
    const std::uint32_t x0 = x == 0U ? 0U : x - 1U;
    const std::uint32_t y0 = y == 0U ? 0U : y - 1U;
    const std::uint32_t x1 = std::min<std::uint32_t>(image.width - 1U, x + 1U);
    const std::uint32_t y1 = std::min<std::uint32_t>(image.height - 1U, y + 1U);
    for (std::uint32_t yy = y0; yy <= y1; ++yy) {
        for (std::uint32_t xx = x0; xx <= x1; ++xx) {
            rgb_sum += static_cast<double>(image.pixels[image.index(xx, yy, channel)]);
            alpha_sum += static_cast<double>(image.pixels[image.index(xx, yy, 3U)]);
        }
    }

    const float target_alpha = image.pixels[image.index(x, y, 3U)];
    if (target_alpha <= 0.0F || alpha_sum <= 0.0) {
        return 0.0F;
    }
    const double straight_mean = rgb_sum / alpha_sum;
    return std::clamp(static_cast<float>(straight_mean * static_cast<double>(target_alpha)),
                      0.0F,
                      target_alpha);
}

[[nodiscard]] float filtered_mean(
    const resample::FloatImage& image,
    std::uint32_t x,
    std::uint32_t y,
    std::uint8_t channel) noexcept {
    if (image.channels == 4U && channel < 3U) {
        return premultiplied_neighborhood_mean(image, x, y, channel);
    }
    return neighborhood_mean(image, x, y, channel);
}

[[nodiscard]] float clamp_processed_sample(
    const resample::FloatImage& image,
    std::uint32_t x,
    std::uint32_t y,
    float value) noexcept {
    if (image.channels == 4U) {
        const float alpha = image.pixels[image.index(x, y, 3U)];
        return std::clamp(value, 0.0F, alpha);
    }
    return clamp01(value);
}

}  // namespace

PhotoRestorationResult restore_photo(
    const resample::FloatImage& source,
    PhotoRestorationOptions options) {
    PhotoRestorationResult result{};
    if (!valid_image_shape(source)) {
        result.error = RestorationError::InvalidImage;
        return result;
    }
    if (source.channels != 1U && source.channels != 3U && source.channels != 4U) {
        result.error = RestorationError::InvalidChannelCount;
        return result;
    }
    if (!std::isfinite(options.denoise_strength) || !std::isfinite(options.sharpen_strength) ||
        options.denoise_strength < 0.0F || options.denoise_strength > 1.0F ||
        options.sharpen_strength < 0.0F || options.sharpen_strength > 1.0F) {
        result.error = RestorationError::InvalidOption;
        return result;
    }
    for (const float sample : source.pixels) {
        if (!std::isfinite(sample)) {
            result.error = RestorationError::NonFiniteSample;
            return result;
        }
        if (sample < 0.0F || sample > 1.0F) {
            result.error = RestorationError::SampleOutOfRange;
            return result;
        }
    }

    result.image = source;
    resample::FloatImage denoised = source;
    const std::uint8_t processed_channels = source.channels == 4U ? 3U : source.channels;

    for (std::uint32_t y = 0U; y < source.height; ++y) {
        for (std::uint32_t x = 0U; x < source.width; ++x) {
            for (std::uint8_t c = 0U; c < processed_channels; ++c) {
                const std::size_t index = source.index(x, y, c);
                const float mean = filtered_mean(source, x, y, c);
                denoised.pixels[index] = clamp_processed_sample(
                    source,
                    x,
                    y,
                    source.pixels[index] + options.denoise_strength * (mean - source.pixels[index]));
            }
        }
    }

    for (std::uint32_t y = 0U; y < source.height; ++y) {
        for (std::uint32_t x = 0U; x < source.width; ++x) {
            for (std::uint8_t c = 0U; c < processed_channels; ++c) {
                const std::size_t index = source.index(x, y, c);
                const float base = denoised.pixels[index];
                const float local = filtered_mean(denoised, x, y, c);
                result.image.pixels[index] = clamp_processed_sample(
                    source,
                    x,
                    y,
                    base + options.sharpen_strength * (base - local));
            }
            if (source.channels == 4U) {
                const std::size_t alpha_index = source.index(x, y, 3U);
                result.image.pixels[alpha_index] = source.pixels[alpha_index];
            }
        }
    }

    return result;
}

}  // namespace vektoryum::restoration
