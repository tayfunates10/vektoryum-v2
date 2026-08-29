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

[[nodiscard]] RestorationError validate_source(const resample::FloatImage& source) noexcept {
    if (!valid_image_shape(source)) {
        return RestorationError::InvalidImage;
    }
    if (source.channels != 1U && source.channels != 3U && source.channels != 4U) {
        return RestorationError::InvalidChannelCount;
    }
    for (const float sample : source.pixels) {
        if (!std::isfinite(sample)) {
            return RestorationError::NonFiniteSample;
        }
        if (sample < 0.0F || sample > 1.0F) {
            return RestorationError::SampleOutOfRange;
        }
    }
    return RestorationError::None;
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

[[nodiscard]] float local_min(const resample::FloatImage& image,
                              std::uint32_t x,
                              std::uint32_t y,
                              std::uint8_t channel) noexcept {
    float value = image.pixels[image.index(x, y, channel)];
    const std::uint32_t x0 = x == 0U ? 0U : x - 1U;
    const std::uint32_t y0 = y == 0U ? 0U : y - 1U;
    const std::uint32_t x1 = std::min<std::uint32_t>(image.width - 1U, x + 1U);
    const std::uint32_t y1 = std::min<std::uint32_t>(image.height - 1U, y + 1U);
    for (std::uint32_t yy = y0; yy <= y1; ++yy) {
        for (std::uint32_t xx = x0; xx <= x1; ++xx) {
            value = std::min(value, image.pixels[image.index(xx, yy, channel)]);
        }
    }
    return value;
}

[[nodiscard]] float local_max(const resample::FloatImage& image,
                              std::uint32_t x,
                              std::uint32_t y,
                              std::uint8_t channel) noexcept {
    float value = image.pixels[image.index(x, y, channel)];
    const std::uint32_t x0 = x == 0U ? 0U : x - 1U;
    const std::uint32_t y0 = y == 0U ? 0U : y - 1U;
    const std::uint32_t x1 = std::min<std::uint32_t>(image.width - 1U, x + 1U);
    const std::uint32_t y1 = std::min<std::uint32_t>(image.height - 1U, y + 1U);
    for (std::uint32_t yy = y0; yy <= y1; ++yy) {
        for (std::uint32_t xx = x0; xx <= x1; ++xx) {
            value = std::max(value, image.pixels[image.index(xx, yy, channel)]);
        }
    }
    return value;
}

void deblock_axis(resample::FloatImage& image, bool vertical, float strength) {
    if (strength <= 0.0F) {
        return;
    }
    const std::uint8_t processed_channels = image.channels == 4U ? 3U : image.channels;
    const std::uint32_t primary = vertical ? image.width : image.height;
    const std::uint32_t secondary = vertical ? image.height : image.width;
    if (primary < 3U) {
        return;
    }
    resample::FloatImage source = image;
    for (std::uint32_t boundary = 8U; boundary < primary; boundary += 8U) {
        if (boundary == 0U || boundary >= primary) {
            continue;
        }
        for (std::uint32_t s = 0U; s < secondary; ++s) {
            const std::uint32_t lx = vertical ? boundary - 1U : s;
            const std::uint32_t ly = vertical ? s : boundary - 1U;
            const std::uint32_t rx = vertical ? boundary : s;
            const std::uint32_t ry = vertical ? s : boundary;
            for (std::uint8_t c = 0U; c < processed_channels; ++c) {
                const float left = source.pixels[source.index(lx, ly, c)];
                const float right = source.pixels[source.index(rx, ry, c)];
                const float jump = std::abs(right - left);
                if (jump > 0.25F) {
                    continue;
                }
                const float mean = 0.5F * (left + right);
                const float lmin = local_min(source, lx, ly, c);
                const float lmax = local_max(source, lx, ly, c);
                const float rmin = local_min(source, rx, ry, c);
                const float rmax = local_max(source, rx, ry, c);
                image.pixels[image.index(lx, ly, c)] = clamp_processed_sample(
                    source, lx, ly, std::clamp(left + strength * 0.5F * (mean - left), lmin, lmax));
                image.pixels[image.index(rx, ry, c)] = clamp_processed_sample(
                    source, rx, ry, std::clamp(right + strength * 0.5F * (mean - right), rmin, rmax));
            }
        }
    }
}

[[nodiscard]] float bilinear(float v00, float v10, float v01, float v11, float tx, float ty) noexcept {
    const float top = v00 + tx * (v10 - v00);
    const float bottom = v01 + tx * (v11 - v01);
    return top + ty * (bottom - top);
}

}  // namespace

PhotoRestorationResult restore_photo(
    const resample::FloatImage& source,
    PhotoRestorationOptions options) {
    PhotoRestorationResult result{};
    result.error = validate_source(source);
    if (result.error != RestorationError::None) {
        return result;
    }
    if (!std::isfinite(options.denoise_strength) || !std::isfinite(options.deblock_strength) ||
        !std::isfinite(options.sharpen_strength) || options.denoise_strength < 0.0F ||
        options.denoise_strength > 1.0F || options.deblock_strength < 0.0F ||
        options.deblock_strength > 1.0F || options.sharpen_strength < 0.0F ||
        options.sharpen_strength > 1.0F) {
        result.error = RestorationError::InvalidOption;
        return result;
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

    deblock_axis(denoised, true, options.deblock_strength);
    deblock_axis(denoised, false, options.deblock_strength);

    for (std::uint32_t y = 0U; y < source.height; ++y) {
        for (std::uint32_t x = 0U; x < source.width; ++x) {
            for (std::uint8_t c = 0U; c < processed_channels; ++c) {
                const std::size_t index = source.index(x, y, c);
                const float base = denoised.pixels[index];
                const float local = filtered_mean(denoised, x, y, c);
                const float candidate = base + options.sharpen_strength * (base - local);
                const float bounded = std::clamp(candidate,
                                                 local_min(source, x, y, c),
                                                 local_max(source, x, y, c));
                result.image.pixels[index] = clamp_processed_sample(source, x, y, bounded);
            }
            if (source.channels == 4U) {
                const std::size_t alpha_index = source.index(x, y, 3U);
                result.image.pixels[alpha_index] = source.pixels[alpha_index];
            }
        }
    }

    return result;
}

PhotoRestorationResult super_resolve_photo(
    const resample::FloatImage& source,
    SuperResolutionOptions options) {
    PhotoRestorationResult result{};
    result.error = validate_source(source);
    if (result.error != RestorationError::None) {
        return result;
    }
    if ((options.scale != 2U && options.scale != 4U) || options.max_output_pixels == 0U) {
        result.error = RestorationError::InvalidOption;
        return result;
    }
    const std::uint64_t output_width = static_cast<std::uint64_t>(source.width) * options.scale;
    const std::uint64_t output_height = static_cast<std::uint64_t>(source.height) * options.scale;
    if (output_width > std::numeric_limits<std::uint32_t>::max() ||
        output_height > std::numeric_limits<std::uint32_t>::max() ||
        output_width > options.max_output_pixels / output_height) {
        result.error = RestorationError::OutputTooLarge;
        return result;
    }
    const std::uint64_t output_pixels = output_width * output_height;
    if (output_pixels > options.max_output_pixels ||
        output_pixels > std::numeric_limits<std::size_t>::max() / source.channels) {
        result.error = RestorationError::OutputTooLarge;
        return result;
    }

    result.image.width = static_cast<std::uint32_t>(output_width);
    result.image.height = static_cast<std::uint32_t>(output_height);
    result.image.channels = source.channels;
    result.image.pixels.resize(static_cast<std::size_t>(output_pixels) * source.channels);

    for (std::uint32_t y = 0U; y < result.image.height; ++y) {
        const float sy = (static_cast<float>(y) + 0.5F) / static_cast<float>(options.scale) - 0.5F;
        const float sy_clamped = std::clamp(sy, 0.0F, static_cast<float>(source.height - 1U));
        const auto y0 = static_cast<std::uint32_t>(std::floor(sy_clamped));
        const auto y1 = std::min<std::uint32_t>(source.height - 1U, y0 + 1U);
        const float ty = sy_clamped - static_cast<float>(y0);
        for (std::uint32_t x = 0U; x < result.image.width; ++x) {
            const float sx = (static_cast<float>(x) + 0.5F) / static_cast<float>(options.scale) - 0.5F;
            const float sx_clamped = std::clamp(sx, 0.0F, static_cast<float>(source.width - 1U));
            const auto x0 = static_cast<std::uint32_t>(std::floor(sx_clamped));
            const auto x1 = std::min<std::uint32_t>(source.width - 1U, x0 + 1U);
            const float tx = sx_clamped - static_cast<float>(x0);

            for (std::uint8_t c = 0U; c < source.channels; ++c) {
                const float v00 = source.pixels[source.index(x0, y0, c)];
                const float v10 = source.pixels[source.index(x1, y0, c)];
                const float v01 = source.pixels[source.index(x0, y1, c)];
                const float v11 = source.pixels[source.index(x1, y1, c)];
                const float minimum = std::min(std::min(v00, v10), std::min(v01, v11));
                const float maximum = std::max(std::max(v00, v10), std::max(v01, v11));
                float value = std::clamp(bilinear(v00, v10, v01, v11, tx, ty), minimum, maximum);
                if (source.channels == 4U && c < 3U) {
                    const float a00 = source.pixels[source.index(x0, y0, 3U)];
                    const float a10 = source.pixels[source.index(x1, y0, 3U)];
                    const float a01 = source.pixels[source.index(x0, y1, 3U)];
                    const float a11 = source.pixels[source.index(x1, y1, 3U)];
                    const float alpha = clamp01(bilinear(a00, a10, a01, a11, tx, ty));
                    value = std::clamp(value, 0.0F, alpha);
                }
                result.image.pixels[result.image.index(x, y, c)] = clamp01(value);
            }
        }
    }

    return result;
}

}  // namespace vektoryum::restoration
