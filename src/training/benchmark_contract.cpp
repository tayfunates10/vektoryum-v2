#include "vektoryum/training/benchmark_contract.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <unordered_set>

#include "vektoryum/training/dataset_contract.hpp"

namespace vektoryum::training {

namespace {

[[nodiscard]] BenchmarkContractResult fail(BenchmarkContractError error,
                                           std::size_t metric_index) noexcept {
    return BenchmarkContractResult{error, metric_index};
}

[[nodiscard]] bool missing_identity(const BenchmarkResult& result) noexcept {
    return result.schema_version.empty() || result.benchmark_id.empty() ||
           result.benchmark_version.empty() || result.dataset_id.empty() ||
           result.dataset_version.empty() || result.model_id.empty() ||
           result.model_version.empty() || result.runtime_id.empty() ||
           result.training_run_id.empty();
}

[[nodiscard]] std::string metric_key(const BenchmarkMetric& metric) {
    return metric.name + "\n" + metric.version;
}

}  // namespace

BenchmarkContractResult validate_benchmark_result(const BenchmarkResult& result,
                                                  const BenchmarkLimits& limits) noexcept {
    if (missing_identity(result)) {
        return fail(BenchmarkContractError::MissingIdentity, 0U);
    }
    if (!is_sha256_hex(result.model_artifact_sha256) || !is_sha256_hex(result.config_sha256)) {
        return fail(BenchmarkContractError::InvalidArtifactDigest, 0U);
    }
    if (result.sample_count == 0U) {
        return fail(BenchmarkContractError::ZeroSamples, 0U);
    }
    if (result.sample_count > limits.max_samples) {
        return fail(BenchmarkContractError::SampleBudgetExceeded, 0U);
    }
    if (result.elapsed_milliseconds == 0U) {
        return fail(BenchmarkContractError::ZeroElapsedTime, 0U);
    }
    if (result.elapsed_milliseconds > limits.max_elapsed_milliseconds) {
        return fail(BenchmarkContractError::RuntimeBudgetExceeded, 0U);
    }
    if (result.report_bytes == 0U) {
        return fail(BenchmarkContractError::ZeroReportBytes, 0U);
    }
    if (result.report_bytes > limits.max_report_bytes) {
        return fail(BenchmarkContractError::ReportBudgetExceeded, 0U);
    }
    if (result.metrics.empty()) {
        return fail(BenchmarkContractError::EmptyMetrics, 0U);
    }
    if (result.metrics.size() > limits.max_metrics) {
        return fail(BenchmarkContractError::TooManyMetrics, 0U);
    }

    std::unordered_set<std::string> keys;
    keys.reserve(result.metrics.size());
    std::string previous_key;
    for (std::size_t index = 0U; index < result.metrics.size(); ++index) {
        const BenchmarkMetric& metric = result.metrics[index];
        if (metric.name.empty() || metric.version.empty() || metric.unit.empty()) {
            return fail(BenchmarkContractError::InvalidMetricIdentity, index);
        }
        if (!std::isfinite(metric.value)) {
            return fail(BenchmarkContractError::NonFiniteMetric, index);
        }
        const std::string key = metric_key(metric);
        if (!keys.insert(key).second) {
            return fail(BenchmarkContractError::DuplicateMetric, index);
        }
        if (index != 0U && key <= previous_key) {
            return fail(BenchmarkContractError::NonCanonicalMetricOrder, index);
        }
        previous_key = key;
    }

    return BenchmarkContractResult{BenchmarkContractError::None, result.metrics.size()};
}

std::string deterministic_benchmark_manifest(const BenchmarkResult& result) {
    std::ostringstream output;
    output.precision(17);
    output << "schema=" << result.schema_version << '\n'
           << "benchmark=" << result.benchmark_id << '@' << result.benchmark_version << '\n'
           << "dataset=" << result.dataset_id << '@' << result.dataset_version << '\n'
           << "model=" << result.model_id << '@' << result.model_version << '\n'
           << "model_artifact_sha256=" << result.model_artifact_sha256 << '\n'
           << "runtime=" << result.runtime_id << '\n'
           << "training_run=" << result.training_run_id << '\n'
           << "config_sha256=" << result.config_sha256 << '\n'
           << "sample_count=" << result.sample_count << '\n'
           << "elapsed_milliseconds=" << result.elapsed_milliseconds << '\n'
           << "report_bytes=" << result.report_bytes << '\n';
    for (const BenchmarkMetric& metric : result.metrics) {
        output << "metric=" << metric.name << '@' << metric.version << '|'
               << metric.unit << '|' << static_cast<unsigned int>(metric.aggregation) << '|'
               << metric.value << '\n';
    }
    return output.str();
}

}  // namespace vektoryum::training
