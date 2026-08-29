#include "vektoryum/ml/runtime_contract.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <limits>

namespace vektoryum::ml {
namespace {

[[nodiscard]] bool nonempty_token(const std::string& value) noexcept {
    if (value.empty()) {
        return false;
    }
    for (const char character : value) {
        const auto code_unit = static_cast<unsigned char>(character);
        if (std::iscntrl(code_unit) != 0 || std::isspace(code_unit) != 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_sha256(const std::string& value) noexcept {
    if (value.size() != 64U) {
        return false;
    }
    for (const char character : value) {
        const bool decimal = character >= '0' && character <= '9';
        const bool hex = character >= 'a' && character <= 'f';
        if (!decimal && !hex) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_model(const ModelDescriptor& left, const ModelDescriptor& right) noexcept {
    return left.model_id == right.model_id && left.model_version == right.model_version &&
           left.artifact_sha256 == right.artifact_sha256;
}

[[nodiscard]] bool same_binding(const RuntimeBinding& left, const RuntimeBinding& right) noexcept {
    return left.backend_id == right.backend_id && left.requested_provider == right.requested_provider &&
           left.active_provider == right.active_provider && left.deterministic == right.deterministic &&
           left.fallback_used == right.fallback_used;
}

[[nodiscard]] ContractValidation validate_tensor_shape(
    const TensorShape& shape, RuntimeLimits limits, RuntimeError invalid_shape_error) noexcept {
    ContractValidation result{};
    const auto max_rank = static_cast<std::size_t>(limits.max_tensor_rank);
    if (limits.max_tensor_rank == 0U || limits.max_tensor_elements == 0U ||
        shape.dimensions.empty() || shape.dimensions.size() > max_rank) {
        result.error = invalid_shape_error;
        return result;
    }

    std::uint64_t elements = 1U;
    for (const std::uint32_t extent : shape.dimensions) {
        if (extent == 0U) {
            result.error = RuntimeError::ZeroTensorExtent;
            return result;
        }
        if (elements > std::numeric_limits<std::uint64_t>::max() / extent) {
            result.error = RuntimeError::TensorElementOverflow;
            return result;
        }
        elements *= extent;
        if (elements > limits.max_tensor_elements) {
            result.error = RuntimeError::TensorBudgetExceeded;
            return result;
        }
    }
    result.tensor_elements = elements;
    return result;
}

[[nodiscard]] ContractValidation validate_binding(const RuntimeBinding& binding) noexcept {
    ContractValidation result{};
    if (!nonempty_token(binding.backend_id)) {
        result.error = RuntimeError::InvalidBackend;
        return result;
    }
    if (binding.requested_provider == ExecutionProvider::Unknown ||
        binding.active_provider == ExecutionProvider::Unknown) {
        result.error = RuntimeError::InvalidProvider;
        return result;
    }
    if (binding.fallback_used || binding.active_provider != binding.requested_provider) {
        result.error = RuntimeError::SilentFallback;
        return result;
    }
    if (!binding.deterministic) {
        result.error = RuntimeError::NonDeterministicProvider;
        return result;
    }
    return result;
}

[[nodiscard]] std::uint32_t shape_channels(const TensorShape& shape, TensorLayout layout) noexcept {
    if (shape.dimensions.size() != 4U) {
        return 0U;
    }
    if (layout == TensorLayout::Nchw) {
        return shape.dimensions[1U];
    }
    if (layout == TensorLayout::Nhwc) {
        return shape.dimensions[3U];
    }
    return 0U;
}

[[nodiscard]] bool supported_channel_count(std::uint32_t channels) noexcept {
    return channels == 1U || channels == 3U || channels == 4U;
}

[[nodiscard]] std::size_t channel_for_index(
    std::size_t index, const TensorShape& shape, TensorLayout layout) noexcept {
    if (layout == TensorLayout::Nhwc) {
        return index % static_cast<std::size_t>(shape.dimensions[3U]);
    }
    const auto plane = static_cast<std::size_t>(shape.dimensions[2U]) *
                       static_cast<std::size_t>(shape.dimensions[3U]);
    return (index / plane) % static_cast<std::size_t>(shape.dimensions[1U]);
}

[[nodiscard]] bool reference_binding(const RuntimeBinding& binding) noexcept {
    return binding.backend_id == "vektoryum-reference" &&
           binding.requested_provider == ExecutionProvider::Cpu &&
           binding.active_provider == ExecutionProvider::Cpu && binding.deterministic &&
           !binding.fallback_used;
}

}  // namespace

ContractValidation validate_runtime_contract(
    const ModelDescriptor& model, const TensorShape& input, RuntimeLimits limits) noexcept {
    ContractValidation result{};
    if (!nonempty_token(model.model_id) || !valid_sha256(model.artifact_sha256)) {
        result.error = RuntimeError::InvalidModelId;
        return result;
    }
    if (!nonempty_token(model.model_version)) {
        result.error = RuntimeError::InvalidModelVersion;
        return result;
    }
    return validate_tensor_shape(input, limits, RuntimeError::InvalidTensorRank);
}

ContractValidation validate_preprocess_contract(
    const PreprocessContract& preprocess, const TensorShape& input) noexcept {
    ContractValidation result{};
    if (preprocess.layout == TensorLayout::Unknown || input.dimensions.size() != 4U) {
        result.error = RuntimeError::InvalidTensorLayout;
        return result;
    }
    const auto channels = shape_channels(input, preprocess.layout);
    if (!supported_channel_count(preprocess.channels) || channels != preprocess.channels) {
        result.error = RuntimeError::InvalidChannelContract;
        return result;
    }
    const auto expected = static_cast<std::size_t>(preprocess.channels);
    if (preprocess.mean.size() != expected || preprocess.scale.size() != expected) {
        result.error = RuntimeError::InvalidNormalization;
        return result;
    }
    for (std::size_t index = 0U; index < expected; ++index) {
        if (!std::isfinite(preprocess.mean[index]) || !std::isfinite(preprocess.scale[index]) ||
            preprocess.scale[index] == 0.0F) {
            result.error = RuntimeError::InvalidNormalization;
            return result;
        }
    }
    return result;
}

ContractValidation validate_postprocess_contract(
    const PostprocessContract& postprocess, const TensorShape& output) noexcept {
    ContractValidation result{};
    if (postprocess.layout == TensorLayout::Unknown || output.dimensions.size() != 4U) {
        result.error = RuntimeError::InvalidTensorLayout;
        return result;
    }
    const auto channels = shape_channels(output, postprocess.layout);
    if (!supported_channel_count(postprocess.channels) || channels != postprocess.channels) {
        result.error = RuntimeError::InvalidChannelContract;
        return result;
    }
    if (!std::isfinite(postprocess.output_min) || !std::isfinite(postprocess.output_max) ||
        postprocess.output_min > postprocess.output_max) {
        result.error = RuntimeError::InvalidPostprocessRange;
        return result;
    }
    return result;
}

ContractValidation validate_execution_contract(
    const ExecutionContract& contract, RuntimeLimits limits) noexcept {
    auto result = validate_runtime_contract(contract.model, contract.input, limits);
    if (!result.ok()) {
        return result;
    }
    const auto binding = validate_binding(contract.binding);
    if (!binding.ok()) {
        result.error = binding.error;
        return result;
    }
    const auto output = validate_tensor_shape(contract.output, limits, RuntimeError::InvalidOutputTensor);
    if (!output.ok()) {
        result.error = output.error;
        return result;
    }
    result.output_tensor_elements = output.tensor_elements;
    const auto preprocess = validate_preprocess_contract(contract.preprocess, contract.input);
    if (!preprocess.ok()) {
        result.error = preprocess.error;
        return result;
    }
    const auto postprocess = validate_postprocess_contract(contract.postprocess, contract.output);
    if (!postprocess.ok()) {
        result.error = postprocess.error;
        return result;
    }
    return result;
}

ContractValidation validate_model_load(const ModelLoadReceipt& receipt) noexcept {
    ContractValidation result{};
    if (!receipt.loaded) {
        result.error = RuntimeError::ModelNotLoaded;
        return result;
    }
    if (!nonempty_token(receipt.model.model_id) || !nonempty_token(receipt.model.model_version) ||
        !valid_sha256(receipt.model.artifact_sha256) || !valid_sha256(receipt.loaded_artifact_sha256)) {
        result.error = RuntimeError::InvalidModelId;
        return result;
    }
    if (receipt.loaded_artifact_sha256 != receipt.model.artifact_sha256) {
        result.error = RuntimeError::ArtifactDigestMismatch;
        return result;
    }
    return validate_binding(receipt.binding);
}

ContractValidation validate_inference_request(
    const InferenceRequest& request, RuntimeLimits limits) noexcept {
    auto result = validate_execution_contract(request.contract, limits);
    if (!result.ok()) {
        return result;
    }
    const auto load = validate_model_load(request.loaded_model);
    if (!load.ok()) {
        result.error = load.error;
        return result;
    }
    if (!same_model(request.contract.model, request.loaded_model.model) ||
        !same_binding(request.contract.binding, request.loaded_model.binding)) {
        result.error = RuntimeError::ArtifactDigestMismatch;
        return result;
    }
    if (request.input_element_count != result.tensor_elements ||
        request.output_element_capacity != result.output_tensor_elements) {
        result.error = RuntimeError::TensorElementCountMismatch;
        return result;
    }
    return result;
}

RuntimeError DeterministicReferenceRuntime::load(const ModelLoadReceipt& receipt) noexcept {
    const auto validation = validate_model_load(receipt);
    if (!validation.ok()) {
        return validation.error;
    }
    if (!reference_binding(receipt.binding)) {
        return RuntimeError::UnsupportedExecution;
    }
    loaded_model_ = receipt;
    return RuntimeError::None;
}

void DeterministicReferenceRuntime::unload() noexcept {
    loaded_model_ = ModelLoadReceipt{};
}

bool DeterministicReferenceRuntime::loaded() const noexcept {
    return loaded_model_.loaded;
}

InferenceResult DeterministicReferenceRuntime::execute(
    const ExecutionContract& contract, const std::vector<float>& input, RuntimeLimits limits) const {
    InferenceResult result{};
    if (!loaded()) {
        result.error = RuntimeError::ModelNotLoaded;
        return result;
    }
    if (!reference_binding(contract.binding)) {
        result.error = RuntimeError::UnsupportedExecution;
        return result;
    }
    const auto contract_validation = validate_execution_contract(contract, limits);
    if (!contract_validation.ok()) {
        result.error = contract_validation.error;
        return result;
    }
    const InferenceRequest request{
        contract,
        loaded_model_,
        static_cast<std::uint64_t>(input.size()),
        contract_validation.output_tensor_elements};
    const auto request_validation = validate_inference_request(request, limits);
    if (!request_validation.ok()) {
        result.error = request_validation.error;
        return result;
    }
    for (const float value : input) {
        if (!std::isfinite(value)) {
            result.error = RuntimeError::NonFiniteTensorData;
            return result;
        }
    }

    const auto output_size = static_cast<std::size_t>(request_validation.output_tensor_elements);
    result.output.resize(output_size);
    for (std::size_t output_index = 0U; output_index < output_size; ++output_index) {
        const auto source_index = (output_index * input.size()) / output_size;
        const auto channel = channel_for_index(source_index, contract.input, contract.preprocess.layout);
        const float normalized =
            (input[source_index] - contract.preprocess.mean[channel]) / contract.preprocess.scale[channel];
        result.output[output_index] = std::clamp(
            normalized, contract.postprocess.output_min, contract.postprocess.output_max);
    }
    return result;
}

}  // namespace vektoryum::ml
