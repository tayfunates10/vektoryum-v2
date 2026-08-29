#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vektoryum::training {

struct BenchmarkMetric {
    std::string name;
    std::string version;
    std::string unit;
    std::string aggregation;
    double value{0.0};
};

struct BenchmarkResultManifest {
    std::string schema_version;
    std::string benchmark_id;
    std::string benchmark_version;
    std::string run_id;
    std::string dataset_id;
    std::string dataset_version;
    std::string model_id;
    std::string model_version;
    std::string runtime_id;
    std::string runtime_version;
    std::uint64_t seed{0U};
    std::uint64_t evaluated_samples{0U};
    std::vector<BenchmarkMetric> metrics;
    std::string artifact_sha256;
};

struct BenchmarkResultLimits {
    std::uint64_t max_evaluated_samples{10'000'000U};
    std::size_t max_metrics{256U};
    std::uint64_t max_artifact_bytes{64ULL * 1024ULL * 1024ULL};
};

enum class BenchmarkResultError : std::uint8_t {
    None,
    MissingSchemaVersion,
    MissingBenchmarkIdentity,
    MissingRunIdentity,
    MissingDatasetIdentity,
    MissingModelIdentity,
    MissingRuntimeIdentity,
    ZeroEvaluatedSamples,
    SampleBudgetExceeded,
    EmptyMetrics,
    TooManyMetrics,
    MissingMetricMetadata,
    DuplicateMetricName,
    NonFiniteMetricValue,
    NonDeterministicMetricOrder,
    InvalidArtifactDigest,
    EmptyArtifact,
    ArtifactBudgetExceeded,
    ArtifactDigestMismatch,
};

struct BenchmarkResultValidation {
    BenchmarkResultError error{BenchmarkResultError::None};
    std::size_t metric_index{0U};
    std::uint64_t artifact_bytes{0U};

    [[nodiscard]] bool ok() const noexcept { return error == BenchmarkResultError::None; }
};

[[nodiscard]] std::string canonical_benchmark_report(const BenchmarkResultManifest& manifest);
[[nodiscard]] BenchmarkResultValidation validate_benchmark_result(
    const BenchmarkResultManifest& manifest,
    const std::vector<std::uint8_t>& artifact,
    const BenchmarkResultLimits& limits = {});

}  // namespace vektoryum::training
