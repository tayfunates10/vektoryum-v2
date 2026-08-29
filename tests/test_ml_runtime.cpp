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

    const ExecutionContract execution{
        model,
        input,
        TensorShape{{1U, 3U, 512U, 512U}},
        RuntimeBinding{"vektoryum-reference", ExecutionProvider::Cpu, ExecutionProvider::Cpu, true, false}};
    const auto execution_valid = validate_execution_contract(execution);
    expect_true(execution_valid.ok(), "explicit deterministic provider binding accepted");
    expect_true(execution_valid.tensor_elements == 196608U,
                "execution contract preserves input element accounting");
    expect_true(execution_valid.output_tensor_elements == 786432U,
                "execution contract accounts output tensor exactly");

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

    const auto execution_repeat = validate_execution_contract(execution);
    expect_true(execution_repeat.error == execution_valid.error &&
                    execution_repeat.tensor_elements == execution_valid.tensor_elements &&
                    execution_repeat.output_tensor_elements == execution_valid.output_tensor_elements,
                "execution contract validation is deterministic");

    return failures;
}
