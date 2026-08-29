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
    InvalidBackend,
    InvalidProvider,
    SilentFallback,
    NonDeterministicProvider,
    InvalidOutputTensor,
};

enum class ExecutionProvider : std::uint8_t {
    Unknown,
    Cpu,
    Cuda,
    DirectML,
    CoreML,
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

struct RuntimeBinding {
    std::string backend_id{};
    ExecutionProvider requested_provider{ExecutionProvider::Unknown};
    ExecutionProvider active_provider{ExecutionProvider::Unknown};
    bool deterministic{false};
    bool fallback_used{false};
};

struct ExecutionContract {
    ModelDescriptor model{};
    TensorShape input{};
    TensorShape output{};
    RuntimeBinding binding{};
};

struct ContractValidation {
    RuntimeError error{RuntimeError::None};
    std::uint64_t tensor_elements{0U};
    std::uint64_t output_tensor_elements{0U};

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

// Validates the execution boundary without selecting or substituting providers.
// A provider mismatch or any reported fallback is rejected: callers must make
// backend/provider changes explicit instead of silently degrading execution.
[[nodiscard]] ContractValidation validate_execution_contract(
    const ExecutionContract& contract,
    RuntimeLimits limits = {}) noexcept;

}  // namespace vektoryum::ml
