#include "vektoryum/hybrid/alpha_composite.hpp"

#include <cmath>

namespace vektoryum::hybrid {

namespace {

[[nodiscard]] bool valid_channel(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

[[nodiscard]] bool has_hidden_rgb(const RgbaSample& sample) noexcept {
    return sample.a == 0.0 && (sample.r != 0.0 || sample.g != 0.0 || sample.b != 0.0);
}

}  // namespace

AlphaCompositeResult composite_alpha_safe(std::span<const RgbaSample> layers) {
    if (layers.empty()) {
        return AlphaCompositeResult{AlphaCompositeError::EmptyLayers, 0U, {}};
    }

    double out_pr = 0.0;
    double out_pg = 0.0;
    double out_pb = 0.0;
    double out_a = 0.0;

    for (std::size_t index = 0U; index < layers.size(); ++index) {
        const RgbaSample& layer = layers[index];
        if (!valid_channel(layer.r) || !valid_channel(layer.g) || !valid_channel(layer.b) ||
            !valid_channel(layer.a)) {
            return AlphaCompositeResult{AlphaCompositeError::InvalidChannel, index, {}};
        }
        if (has_hidden_rgb(layer)) {
            return AlphaCompositeResult{AlphaCompositeError::HiddenRgbInTransparentPixel, index,
                                        {}};
        }

        const double source_pr = layer.r * layer.a;
        const double source_pg = layer.g * layer.a;
        const double source_pb = layer.b * layer.a;
        const double inverse_source_alpha = 1.0 - layer.a;

        out_pr = source_pr + out_pr * inverse_source_alpha;
        out_pg = source_pg + out_pg * inverse_source_alpha;
        out_pb = source_pb + out_pb * inverse_source_alpha;
        out_a = layer.a + out_a * inverse_source_alpha;
    }

    RgbaSample output{};
    output.a = out_a;
    if (out_a > 0.0) {
        output.r = out_pr / out_a;
        output.g = out_pg / out_a;
        output.b = out_pb / out_a;
    }

    return AlphaCompositeResult{AlphaCompositeError::None, layers.size(), output};
}

}  // namespace vektoryum::hybrid
