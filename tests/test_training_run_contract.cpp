#include <limits>
#include <string_view>

#include "vektoryum/training/training_run_contract.hpp"

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
    }
}

[[nodiscard]] vektoryum::training::TrainingRunManifest valid_manifest() {
    using namespace vektoryum::training;
    TrainingRunManifest manifest;
    manifest.schema_version = "vektoryum-training-run-v1";
    manifest.run_id = "stage8-run-001";
    manifest.dataset_id = "stage8-regression";
    manifest.dataset_version = "1";
    manifest.model_id = "reference-model";
    manifest.model_version = "1";
    manifest.runtime_id = "vektoryum-reference";
    manifest.runtime_version = "1";
    manifest.seed = 0x5A17U;
    manifest.max_steps = 1'000U;
    manifest.batch_size = 16U;
    manifest.hyperparameters = {{"learning_rate", 0.001}, {"weight_decay", 0.0001}};
    return manifest;
}

}  // namespace

int run_training_run_contract_tests() {
    using namespace vektoryum::training;

    const TrainingRunManifest baseline = valid_manifest();
    expect_true(validate_training_run_manifest(baseline).ok(),
                "deterministic training manifest is accepted");

    TrainingRunManifest invalid = baseline;
    invalid.schema_version.clear();
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::MissingSchemaVersion,
                "missing schema version is rejected");

    invalid = baseline;
    invalid.dataset_version.clear();
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::MissingDatasetIdentity,
                "dataset identity must include version");

    invalid = baseline;
    invalid.runtime_id.clear();
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::MissingRuntimeIdentity,
                "runtime identity is mandatory");

    invalid = baseline;
    invalid.max_steps = 0U;
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::ZeroMaxSteps,
                "zero training step budget is rejected");

    TrainingRunLimits limits;
    limits.max_steps = 999U;
    expect_true(validate_training_run_manifest(baseline, limits).error ==
                    TrainingRunContractError::StepBudgetExceeded,
                "training step resource budget is fail-closed");

    limits = TrainingRunLimits{};
    limits.max_batch_size = 8U;
    expect_true(validate_training_run_manifest(baseline, limits).error ==
                    TrainingRunContractError::BatchBudgetExceeded,
                "batch resource budget is fail-closed");

    invalid = baseline;
    invalid.hyperparameters[0].value = std::numeric_limits<double>::infinity();
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::NonFiniteHyperparameterValue,
                "non-finite hyperparameters are rejected");

    invalid = baseline;
    invalid.hyperparameters = {{"weight_decay", 0.0001}, {"learning_rate", 0.001}};
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::NonDeterministicHyperparameterOrder,
                "hyperparameters require deterministic canonical ordering");

    invalid = baseline;
    invalid.hyperparameters = {{"learning_rate", 0.001}, {"learning_rate", 0.002}};
    expect_true(validate_training_run_manifest(invalid).error ==
                    TrainingRunContractError::DuplicateHyperparameterName,
                "duplicate hyperparameters are rejected");

    return failures;
}
