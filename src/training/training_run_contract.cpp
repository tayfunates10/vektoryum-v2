#include "vektoryum/training/training_run_contract.hpp"

#include <cmath>
#include <string>
#include <unordered_set>

namespace vektoryum::training {

namespace {

[[nodiscard]] TrainingRunContractResult fail(TrainingRunContractError error,
                                             std::size_t index) noexcept {
    return TrainingRunContractResult{error, index};
}

[[nodiscard]] bool is_lower_hex_sha256(const std::string& value) noexcept {
    if (value.size() != 64U) {
        return false;
    }
    for (const char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

}  // namespace

TrainingRunContractResult validate_training_run_manifest(const TrainingRunManifest& manifest,
                                                         const TrainingRunLimits& limits) {
    if (manifest.schema_version.empty()) {
        return fail(TrainingRunContractError::MissingSchemaVersion, 0U);
    }
    if (manifest.run_id.empty()) {
        return fail(TrainingRunContractError::MissingRunIdentity, 0U);
    }
    if (manifest.dataset_id.empty() || manifest.dataset_version.empty()) {
        return fail(TrainingRunContractError::MissingDatasetIdentity, 0U);
    }
    if (manifest.model_id.empty() || manifest.model_version.empty()) {
        return fail(TrainingRunContractError::MissingModelIdentity, 0U);
    }
    if (manifest.architecture_revision.empty()) {
        return fail(TrainingRunContractError::MissingArchitectureRevision, 0U);
    }
    if (manifest.training_code_revision.empty()) {
        return fail(TrainingRunContractError::MissingTrainingCodeRevision, 0U);
    }
    if (manifest.degradation_pipeline_revision.empty()) {
        return fail(TrainingRunContractError::MissingDegradationPipelineRevision, 0U);
    }
    if (manifest.runtime_id.empty() || manifest.runtime_version.empty()) {
        return fail(TrainingRunContractError::MissingRuntimeIdentity, 0U);
    }
    if (manifest.artifact_sha256.empty()) {
        return fail(TrainingRunContractError::MissingArtifactChecksum, 0U);
    }
    if (!is_lower_hex_sha256(manifest.artifact_sha256)) {
        return fail(TrainingRunContractError::InvalidArtifactChecksum, 0U);
    }
    if (manifest.max_steps == 0U) {
        return fail(TrainingRunContractError::ZeroMaxSteps, 0U);
    }
    if (manifest.max_steps > limits.max_steps) {
        return fail(TrainingRunContractError::StepBudgetExceeded, 0U);
    }
    if (manifest.batch_size == 0U) {
        return fail(TrainingRunContractError::ZeroBatchSize, 0U);
    }
    if (manifest.batch_size > limits.max_batch_size) {
        return fail(TrainingRunContractError::BatchBudgetExceeded, 0U);
    }
    if (manifest.hyperparameters.size() > limits.max_hyperparameters) {
        return fail(TrainingRunContractError::TooManyHyperparameters, 0U);
    }

    std::unordered_set<std::string> names;
    names.reserve(manifest.hyperparameters.size());
    std::string previous_name;
    for (std::size_t index = 0U; index < manifest.hyperparameters.size(); ++index) {
        const TrainingHyperparameter& parameter = manifest.hyperparameters[index];
        if (parameter.name.empty()) {
            return fail(TrainingRunContractError::MissingHyperparameterName, index);
        }
        if (!names.insert(parameter.name).second) {
            return fail(TrainingRunContractError::DuplicateHyperparameterName, index);
        }
        if (!std::isfinite(parameter.value)) {
            return fail(TrainingRunContractError::NonFiniteHyperparameterValue, index);
        }
        if (!previous_name.empty() && parameter.name <= previous_name) {
            return fail(TrainingRunContractError::NonDeterministicHyperparameterOrder, index);
        }
        previous_name = parameter.name;
    }

    return TrainingRunContractResult{TrainingRunContractError::None,
                                     manifest.hyperparameters.size()};
}

}  // namespace vektoryum::training
