#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#include "vektoryum/training/benchmark_contract.hpp"

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

[[nodiscard]] vektoryum::training::BenchmarkResult valid_result() {
    using namespace vektoryum::training;
    BenchmarkResult result;
    result.schema_version = "vektoryum-benchmark-result-v1";
    result.benchmark_id = "stage8-regression";
    result.benchmark_version = "1";
    result.dataset_id = "stage8-dataset";
    result.dataset_version = "1";
    result.model_id = "reference-runtime-model";
    result.model_version = "1";
    result.model_artifact_sha256 =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    result.runtime_id = "deterministic-reference-runtime";
    result.training_run_id = "stage8-training-run-001";
    result.config_sha256 =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    result.sample_count = 128U;
    result.elapsed_milliseconds = 2500U;
    result.report_bytes = 4096U;
    result.metrics.push_back(BenchmarkMetric{"fidelity.mae", "1", "normalized", 0.01,
                                             MetricAggregation::Mean});
    result.metrics.push_back(BenchmarkMetric{"fidelity.psnr", "1", "dB", 42.0,
                                             MetricAggregation::Mean});
    return result;
}

}  // namespace

int main() {
    using namespace vektoryum::training;

    const BenchmarkResult baseline = valid_result();
    expect_true(validate_benchmark_result(baseline).ok(),
                "fully bound benchmark provenance is accepted");
    const std::string manifest_a = deterministic_benchmark_manifest(baseline);
    const std::string manifest_b = deterministic_benchmark_manifest(baseline);
    expect_true(manifest_a == manifest_b,
                "benchmark result manifest is byte-deterministic for identical input");
    expect_true(manifest_a.find("model_artifact_sha256=") != std::string::npos &&
                    manifest_a.find("config_sha256=") != std::string::npos,
                "manifest preserves model and configuration provenance");

    BenchmarkResult invalid = baseline;
    invalid.config_sha256[0] = 'z';
    expect_true(validate_benchmark_result(invalid).error ==
                    BenchmarkContractError::InvalidArtifactDigest,
                "malformed configuration digest is rejected");

    invalid = baseline;
    invalid.metrics[0].value = std::numeric_limits<double>::infinity();
    expect_true(validate_benchmark_result(invalid).error ==
                    BenchmarkContractError::NonFiniteMetric,
                "non-finite benchmark metric is rejected");

    invalid = baseline;
    invalid.metrics.push_back(invalid.metrics.back());
    expect_true(validate_benchmark_result(invalid).error == BenchmarkContractError::DuplicateMetric,
                "duplicate metric identity is rejected");

    invalid = baseline;
    const BenchmarkMetric first = invalid.metrics.front();
    invalid.metrics.front() = invalid.metrics.back();
    invalid.metrics.back() = first;
    expect_true(validate_benchmark_result(invalid).error ==
                    BenchmarkContractError::NonCanonicalMetricOrder,
                "nondeterministic metric ordering is rejected");

    invalid = baseline;
    invalid.sample_count = 0U;
    expect_true(validate_benchmark_result(invalid).error == BenchmarkContractError::ZeroSamples,
                "empty benchmark execution is rejected");

    BenchmarkLimits limits;
    limits.max_samples = 64U;
    expect_true(validate_benchmark_result(baseline, limits).error ==
                    BenchmarkContractError::SampleBudgetExceeded,
                "benchmark sample budget is fail-closed");

    limits = BenchmarkLimits{};
    limits.max_elapsed_milliseconds = 1000U;
    expect_true(validate_benchmark_result(baseline, limits).error ==
                    BenchmarkContractError::RuntimeBudgetExceeded,
                "benchmark runtime budget is fail-closed");

    limits = BenchmarkLimits{};
    limits.max_report_bytes = 1024U;
    expect_true(validate_benchmark_result(baseline, limits).error ==
                    BenchmarkContractError::ReportBudgetExceeded,
                "benchmark report budget is fail-closed");

    if (failures != 0) {
        std::cerr << failures << " benchmark contract test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All benchmark contract tests passed\n";
    return EXIT_SUCCESS;
}
