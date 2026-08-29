#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::training {

struct TrainingHyperparameter {
    std::string name;
    double value{0.0};
};

struct TrainingRunManifest {
    std::string schema_version;
    std::string run_id;
    std::string dataset_id;
    std::string dataset_version;
    std::string model_id;
    std::string model_version;
    std::string runtime_id;
    std::string runtime_version;
    std::uint64_t seed{0U};
    std::uint64_t max_steps{0U};
    std::uint32_t batch_size{0U};
    std::vector<TrainingHyperparameter> hyperparameters;
};

struct TrainingRunLimits {
    std::uint64_t max_steps{10'000'000U};
    std::uint32_t max_batch_size{65'536U};
    std::size_t max_hyperparameters{256U};
};

enum class TrainingRunContractError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingRunIdentity,
    MissingDatasetIdentity,
    MissingModelIdentity,
    MissingRuntimeIdentity,
    ZeroMaxSteps,
    StepBudgetExceeded,
    ZeroBatchSize,
    BatchBudgetExceeded,
    TooManyHyperparameters,
    MissingHyperparameterName,
    DuplicateHyperparameterName,
    NonFiniteHyperparameterValue,
    NonDeterministicHyperparameterOrder,
};

struct TrainingRunContractResult {
    TrainingRunContractError error{TrainingRunContractError::None};
    std::size_t hyperparameter_index{0U};

    [[nodiscard]] bool ok() const noexcept { return error == TrainingRunContractError::None; }
};

[[nodiscard]] TrainingRunContractResult validate_training_run_manifest(
    const TrainingRunManifest& manifest, const TrainingRunLimits& limits = {});

}  // namespace vektoryum::training
