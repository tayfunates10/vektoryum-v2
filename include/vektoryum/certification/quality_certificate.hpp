#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vektoryum::certification {

struct MetricGate {
    std::string name;
    double measured{0.0};
    double minimum{0.0};
    double maximum{0.0};
};

struct QualityCertificateRequest {
    std::string schema_version;
    std::string certificate_id;
    std::string input_sha256;
    std::string output_sha256;
    std::string toolchain_revision;
    std::uint64_t sample_count{0U};
    std::uint64_t execution_units{0U};
    std::vector<MetricGate> metrics;
};

struct QualityCertificateLimits {
    std::uint64_t max_samples{1'000'000U};
    std::uint64_t max_execution_units{100'000'000U};
    std::size_t max_metrics{128U};
};

enum class QualityCertificateError : std::uint8_t {
    None,
    MissingIdentity,
    InvalidDigest,
    MissingToolchainRevision,
    ZeroSamples,
    SampleBudgetExceeded,
    ZeroExecutionUnits,
    ExecutionBudgetExceeded,
    EmptyMetrics,
    TooManyMetrics,
    InvalidMetric,
    DuplicateMetric,
    NonDeterministicMetricOrder,
    ThresholdViolation,
};

struct QualityCertificateValidation {
    QualityCertificateError error{QualityCertificateError::None};
    std::size_t metric_index{0U};
    [[nodiscard]] bool ok() const noexcept { return error == QualityCertificateError::None; }
};

[[nodiscard]] QualityCertificateValidation validate_quality_certificate_request(
    const QualityCertificateRequest& request,
    const QualityCertificateLimits& limits = {});
[[nodiscard]] std::string canonical_quality_certificate_report(const QualityCertificateRequest& request);

}  // namespace vektoryum::certification
