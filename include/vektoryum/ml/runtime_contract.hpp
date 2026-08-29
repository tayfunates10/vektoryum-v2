#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::ml {

enum class RuntimeError : std::uint8_t {
    None,
    InvalidModelId,
    InvalidModelVersion,
    InvalidTensorRank,
    ZeroTensorExtent,
    TensorElementOverflow,
    TensorBudgetExceeded,
};

struct TensorShape {
    std::vector<std::uint32_t> dimensions{};
};

struct ModelDescriptor {
    std::string model_id{};
    std::string model_version{};
    std::string artifact_sha256{};
};

struct RuntimeLimits {
    std::uint64_t max_tensor_elements{64ULL * 1024ULL * 1024ULL};
    std::uint8_t max_tensor_rank{8U};
};

struct ContractValidation {
    RuntimeError error{RuntimeError::None};
    std::uint64_t tensor_elements{0U};

    [[nodiscard]] bool ok() const noexcept {
        return error == RuntimeError::None;
    }
};

// Fail-closed validation for model identity/provenance and bounded tensor geometry.
// Model artifacts must carry a lowercase 64-character SHA-256 hex digest.
[[nodiscard]] ContractValidation validate_runtime_contract(
    const ModelDescriptor& model,
    const TensorShape& input,
    RuntimeLimits limits = {}) noexcept;

}  // namespace vektoryum::ml
