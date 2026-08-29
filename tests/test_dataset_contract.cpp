#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    value.rights_grant_source = "fixture-rights-owner";
    value.production_training_authorized = true;
    value.byte_size = 14U;
    value.split = deterministic_split(value.content_sha256, seed);
    value.rights_binding_sha256 = dataset_rights_binding_sha256(value);
    return value;
}

[[nodiscard]] vektoryum::training::DatasetManifest valid_manifest() {
    using namespace vektoryum::training;
    constexpr std::uint64_t seed = 0x5A17U;
    DatasetManifest manifest;
    manifest.schema_version = "vektoryum-dataset-v1";
    manifest.dataset_id = "stage8-regression";
    manifest.version = "1";
    manifest.split_seed = seed;
    manifest.samples.push_back(sample(
        "sample-a", "18a35ef685a8d84a3fe4a3858c9f35ce5c491ac4ef26908777f9abb3d5f74c49", seed));
    manifest.samples.push_back(sample(
        "sample-b", "e71aa7af0b025dc77b595436cf6853bfd700517673c0317da3f4ab447fdae82c", seed));
    return manifest;
}

[[nodiscard]] std::vector<vektoryum::training::DatasetSampleArtifact> valid_artifacts() {
    using namespace vektoryum::training;
    return {
        DatasetSampleArtifact{"sample-a", {'s','a','m','p','l','e','-','a','-','b','y','t','e','s'}},
        DatasetSampleArtifact{"sample-b", {'s','a','m','p','l','e','-','b','-','b','y','t','e','s'}},
    };
}

}  // namespace

int run_dataset_contract_tests() {
    using namespace vektoryum::training;

    const DatasetManifest baseline = valid_manifest();
    const auto artifacts = valid_artifacts();
    const DatasetContractResult accepted = validate_dataset_manifest(baseline, artifacts);
    expect_true(accepted.ok(), "dataset manifest with exact provenance and bytes is accepted");
    expect_true(accepted.total_bytes == 28U, "dataset byte accounting is exact");

    const DatasetSplit split_a = deterministic_split(baseline.samples[0].content_sha256,
                                                     baseline.split_seed);
    const DatasetSplit split_b = deterministic_split(baseline.samples[0].content_sha256,
                                                     baseline.split_seed);
    expect_true(split_a == split_b, "split assignment is deterministic for digest and seed");

    DatasetManifest invalid = baseline;
    invalid.schema_version.clear();
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::MissingSchemaVersion,
                "missing dataset schema version is rejected");

    invalid = baseline;
    invalid.samples[0].license_id.clear();
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::MissingRightsProvenance,
                "missing license provenance is rejected");

    invalid = baseline;
    invalid.samples[0].rights_statement.clear();
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::MissingRightsProvenance,
                "missing rights statement is rejected");

    invalid = baseline;
    invalid.samples[0].rights_grant_source.clear();
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::MissingRightsProvenance,
                "missing rights grant source is rejected");

    invalid = baseline;
    invalid.samples[0].license_id = "UNKNOWN-LICENSE";
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::LicenseNotAllowed,
                "unknown or incompatible license is rejected fail-closed");

    invalid = baseline;
    invalid.samples[0].production_training_authorized = false;
    expect_true(validate_dataset_manifest(invalid, artifacts).error ==
                    DatasetContractError::ProductionTrainingUnauthorized,
                "unauthorized production training data is rejected");

    invalid = baseline;
    invalid.samples[0].rights_binding_sha256 = "not-a-sha256";
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::InvalidRightsBinding,
                "malformed digest-to-rights binding is rejected");

    invalid = baseline;
    invalid.samples[0].rights_statement = "modified rights metadata";
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::RightsBindingMismatch,
                "changed rights metadata is rejected by digest binding");

    invalid = baseline;
    invalid.samples[0].rights_grant_source = "different-rights-owner";
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::RightsBindingMismatch,
                "changed rights grant source is rejected by digest binding");

    invalid = baseline;
    auto invalid_artifacts = artifacts;
    std::swap(invalid.samples[0], invalid.samples[1]);
    std::swap(invalid_artifacts[0], invalid_artifacts[1]);
    expect_true(validate_dataset_manifest(invalid, invalid_artifacts).error ==
                    DatasetContractError::NonDeterministicSampleOrder,
                "non-canonical sample ordering is rejected");

    invalid = baseline;
    invalid_artifacts = artifacts;
    invalid.samples[1].content_sha256 = invalid.samples[0].content_sha256;
    invalid.samples[1].split = deterministic_split(invalid.samples[1].content_sha256,
                                                   invalid.split_seed);
    invalid.samples[1].rights_binding_sha256 = dataset_rights_binding_sha256(invalid.samples[1]);
    invalid_artifacts[1].bytes = invalid_artifacts[0].bytes;
    expect_true(validate_dataset_manifest(invalid, invalid_artifacts).error == DatasetContractError::DuplicateContentDigest,
                "duplicate content cannot leak across dataset splits");

    invalid = baseline;
    invalid.samples[0].split = invalid.samples[0].split == DatasetSplit::Train
                                   ? DatasetSplit::Test
                                   : DatasetSplit::Train;
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::SplitAssignmentMismatch,
                "manifest cannot override deterministic split assignment");

    invalid = baseline;
    invalid.samples[0].content_sha256[0] = 'z';
    expect_true(validate_dataset_manifest(invalid, artifacts).error == DatasetContractError::InvalidContentDigest,
                "malformed content digest is rejected");

    DatasetLimits limits;
    limits.max_sample_bytes = 13U;
    expect_true(validate_dataset_manifest(baseline, artifacts, limits).error ==
                    DatasetContractError::SampleByteBudgetExceeded,
                "per-sample resource budget is fail-closed");

    limits = DatasetLimits{};
    limits.max_total_bytes = 20U;
    expect_true(validate_dataset_manifest(baseline, artifacts, limits).error ==
                    DatasetContractError::TotalByteBudgetExceeded,
                "aggregate dataset resource budget is fail-closed");

    invalid = baseline;
    invalid.samples[1].sample_id = invalid.samples[0].sample_id;
    invalid_artifacts = artifacts;
    invalid_artifacts[1].sample_id = invalid.samples[1].sample_id;
    expect_true(validate_dataset_manifest(invalid, invalid_artifacts).error == DatasetContractError::DuplicateSampleId,
                "duplicate sample identity is rejected");

    invalid_artifacts = artifacts;
    invalid_artifacts.pop_back();
    expect_true(validate_dataset_manifest(baseline, invalid_artifacts).error == DatasetContractError::ArtifactCountMismatch,
                "missing sample artifact is rejected");

    invalid_artifacts = artifacts;
    invalid_artifacts[0].sample_id = "wrong-sample";
    expect_true(validate_dataset_manifest(baseline, invalid_artifacts).error == DatasetContractError::ArtifactIdentityMismatch,
                "artifact identity mismatch is rejected");

    invalid_artifacts = artifacts;
    invalid_artifacts[0].bytes.pop_back();
    expect_true(validate_dataset_manifest(baseline, invalid_artifacts).error == DatasetContractError::ArtifactByteSizeMismatch,
                "artifact byte-size mismatch is rejected");

    invalid_artifacts = artifacts;
    invalid_artifacts[0].bytes[0] = 'S';
    expect_true(validate_dataset_manifest(baseline, invalid_artifacts).error == DatasetContractError::ContentDigestMismatch,
                "artifact byte content mismatch is rejected by SHA-256");

    return failures;
}
