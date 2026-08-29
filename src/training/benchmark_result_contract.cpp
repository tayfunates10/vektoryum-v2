#include "vektoryum/training/benchmark_result_contract.hpp"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/training/dataset_contract.hpp"

namespace vektoryum::training {

namespace {

[[nodiscard]] BenchmarkResultValidation fail(BenchmarkResultError error, std::size_t index,
                                             std::uint64_t artifact_bytes) noexcept {
    return BenchmarkResultValidation{error, index, artifact_bytes};
}

[[nodiscard]] bool missing_identity(std::string_view first, std::string_view second) noexcept {
    return first.empty() || second.empty();
}

}  // namespace

std::string canonical_benchmark_report(const BenchmarkResultManifest& manifest) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "schema=" << manifest.schema_version << '\n'
           << "benchmark=" << manifest.benchmark_id << '@' << manifest.benchmark_version << '\n'
           << "run=" << manifest.run_id << '\n'
           << "dataset=" << manifest.dataset_id << '@' << manifest.dataset_version << '\n'
           << "model=" << manifest.model_id << '@' << manifest.model_version << '\n'
           << "runtime=" << manifest.runtime_id << '@' << manifest.runtime_version << '\n'
           << "seed=" << manifest.seed << '\n'
           << "evaluated_samples=" << manifest.evaluated_samples << '\n';
    output << std::scientific << std::setprecision(17);
    for (const BenchmarkMetric& metric : manifest.metrics) {
        output << "metric=" << metric.name << '@' << metric.version << '|'
               << metric.unit << '|' << metric.aggregation << '|' << metric.value << '\n';
    }
    return output.str();
}

BenchmarkResultValidation validate_benchmark_result(const BenchmarkResultManifest& manifest,
                                                    const std::vector<std::uint8_t>& artifact,
                                                    const BenchmarkResultLimits& limits) {
    const std::uint64_t artifact_bytes = static_cast<std::uint64_t>(artifact.size());
    if (manifest.schema_version.empty()) {
        return fail(BenchmarkResultError::MissingSchemaVersion, 0U, artifact_bytes);
    }
    if (missing_identity(manifest.benchmark_id, manifest.benchmark_version)) {
        return fail(BenchmarkResultError::MissingBenchmarkIdentity, 0U, artifact_bytes);
    }
    if (manifest.run_id.empty()) {
        return fail(BenchmarkResultError::MissingRunIdentity, 0U, artifact_bytes);
    }
    if (missing_identity(manifest.dataset_id, manifest.dataset_version)) {
        return fail(BenchmarkResultError::MissingDatasetIdentity, 0U, artifact_bytes);
    }
    if (missing_identity(manifest.model_id, manifest.model_version)) {
        return fail(BenchmarkResultError::MissingModelIdentity, 0U, artifact_bytes);
    }
    if (missing_identity(manifest.runtime_id, manifest.runtime_version)) {
        return fail(BenchmarkResultError::MissingRuntimeIdentity, 0U, artifact_bytes);
    }
    if (manifest.evaluated_samples == 0U) {
        return fail(BenchmarkResultError::ZeroEvaluatedSamples, 0U, artifact_bytes);
    }
    if (manifest.evaluated_samples > limits.max_evaluated_samples) {
        return fail(BenchmarkResultError::SampleBudgetExceeded, 0U, artifact_bytes);
    }
    if (manifest.metrics.empty()) {
        return fail(BenchmarkResultError::EmptyMetrics, 0U, artifact_bytes);
    }
    if (manifest.metrics.size() > limits.max_metrics) {
        return fail(BenchmarkResultError::TooManyMetrics, 0U, artifact_bytes);
    }

    std::string_view previous_name;
    for (std::size_t index = 0U; index < manifest.metrics.size(); ++index) {
        const BenchmarkMetric& metric = manifest.metrics[index];
        if (metric.name.empty() || metric.version.empty() || metric.unit.empty() ||
            metric.aggregation.empty()) {
            return fail(BenchmarkResultError::MissingMetricMetadata, index, artifact_bytes);
        }
        if (!std::isfinite(metric.value)) {
            return fail(BenchmarkResultError::NonFiniteMetricValue, index, artifact_bytes);
        }
        if (!previous_name.empty()) {
            if (metric.name == previous_name) {
                return fail(BenchmarkResultError::DuplicateMetricName, index, artifact_bytes);
            }
            if (metric.name < previous_name) {
                return fail(BenchmarkResultError::NonDeterministicMetricOrder, index, artifact_bytes);
            }
        }
        previous_name = metric.name;
    }

    if (!is_sha256_hex(manifest.artifact_sha256)) {
        return fail(BenchmarkResultError::InvalidArtifactDigest, 0U, artifact_bytes);
    }
    if (artifact.empty()) {
        return fail(BenchmarkResultError::EmptyArtifact, 0U, 0U);
    }
    if (artifact_bytes > limits.max_artifact_bytes) {
        return fail(BenchmarkResultError::ArtifactBudgetExceeded, 0U, artifact_bytes);
    }
    if (ml::sha256_hex(artifact) != manifest.artifact_sha256) {
        return fail(BenchmarkResultError::ArtifactDigestMismatch, 0U, artifact_bytes);
    }

    return BenchmarkResultValidation{BenchmarkResultError::None, manifest.metrics.size(), artifact_bytes};
}

}  // namespace vektoryum::training
