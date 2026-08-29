#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::training {

enum class MetricAggregation : std::uint8_t {
    Mean,
    Median,
    Minimum,
    Maximum,
};

struct BenchmarkMetric {
    std::string name;
    std::string version;
    std::string unit;
    double value{0.0};
    MetricAggregation aggregation{MetricAggregation::Mean};
};

struct BenchmarkResult {
    std::string schema_version;
    std::string benchmark_id;
    std::string benchmark_version;
    std::string dataset_id;
    std::string dataset_version;
    std::string model_id;
    std::string model_version;
    std::string model_artifact_sha256;
    std::string runtime_id;
    std::string training_run_id;
    std::string config_sha256;
    std::uint64_t sample_count{0U};
    std::uint64_t elapsed_milliseconds{0U};
    std::uint64_t report_bytes{0U};
    std::vector<BenchmarkMetric> metrics;
};

struct BenchmarkLimits {
    std::uint64_t max_samples{1'000'000U};
    std::uint64_t max_elapsed_milliseconds{24ULL * 60ULL * 60ULL * 1000ULL};
    std::uint64_t max_report_bytes{16ULL * 1024ULL * 1024ULL};
    std::size_t max_metrics{128U};
};

enum class BenchmarkContractError : std::uint8_t {
    None,
    MissingIdentity,
    InvalidArtifactDigest,
    EmptyMetrics,
    TooManyMetrics,
    InvalidMetricIdentity,
    NonFiniteMetric,
    DuplicateMetric,
    NonCanonicalMetricOrder,
    ZeroSamples,
    SampleBudgetExceeded,
    ZeroElapsedTime,
    RuntimeBudgetExceeded,
    ZeroReportBytes,
    ReportBudgetExceeded,
};

struct BenchmarkContractResult {
    BenchmarkContractError error{BenchmarkContractError::None};
    std::size_t metric_index{0U};

    [[nodiscard]] bool ok() const noexcept { return error == BenchmarkContractError::None; }
};

[[nodiscard]] BenchmarkContractResult validate_benchmark_result(
    const BenchmarkResult& result, const BenchmarkLimits& limits = {}) noexcept;

[[nodiscard]] std::string deterministic_benchmark_manifest(const BenchmarkResult& result);

}  // namespace vektoryum::training
