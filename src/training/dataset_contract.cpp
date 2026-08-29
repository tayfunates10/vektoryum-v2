#include "vektoryum/training/dataset_contract.hpp"

#include <limits>
#include <unordered_set>

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
        if (sample.license_id.empty() || sample.rights_statement.empty()) {
            return fail(DatasetContractError::MissingRightsProvenance, index, total_bytes);
        }
        if (!sample.production_training_authorized) {
            return fail(DatasetContractError::ProductionTrainingUnauthorized, index, total_bytes);
        }
        if (sample.byte_size == 0U) {
            return fail(DatasetContractError::ZeroSampleBytes, index, total_bytes);
        }
        if (sample.byte_size > limits.max_sample_bytes) {
            return fail(DatasetContractError::SampleByteBudgetExceeded, index, total_bytes);
        }
        if (sample.byte_size > limits.max_total_bytes - total_bytes) {
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
