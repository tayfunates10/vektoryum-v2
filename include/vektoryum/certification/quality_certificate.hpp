#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/export/export_artifact_contract.hpp"

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
    InvalidExportArtifact,
    InputProvenanceMismatch,
    OutputProvenanceMismatch,
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

struct QualityCertificateArtifact {
    std::string certificate_id;
    std::string input_sha256;
    std::string output_sha256;
    std::string toolchain_revision;
    std::vector<std::uint8_t> canonical_bytes;
    std::string certificate_sha256;
};

struct QualityCertificateIssueResult {
    QualityCertificateValidation validation{};
    QualityCertificateArtifact artifact{};
    [[nodiscard]] bool ok() const noexcept { return validation.ok(); }
};

[[nodiscard]] QualityCertificateValidation validate_quality_certificate_request(
    const QualityCertificateRequest& request,
    const exporting::ExportRequest& export_request,
    const hybrid::HybridOutputManifest& source_output,
    const exporting::EncodedExportArtifact& export_artifact,
    const QualityCertificateLimits& limits = {},
    const exporting::ExportLimits& export_limits = {},
    const exporting::ExportExecutionLimits& export_execution_limits = {});
[[nodiscard]] std::string canonical_quality_certificate_report(const QualityCertificateRequest& request);
[[nodiscard]] QualityCertificateIssueResult issue_quality_certificate(
    const QualityCertificateRequest& request,
    const exporting::ExportRequest& export_request,
    const hybrid::HybridOutputManifest& source_output,
    const exporting::EncodedExportArtifact& export_artifact,
    const QualityCertificateLimits& limits = {},
    const exporting::ExportLimits& export_limits = {},
    const exporting::ExportExecutionLimits& export_execution_limits = {});

}  // namespace vektoryum::certification
