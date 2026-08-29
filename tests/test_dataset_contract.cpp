#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include "vektoryum/training/dataset_contract.hpp"

namespace {

int failures = 0;

void expect_true(bool condition, std::string_view name) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
        return;
    }
    std::cout << "[PASS] " << name << '\n';
}

[[nodiscard]] vektoryum::training::DatasetSample sample(std::string id, std::string digest,
                                                        std::uint64_t seed) {
    using namespace vektoryum::training;
    DatasetSample value;
    value.sample_id = std::move(id);
    value.source_id = "fixture-source";
    value.content_sha256 = std::move(digest);
    value.license_id = "CC0-1.0";
    value.rights_statement = "production training permitted";
    value.production_training_authorized = true;
    value.byte_size = 1024U;
    value.split = deterministic_split(value.content_sha256, seed);
    return value;
}

[[nodiscard]] vektoryum::training::DatasetManifest valid_manifest() {
    using namespace vektoryum::training;
    constexpr std::uint64_t seed = 0x5A17U;
    DatasetManifest manifest;
    manifest.dataset_id = "stage8-regression";
    manifest.version = "1";
    manifest.split_seed = seed;
    manifest.samples.push_back(sample(
        "sample-a", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", seed));
    manifest.samples.push_back(sample(
        "sample-b", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", seed));
    return manifest;
}

}  // namespace

int run_dataset_contract_tests() {
    using namespace vektoryum::training;

    const DatasetManifest baseline = valid_manifest();
    const DatasetContractResult accepted = validate_dataset_manifest(baseline);
    expect_true(accepted.ok(), "dataset manifest with exact provenance is accepted");
    expect_true(accepted.total_bytes == 2048U, "dataset byte accounting is exact");

    const DatasetSplit split_a = deterministic_split(baseline.samples[0].content_sha256,
                                                     baseline.split_seed);
    const DatasetSplit split_b = deterministic_split(baseline.samples[0].content_sha256,
                                                     baseline.split_seed);
    expect_true(split_a == split_b, "split assignment is deterministic for digest and seed");

    DatasetManifest invalid = baseline;
    invalid.samples[0].license_id.clear();
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::MissingRightsProvenance,
                "missing license provenance is rejected");

    invalid = baseline;
    invalid.samples[0].rights_statement.clear();
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::MissingRightsProvenance,
                "missing rights statement is rejected");

    invalid = baseline;
    invalid.samples[0].production_training_authorized = false;
    expect_true(validate_dataset_manifest(invalid).error ==
                    DatasetContractError::ProductionTrainingUnauthorized,
                "unauthorized production training data is rejected");

    invalid = baseline;
    invalid.samples[1].content_sha256 = invalid.samples[0].content_sha256;
    invalid.samples[1].split = deterministic_split(invalid.samples[1].content_sha256,
                                                   invalid.split_seed);
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::DuplicateContentDigest,
                "duplicate content cannot leak across dataset splits");

    invalid = baseline;
    invalid.samples[0].split = invalid.samples[0].split == DatasetSplit::Train
                                   ? DatasetSplit::Test
                                   : DatasetSplit::Train;
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::SplitAssignmentMismatch,
                "manifest cannot override deterministic split assignment");

    invalid = baseline;
    invalid.samples[0].content_sha256[0] = 'z';
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::InvalidContentDigest,
                "malformed content digest is rejected");

    DatasetLimits limits;
    limits.max_sample_bytes = 512U;
    expect_true(validate_dataset_manifest(baseline, limits).error ==
                    DatasetContractError::SampleByteBudgetExceeded,
                "per-sample resource budget is fail-closed");

    limits = DatasetLimits{};
    limits.max_total_bytes = 1500U;
    expect_true(validate_dataset_manifest(baseline, limits).error ==
                    DatasetContractError::TotalByteBudgetExceeded,
                "aggregate dataset resource budget is fail-closed");

    invalid = baseline;
    invalid.samples[1].sample_id = invalid.samples[0].sample_id;
    expect_true(validate_dataset_manifest(invalid).error == DatasetContractError::DuplicateSampleId,
                "duplicate sample identity is rejected");

    return failures;
}
