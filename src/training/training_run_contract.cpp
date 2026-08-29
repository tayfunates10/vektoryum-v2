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
    if (manifest.runtime_id.empty() || manifest.runtime_version.empty()) {
        return fail(TrainingRunContractError::MissingRuntimeIdentity, 0U);
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
