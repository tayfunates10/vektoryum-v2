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
    const auto max_rank = static_cast<std::size_t>(limits.max_tensor_rank);
    if (limits.max_tensor_rank == 0U || limits.max_tensor_elements == 0U ||
        input.dimensions.empty() || input.dimensions.size() > max_rank) {
        result.error = RuntimeError::InvalidTensorRank;
        return result;
    }

    std::uint64_t elements = 1U;
    for (const std::uint32_t extent : input.dimensions) {
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

}  // namespace vektoryum::ml
