#include "vektoryum/ml/runtime_contract.hpp"

#include <cctype>
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

[[nodiscard]] bool same_model(
    const ModelDescriptor& left,
    const ModelDescriptor& right) noexcept {
    return left.model_id == right.model_id &&
           left.model_version == right.model_version &&
           left.artifact_sha256 == right.artifact_sha256;
}

[[nodiscard]] bool same_binding(
    const RuntimeBinding& left,
    const RuntimeBinding& right) noexcept {
    return left.backend_id == right.backend_id &&
           left.requested_provider == right.requested_provider &&
           left.active_provider == right.active_provider &&
           left.deterministic == right.deterministic &&
           left.fallback_used == right.fallback_used;
}

[[nodiscard]] ContractValidation validate_tensor_shape(
    const TensorShape& shape,
    RuntimeLimits limits,
    RuntimeError invalid_shape_error) noexcept {
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

[[nodiscard]] ContractValidation validate_binding(
    const RuntimeBinding& binding) noexcept {
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

}  // namespace

ContractValidation validate_runtime_contract(
    const ModelDescriptor& model,
    const TensorShape& input,
    RuntimeLimits limits) noexcept {
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

ContractValidation validate_execution_contract(
    const ExecutionContract& contract,
    RuntimeLimits limits) noexcept {
    auto result = validate_runtime_contract(contract.model, contract.input, limits);
    if (!result.ok()) {
        return result;
    }
    const auto binding = validate_binding(contract.binding);
    if (!binding.ok()) {
        result.error = binding.error;
        return result;
    }

    const auto output = validate_tensor_shape(
        contract.output, limits, RuntimeError::InvalidOutputTensor);
    if (!output.ok()) {
        result.error = output.error;
        return result;
    }
    result.output_tensor_elements = output.tensor_elements;
    return result;
}

ContractValidation validate_model_load(const ModelLoadReceipt& receipt) noexcept {
    ContractValidation result{};
    if (!receipt.loaded) {
        result.error = RuntimeError::ModelNotLoaded;
        return result;
    }
    if (!nonempty_token(receipt.model.model_id) ||
        !nonempty_token(receipt.model.model_version) ||
        !valid_sha256(receipt.model.artifact_sha256) ||
        !valid_sha256(receipt.loaded_artifact_sha256)) {
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
    const InferenceRequest& request,
    RuntimeLimits limits) noexcept {
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

}  // namespace vektoryum::ml
