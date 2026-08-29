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
    ArtifactDigestMismatch,
    ModelNotLoaded,
    TensorElementCountMismatch,
    InvalidTensorLayout,
    InvalidChannelContract,
    InvalidNormalization,
    InvalidPostprocessRange,
    NonFiniteTensorData,
    UnsupportedExecution,
};

enum class ExecutionProvider : std::uint8_t {
    Unknown,
    Cpu,
    Cuda,
    DirectML,
    CoreML,
};

enum class TensorLayout : std::uint8_t {
    Unknown,
    Nchw,
    Nhwc,
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

struct PreprocessContract {
    TensorLayout layout{TensorLayout::Unknown};
    std::uint32_t channels{0U};
    std::vector<float> mean{};
    std::vector<float> scale{};
};

struct PostprocessContract {
    TensorLayout layout{TensorLayout::Unknown};
    std::uint32_t channels{0U};
    float output_min{0.0F};
    float output_max{1.0F};
};

struct ExecutionContract {
    ModelDescriptor model{};
    TensorShape input{};
    TensorShape output{};
    RuntimeBinding binding{};
    PreprocessContract preprocess{};
    PostprocessContract postprocess{};
};

struct ModelLoadReceipt {
    ModelDescriptor model{};
    RuntimeBinding binding{};
    std::string loaded_artifact_sha256{};
    bool loaded{false};
};

struct InferenceRequest {
    ExecutionContract contract{};
    ModelLoadReceipt loaded_model{};
    std::uint64_t input_element_count{0U};
    std::uint64_t output_element_capacity{0U};
};

struct ContractValidation {
    RuntimeError error{RuntimeError::None};
    std::uint64_t tensor_elements{0U};
    std::uint64_t output_tensor_elements{0U};

    [[nodiscard]] bool ok() const noexcept {
        return error == RuntimeError::None;
    }
};

struct InferenceResult {
    RuntimeError error{RuntimeError::None};
    std::vector<float> output{};

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

// Validates preprocessing layout/channel semantics and finite per-channel
// normalization vectors against the declared input tensor shape.
[[nodiscard]] ContractValidation validate_preprocess_contract(
    const PreprocessContract& preprocess,
    const TensorShape& input) noexcept;

// Validates postprocessing layout/channel semantics and a finite, ordered output
// range against the declared output tensor shape.
[[nodiscard]] ContractValidation validate_postprocess_contract(
    const PostprocessContract& postprocess,
    const TensorShape& output) noexcept;

// Validates the execution boundary without selecting or substituting providers.
// A provider mismatch or any reported fallback is rejected: callers must make
// backend/provider changes explicit instead of silently degrading execution.
[[nodiscard]] ContractValidation validate_execution_contract(
    const ExecutionContract& contract,
    RuntimeLimits limits = {}) noexcept;

// Validates a concrete model-load receipt against declared provenance and the
// exact backend/provider binding used to load it. The runtime never treats an
// unverified or fallback-loaded artifact as production-ready.
[[nodiscard]] ContractValidation validate_model_load(
    const ModelLoadReceipt& receipt) noexcept;

// Validates the boundary immediately before inference: the loaded model must
// exactly match the execution contract, and caller-provided tensor storage must
// match the certified input/output element counts. No allocation or provider
// substitution is performed here.
[[nodiscard]] ContractValidation validate_inference_request(
    const InferenceRequest& request,
    RuntimeLimits limits = {}) noexcept;

// Small project-owned deterministic CPU execution boundary used to prove that
// Stage 7 owns model state and inference orchestration rather than only
// validating caller-created receipts. It deliberately supports only the
// explicit "vektoryum-reference" CPU backend and never falls back.
class DeterministicReferenceRuntime {
public:
    [[nodiscard]] RuntimeError load(const ModelLoadReceipt& receipt) noexcept;
    void unload() noexcept;
    [[nodiscard]] bool loaded() const noexcept;

    // Executes a deterministic nearest-index reference transform after applying
    // the declared per-channel normalization, then clamps to the declared
    // postprocess range. This is a runtime conformance primitive, not an ML
    // quality model. Unsupported bindings fail closed.
    [[nodiscard]] InferenceResult execute(
        const ExecutionContract& contract,
        const std::vector<float>& input,
        RuntimeLimits limits = {}) const noexcept;

private:
    ModelLoadReceipt loaded_model_{};
};

}  // namespace vektoryum::ml
