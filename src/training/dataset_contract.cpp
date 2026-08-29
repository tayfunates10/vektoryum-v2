#include "vektoryum/training/dataset_contract.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::training {

namespace {

[[nodiscard]] int hex_value(char ch) noexcept {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

[[nodiscard]] DatasetContractResult fail(DatasetContractError error, std::size_t index,
                                         std::uint64_t total_bytes) noexcept {
    return DatasetContractResult{error, index, total_bytes};
}

void append_binding_field(std::vector<std::uint8_t>& bytes, std::string_view value) {
    bytes.insert(bytes.end(), value.begin(), value.end());
    bytes.push_back(0U);
}

[[nodiscard]] bool license_allowed(std::string_view license_id,
                                   const DatasetLimits& limits) noexcept {
    return std::find(limits.allowed_license_ids.begin(), limits.allowed_license_ids.end(),
                     license_id) != limits.allowed_license_ids.end();
}

}  // namespace

bool is_sha256_hex(std::string_view value) noexcept {
    if (value.size() != 64U) {
        return false;
    }
    for (const char ch : value) {
        if (hex_value(ch) < 0) {
            return false;
        }
    }
    return true;
}

DatasetSplit deterministic_split(std::string_view content_sha256,
                                 std::uint64_t split_seed) noexcept {
    if (!is_sha256_hex(content_sha256)) {
        return DatasetSplit::Train;
    }

    std::uint64_t prefix = 0U;
    for (std::size_t i = 0U; i < 16U; ++i) {
        prefix = (prefix << 4U) | static_cast<std::uint64_t>(hex_value(content_sha256[i]));
    }
    const std::uint64_t bucket = (prefix ^ split_seed) % 100U;
    if (bucket < 80U) {
        return DatasetSplit::Train;
    }
    if (bucket < 90U) {
        return DatasetSplit::Validation;
    }
    return DatasetSplit::Test;
}

std::string dataset_rights_binding_sha256(const DatasetSample& sample) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(sample.content_sha256.size() + sample.license_id.size() +
                  sample.rights_statement.size() + sample.rights_grant_source.size() + 5U);
    append_binding_field(bytes, sample.content_sha256);
    append_binding_field(bytes, sample.license_id);
    append_binding_field(bytes, sample.rights_statement);
    append_binding_field(bytes, sample.rights_grant_source);
    bytes.push_back(sample.production_training_authorized ? 1U : 0U);
    return ml::sha256_hex(bytes);
}

DatasetContractResult validate_dataset_manifest(const DatasetManifest& manifest,
                                                const DatasetLimits& limits) {
    if (manifest.dataset_id.empty() || manifest.version.empty()) {
        return fail(DatasetContractError::MissingDatasetIdentity, 0U, 0U);
    }
    if (manifest.samples.empty()) {
        return fail(DatasetContractError::EmptyDataset, 0U, 0U);
    }
    if (manifest.samples.size() > limits.max_samples) {
        return fail(DatasetContractError::TooManySamples, 0U, 0U);
    }

    std::unordered_set<std::string> sample_ids;
    std::unordered_set<std::string> content_digests;
    sample_ids.reserve(manifest.samples.size());
    content_digests.reserve(manifest.samples.size());

    std::uint64_t total_bytes = 0U;
    for (std::size_t index = 0U; index < manifest.samples.size(); ++index) {
        const DatasetSample& sample = manifest.samples[index];
        if (sample.sample_id.empty() || sample.source_id.empty()) {
            return fail(DatasetContractError::MissingSampleIdentity, index, total_bytes);
        }
        if (!is_sha256_hex(sample.content_sha256)) {
            return fail(DatasetContractError::InvalidContentDigest, index, total_bytes);
        }
        if (sample.license_id.empty() || sample.rights_statement.empty() ||
            sample.rights_grant_source.empty()) {
            return fail(DatasetContractError::MissingRightsProvenance, index, total_bytes);
        }
        if (!license_allowed(sample.license_id, limits)) {
            return fail(DatasetContractError::LicenseNotAllowed, index, total_bytes);
        }
        if (!sample.production_training_authorized) {
            return fail(DatasetContractError::ProductionTrainingUnauthorized, index, total_bytes);
        }
        if (!is_sha256_hex(sample.rights_binding_sha256)) {
            return fail(DatasetContractError::InvalidRightsBinding, index, total_bytes);
        }
        if (dataset_rights_binding_sha256(sample) != sample.rights_binding_sha256) {
            return fail(DatasetContractError::RightsBindingMismatch, index, total_bytes);
        }
        if (sample.byte_size == 0U) {
            return fail(DatasetContractError::ZeroSampleBytes, index, total_bytes);
        }
        if (sample.byte_size > limits.max_sample_bytes) {
            return fail(DatasetContractError::SampleByteBudgetExceeded, index, total_bytes);
        }
        if (total_bytes > limits.max_total_bytes || sample.byte_size > limits.max_total_bytes - total_bytes) {
            return fail(DatasetContractError::TotalByteBudgetExceeded, index, total_bytes);
        }
        total_bytes += sample.byte_size;

        if (!sample_ids.insert(sample.sample_id).second) {
            return fail(DatasetContractError::DuplicateSampleId, index, total_bytes);
        }
        if (!content_digests.insert(sample.content_sha256).second) {
            return fail(DatasetContractError::DuplicateContentDigest, index, total_bytes);
        }
        if (sample.split != deterministic_split(sample.content_sha256, manifest.split_seed)) {
            return fail(DatasetContractError::SplitAssignmentMismatch, index, total_bytes);
        }
    }

    return DatasetContractResult{DatasetContractError::None, manifest.samples.size(), total_bytes};
}

}  // namespace vektoryum::training
