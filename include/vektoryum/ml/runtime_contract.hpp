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

[[nodiscard]] ContractValidation validate_runtime_contract(
    const ModelDescriptor& model,
    const TensorShape& input,
    RuntimeLimits limits = {}) noexcept;

[[nodiscard]] ContractValidation validate_preprocess_contract(
    const PreprocessContract& preprocess,
    const TensorShape& input) noexcept;

[[nodiscard]] ContractValidation validate_postprocess_contract(
    const PostprocessContract& postprocess,
    const TensorShape& output) noexcept;

[[nodiscard]] ContractValidation validate_execution_contract(
    const ExecutionContract& contract,
    RuntimeLimits limits = {}) noexcept;

[[nodiscard]] ContractValidation validate_model_load(
    const ModelLoadReceipt& receipt) noexcept;

[[nodiscard]] ContractValidation validate_inference_request(
    const InferenceRequest& request,
    RuntimeLimits limits = {}) noexcept;

class DeterministicReferenceRuntime {
public:
    [[nodiscard]] RuntimeError load(const ModelLoadReceipt& receipt) noexcept;
    void unload() noexcept;
    [[nodiscard]] bool loaded() const noexcept;

    [[nodiscard]] InferenceResult execute(
        const ExecutionContract& contract,
        const std::vector<float>& input,
        RuntimeLimits limits = {}) const;

private:
    ModelLoadReceipt loaded_model_{};
};

}  // namespace vektoryum::ml
