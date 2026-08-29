#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vektoryum::training {

enum class DatasetSplit : std::uint8_t {
    Train,
    Validation,
    Test,
};

struct DatasetSample {
    std::string sample_id;
    std::string source_id;
    std::string content_sha256;
    std::string license_id;
    std::string rights_statement;
    std::string rights_grant_source;
    std::string rights_binding_sha256;
    bool production_training_authorized{false};
    std::uint64_t byte_size{0U};
    DatasetSplit split{DatasetSplit::Train};
};

struct DatasetSampleArtifact {
    std::string sample_id;
    std::vector<std::uint8_t> bytes;
};

struct DatasetManifest {
    std::string schema_version;
    std::string dataset_id;
    std::string version;
    std::uint64_t split_seed{0U};
    std::vector<DatasetSample> samples;
};

struct DatasetLimits {
    std::size_t max_samples{100'000U};
    std::uint64_t max_total_bytes{64ULL * 1024ULL * 1024ULL * 1024ULL};
    std::uint64_t max_sample_bytes{512ULL * 1024ULL * 1024ULL};
    std::vector<std::string> allowed_license_ids{"CC0-1.0"};
};

enum class DatasetContractError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingDatasetIdentity,
    EmptyDataset,
    TooManySamples,
    MissingSampleIdentity,
    NonDeterministicSampleOrder,
    InvalidContentDigest,
    MissingRightsProvenance,
    LicenseNotAllowed,
    InvalidRightsBinding,
    RightsBindingMismatch,
    ProductionTrainingUnauthorized,
    ZeroSampleBytes,
    SampleByteBudgetExceeded,
    TotalByteBudgetExceeded,
    DuplicateSampleId,
    DuplicateContentDigest,
    SplitAssignmentMismatch,
    ArtifactCountMismatch,
    ArtifactIdentityMismatch,
    ArtifactByteSizeMismatch,
    ContentDigestMismatch,
};

struct DatasetContractResult {
    DatasetContractError error{DatasetContractError::None};
    std::size_t sample_index{0U};
    std::uint64_t total_bytes{0U};

    [[nodiscard]] bool ok() const noexcept { return error == DatasetContractError::None; }
};

[[nodiscard]] bool is_sha256_hex(std::string_view value) noexcept;
[[nodiscard]] DatasetSplit deterministic_split(std::string_view content_sha256,
                                               std::uint64_t split_seed) noexcept;
[[nodiscard]] std::string dataset_rights_binding_sha256(const DatasetSample& sample);
[[nodiscard]] DatasetContractResult validate_dataset_manifest(
    const DatasetManifest& manifest,
    const std::vector<DatasetSampleArtifact>& artifacts,
    const DatasetLimits& limits = {});

}  // namespace vektoryum::training
