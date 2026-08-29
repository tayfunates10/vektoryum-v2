#include <iostream>
#include <limits>
#include <string_view>

#include "vektoryum/ml/runtime_contract.hpp"

namespace {
int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    } else {
        std::cout << "[PASS] " << name << '\n';
    }
}
}  // namespace

int run_ml_runtime_tests() {
    using namespace vektoryum::ml;

    const ModelDescriptor model{
        "vektoryum-sr",
        "1.0.0",
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
    const TensorShape input{{1U, 3U, 256U, 256U}};

    const auto valid = validate_runtime_contract(model, input);
    expect_true(valid.ok(), "valid ML runtime contract accepted");
    expect_true(valid.tensor_elements == 196608U, "tensor element count is exact");

    const auto repeat = validate_runtime_contract(model, input);
    expect_true(repeat.error == valid.error && repeat.tensor_elements == valid.tensor_elements,
                "runtime contract validation is deterministic");

    auto bad_model = model;
    bad_model.model_id.clear();
    expect_true(validate_runtime_contract(bad_model, input).error == RuntimeError::InvalidModelId,
                "empty model id rejected");

    bad_model = model;
    bad_model.artifact_sha256[0] = 'A';
    expect_true(validate_runtime_contract(bad_model, input).error == RuntimeError::InvalidModelId,
                "non-canonical model digest rejected");

    bad_model = model;
    bad_model.model_version = "1.0 beta";
    expect_true(validate_runtime_contract(bad_model, input).error == RuntimeError::InvalidModelVersion,
                "whitespace-bearing model version rejected");

    expect_true(validate_runtime_contract(model, TensorShape{}).error == RuntimeError::InvalidTensorRank,
                "empty tensor rank rejected");
    expect_true(validate_runtime_contract(model, TensorShape{{1U, 0U, 4U}}).error ==
                    RuntimeError::ZeroTensorExtent,
                "zero tensor extent rejected");
    expect_true(validate_runtime_contract(model, TensorShape{{2U, 3U, 4U}}, {16U, 8U}).error ==
                    RuntimeError::TensorBudgetExceeded,
                "tensor budget fails closed");
    expect_true(validate_runtime_contract(model, TensorShape{{1U, 1U, 1U}}, {1024U, 0U}).error ==
                    RuntimeError::InvalidTensorRank,
                "zero rank limit rejected");

    const RuntimeBinding binding{
        "vektoryum-reference", ExecutionProvider::Cpu, ExecutionProvider::Cpu, true, false};
    const PreprocessContract preprocess{
        TensorLayout::Nchw,
        3U,
        {0.5F, 0.5F, 0.5F},
        {0.5F, 0.5F, 0.5F}};
    const PostprocessContract postprocess{TensorLayout::Nchw, 3U, 0.0F, 1.0F};
    const ExecutionContract execution{
        model,
        input,
        TensorShape{{1U, 3U, 512U, 512U}},
        binding,
        preprocess,
        postprocess};
    const auto execution_valid = validate_execution_contract(execution);
    expect_true(execution_valid.ok(), "explicit deterministic provider binding accepted");
    expect_true(execution_valid.tensor_elements == 196608U,
                "execution contract preserves input element accounting");
    expect_true(execution_valid.output_tensor_elements == 786432U,
                "execution contract accounts output tensor exactly");

    const auto preprocess_valid = validate_preprocess_contract(preprocess, input);
    expect_true(preprocess_valid.ok(), "finite NCHW preprocessing contract accepted");
    expect_true(validate_postprocess_contract(postprocess, execution.output).ok(),
                "bounded NCHW postprocessing contract accepted");

    auto bad_preprocess = preprocess;
    bad_preprocess.layout = TensorLayout::Nhwc;
    expect_true(validate_preprocess_contract(bad_preprocess, input).error ==
                    RuntimeError::InvalidChannelContract,
                "layout and tensor channel mismatch rejected");

    bad_preprocess = preprocess;
    bad_preprocess.mean.pop_back();
    expect_true(validate_preprocess_contract(bad_preprocess, input).error ==
                    RuntimeError::InvalidNormalization,
                "incomplete normalization vector rejected");

    bad_preprocess = preprocess;
    bad_preprocess.scale[1U] = 0.0F;
    expect_true(validate_preprocess_contract(bad_preprocess, input).error ==
                    RuntimeError::InvalidNormalization,
                "zero normalization scale rejected");

    bad_preprocess = preprocess;
    bad_preprocess.mean[0U] = std::numeric_limits<float>::quiet_NaN();
    expect_true(validate_preprocess_contract(bad_preprocess, input).error ==
                    RuntimeError::InvalidNormalization,
                "non-finite normalization rejected");

    auto bad_postprocess = postprocess;
    bad_postprocess.output_min = 2.0F;
    bad_postprocess.output_max = 1.0F;
    expect_true(validate_postprocess_contract(bad_postprocess, execution.output).error ==
                    RuntimeError::InvalidPostprocessRange,
                "reversed postprocess range rejected");

    bad_postprocess = postprocess;
    bad_postprocess.output_max = std::numeric_limits<float>::infinity();
    expect_true(validate_postprocess_contract(bad_postprocess, execution.output).error ==
                    RuntimeError::InvalidPostprocessRange,
                "non-finite postprocess range rejected");

    auto bad_execution = execution;
    bad_execution.binding.backend_id.clear();
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::InvalidBackend,
                "empty backend identity rejected");

    bad_execution = execution;
    bad_execution.binding.active_provider = ExecutionProvider::Cuda;
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::SilentFallback,
                "provider substitution rejected");

    bad_execution = execution;
    bad_execution.binding.fallback_used = true;
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::SilentFallback,
                "reported fallback rejected");

    bad_execution = execution;
    bad_execution.binding.deterministic = false;
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::NonDeterministicProvider,
                "non-deterministic provider rejected");

    bad_execution = execution;
    bad_execution.output = TensorShape{};
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::InvalidOutputTensor,
                "empty output tensor contract rejected");

    bad_execution = execution;
    bad_execution.preprocess.channels = 4U;
    expect_true(validate_execution_contract(bad_execution).error == RuntimeError::InvalidChannelContract,
                "execution rejects mismatched preprocessing channels");

    bad_execution = execution;
    bad_execution.postprocess.output_min = 3.0F;
    bad_execution.postprocess.output_max = 2.0F;
    expect_true(validate_execution_contract(bad_execution).error ==
                    RuntimeError::InvalidPostprocessRange,
                "execution rejects invalid postprocess range");

    const auto execution_repeat = validate_execution_contract(execution);
    expect_true(execution_repeat.error == execution_valid.error &&
                    execution_repeat.tensor_elements == execution_valid.tensor_elements &&
                    execution_repeat.output_tensor_elements == execution_valid.output_tensor_elements,
                "execution contract validation is deterministic");

    const ModelLoadReceipt load{model, binding, model.artifact_sha256, true};
    expect_true(validate_model_load(load).ok(), "matching model load receipt accepted");

    auto bad_load = load;
    bad_load.loaded = false;
    expect_true(validate_model_load(bad_load).error == RuntimeError::ModelNotLoaded,
                "unloaded model rejected");

    bad_load = load;
    bad_load.loaded_artifact_sha256[0] = 'f';
    expect_true(validate_model_load(bad_load).error == RuntimeError::ArtifactDigestMismatch,
                "artifact digest mismatch rejected");

    bad_load = load;
    bad_load.binding.active_provider = ExecutionProvider::Cuda;
    expect_true(validate_model_load(bad_load).error == RuntimeError::SilentFallback,
                "model load provider substitution rejected");

    const InferenceRequest inference{execution, load, 196608U, 786432U};
    const auto inference_valid = validate_inference_request(inference);
    expect_true(inference_valid.ok(), "matching loaded model and tensor storage accepted");

    auto bad_inference = inference;
    bad_inference.input_element_count = 196607U;
    expect_true(validate_inference_request(bad_inference).error ==
                    RuntimeError::TensorElementCountMismatch,
                "undersized inference input storage rejected");

    bad_inference = inference;
    bad_inference.output_element_capacity = 786431U;
    expect_true(validate_inference_request(bad_inference).error ==
                    RuntimeError::TensorElementCountMismatch,
                "undersized inference output storage rejected");

    bad_inference = inference;
    bad_inference.loaded_model.model.model_version = "2.0.0";
    expect_true(validate_inference_request(bad_inference).error ==
                    RuntimeError::ArtifactDigestMismatch,
                "loaded model identity mismatch rejected");

    bad_inference = inference;
    bad_inference.loaded_model.binding.backend_id = "other-backend";
    expect_true(validate_inference_request(bad_inference).error ==
                    RuntimeError::ArtifactDigestMismatch,
                "loaded backend mismatch rejected");

    const auto inference_repeat = validate_inference_request(inference);
    expect_true(inference_repeat.error == inference_valid.error &&
                    inference_repeat.tensor_elements == inference_valid.tensor_elements &&
                    inference_repeat.output_tensor_elements == inference_valid.output_tensor_elements,
                "inference boundary validation is deterministic");

    return failures;
}
