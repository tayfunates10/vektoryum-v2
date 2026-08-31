#include "vektoryum/resample/resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace vektoryum::resample {
namespace {

struct Tap {
    std::uint32_t source_index{};
    double weight{};
};

using TapTable = std::vector<std::vector<Tap>>;

[[nodiscard]] bool checked_sample_count(
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t channels,
    std::size_t& result) noexcept {
    if (width == 0U || height == 0U || channels == 0U) {
        return false;
    }
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const auto c = static_cast<std::size_t>(channels);
    const auto max = std::numeric_limits<std::size_t>::max();
    if (w > max / h) {
        return false;
    }
    const std::size_t pixels = w * h;
    if (pixels > max / c) {
        return false;
    }
    result = pixels * c;
    return true;
}

[[nodiscard]] double sinc(double value) noexcept {
    if (std::abs(value) < 1.0e-12) {
        return 1.0;
    }
    const double scaled = std::numbers::pi_v<double> * value;
    return std::sin(scaled) / scaled;
}

[[nodiscard]] double kernel_value(Filter filter, double distance) noexcept {
    const double x = std::abs(distance);
    switch (filter) {
        case Filter::Bilinear:
            return x < 1.0 ? 1.0 - x : 0.0;
        case Filter::Bicubic: {
            constexpr double a = -0.5;
            if (x < 1.0) {
                return ((a + 2.0) * x * x * x) - ((a + 3.0) * x * x) + 1.0;
            }
            if (x < 2.0) {
                return (a * x * x * x) - (5.0 * a * x * x) + (8.0 * a * x) - (4.0 * a);
            }
            return 0.0;
        }
        case Filter::Lanczos3:
            return x < 3.0 ? sinc(x) * sinc(x / 3.0) : 0.0;
    }
    return 0.0;
}

[[nodiscard]] double kernel_support(Filter filter) noexcept {
    switch (filter) {
        case Filter::Bilinear:
            return 1.0;
        case Filter::Bicubic:
            return 2.0;
        case Filter::Lanczos3:
            return 3.0;
    }
    return 1.0;
}

[[nodiscard]] std::uint32_t clamp_source_index(std::int64_t index, std::uint32_t source_extent) noexcept {
    if (index <= 0) {
        return 0U;
    }
    const auto max_index = static_cast<std::int64_t>(source_extent - 1U);
    if (index >= max_index) {
        return source_extent - 1U;
    }
    return static_cast<std::uint32_t>(index);
}

[[nodiscard]] TapTable build_taps(
    std::uint32_t source_extent,
    std::uint32_t target_extent,
    Filter filter) {
    const double scale = static_cast<double>(target_extent) / static_cast<double>(source_extent);
    const double filter_scale = std::min(1.0, scale);
    const double support = kernel_support(filter) / filter_scale;
    TapTable table(static_cast<std::size_t>(target_extent));

    for (std::uint32_t destination = 0U; destination < target_extent; ++destination) {
        const double source_center = (static_cast<double>(destination) + 0.5) / scale - 0.5;
        const auto begin = static_cast<std::int64_t>(std::ceil(source_center - support));
        const auto end = static_cast<std::int64_t>(std::floor(source_center + support));
        auto& taps = table[static_cast<std::size_t>(destination)];
        taps.reserve(static_cast<std::size_t>(end - begin + 1));

        double sum = 0.0;
        for (std::int64_t source = begin; source <= end; ++source) {
            const double distance = (source_center - static_cast<double>(source)) * filter_scale;
            const double weight = kernel_value(filter, distance) * filter_scale;
            if (std::abs(weight) <= 1.0e-15) {
                continue;
            }
            taps.push_back({clamp_source_index(source, source_extent), weight});
            sum += weight;
        }

        if (taps.empty() || std::abs(sum) <= 1.0e-15) {
            const auto nearest = static_cast<std::int64_t>(std::llround(source_center));
            taps.assign(1U, Tap{clamp_source_index(nearest, source_extent), 1.0});
            continue;
        }
        for (Tap& tap : taps) {
            tap.weight /= sum;
        }
    }
    return table;
}

[[nodiscard]] float clamp_local(float value, float minimum, float maximum, bool enabled) noexcept {
    return enabled ? std::clamp(value, minimum, maximum) : value;
}

}  // namespace

bool FloatImage::valid() const noexcept {
    if (channels == 0U || channels > 4U) {
        return false;
    }
    std::size_t expected = 0U;
    return checked_sample_count(width, height, channels, expected) && expected == pixels.size() &&
           std::all_of(pixels.begin(), pixels.end(), [](float value) { return std::isfinite(value); });
}

std::size_t FloatImage::index(
    std::uint32_t x,
    std::uint32_t y,
    std::uint8_t channel) const noexcept {
    const auto row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    const auto pixel = row + static_cast<std::size_t>(x);
    return pixel * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel);
}

ResampleResult resize(
    const FloatImage& source,
    std::uint32_t target_width,
    std::uint32_t target_height,
    ResampleOptions options) {
    if (source.width == 0U || source.height == 0U) {
        return {ResampleError::ZeroSourceDimension, {}};
    }
    if (target_width == 0U || target_height == 0U) {
        return {ResampleError::ZeroTargetDimension, {}};
    }
    if (source.channels == 0U || source.channels > 4U) {
        return {ResampleError::InvalidChannelCount, {}};
    }

    std::size_t source_samples = 0U;
    if (!checked_sample_count(source.width, source.height, source.channels, source_samples)) {
        return {ResampleError::SizeOverflow, {}};
    }
    if (source_samples != source.pixels.size()) {
        return {ResampleError::SourceSizeMismatch, {}};
    }
    if (!std::all_of(source.pixels.begin(), source.pixels.end(), [](float value) { return std::isfinite(value); })) {
        return {ResampleError::NonFiniteSample, {}};
    }

    std::size_t target_samples = 0U;
    if (!checked_sample_count(target_width, target_height, source.channels, target_samples)) {
        return {ResampleError::SizeOverflow, {}};
    }
    if (target_width == source.width && target_height == source.height) {
        return {ResampleError::None, source};
    }

    const TapTable horizontal_taps = build_taps(source.width, target_width, options.filter);
    const TapTable vertical_taps = build_taps(source.height, target_height, options.filter);

    std::size_t horizontal_samples = 0U;
    if (!checked_sample_count(target_width, source.height, source.channels, horizontal_samples)) {
        return {ResampleError::SizeOverflow, {}};
    }
    FloatImage horizontal{target_width, source.height, source.channels, std::vector<float>(horizontal_samples, 0.0F)};

    for (std::uint32_t y = 0U; y < source.height; ++y) {
        for (std::uint32_t x = 0U; x < target_width; ++x) {
            const auto& taps = horizontal_taps[static_cast<std::size_t>(x)];
            for (std::uint8_t channel = 0U; channel < source.channels; ++channel) {
                double weighted = 0.0;
                for (const Tap& tap : taps) {
                    const float sample = source.pixels[source.index(tap.source_index, y, channel)];
                    weighted += static_cast<double>(sample) * tap.weight;
                }
                horizontal.pixels[horizontal.index(x, y, channel)] = static_cast<float>(weighted);
            }
        }
    }

    FloatImage output{target_width, target_height, source.channels, std::vector<float>(target_samples, 0.0F)};
    for (std::uint32_t y = 0U; y < target_height; ++y) {
        const auto& y_taps = vertical_taps[static_cast<std::size_t>(y)];
        for (std::uint32_t x = 0U; x < target_width; ++x) {
            const auto& x_taps = horizontal_taps[static_cast<std::size_t>(x)];
            for (std::uint8_t channel = 0U; channel < source.channels; ++channel) {
                double weighted = 0.0;
                for (const Tap& tap : y_taps) {
                    const float sample = horizontal.pixels[horizontal.index(x, tap.source_index, channel)];
                    weighted += static_cast<double>(sample) * tap.weight;
                }

                float local_min = std::numeric_limits<float>::infinity();
                float local_max = -std::numeric_limits<float>::infinity();
                if (options.clamp_to_local_range) {
                    for (const Tap& y_tap : y_taps) {
                        for (const Tap& x_tap : x_taps) {
                            const float sample = source.pixels[source.index(
                                x_tap.source_index,
                                y_tap.source_index,
                                channel)];
                            local_min = std::min(local_min, sample);
                            local_max = std::max(local_max, sample);
                        }
                    }
                }
                output.pixels[output.index(x, y, channel)] = clamp_local(
                    static_cast<float>(weighted), local_min, local_max, options.clamp_to_local_range);
            }
        }
    }
    return {ResampleError::None, std::move(output)};
}

}  // namespace vektoryum::resample
