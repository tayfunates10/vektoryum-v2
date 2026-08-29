#include "vektoryum/certification/quality_certificate.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::certification {
namespace {

[[nodiscard]] bool is_lower_hex_sha256(const std::string& value) {
    if (value.size() != 64U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

[[nodiscard]] bool contains_report_delimiter(const std::string& value) {
    return value.find_first_of("\n\r\0", 0U, 3U) != std::string::npos;
}

}  // namespace

QualityCertificateValidation validate_quality_certificate_request(
    const QualityCertificateRequest& request,
    const exporting::ExportRequest& export_request,
    const hybrid::HybridOutputManifest& source_output,
    const exporting::EncodedExportArtifact& export_artifact,
    const QualityCertificateLimits& limits,
    const exporting::ExportLimits& export_limits,
    const exporting::ExportExecutionLimits& export_execution_limits) {
    if (!exporting::validate_encoded_export_artifact(
             export_request, source_output, export_artifact, export_limits, export_execution_limits)
             .ok()) {
        return {QualityCertificateError::InvalidExportArtifact, 0U};
    }
    if (request.input_sha256 != export_artifact.source_output_sha256) {
        return {QualityCertificateError::InputProvenanceMismatch, 0U};
    }
    if (request.output_sha256 != export_artifact.output_sha256) {
        return {QualityCertificateError::OutputProvenanceMismatch, 0U};
    }
    if (request.schema_version.empty() || request.certificate_id.empty() ||
        contains_report_delimiter(request.schema_version) || contains_report_delimiter(request.certificate_id)) {
        return {QualityCertificateError::MissingIdentity, 0U};
    }
    if (!is_lower_hex_sha256(request.input_sha256) || !is_lower_hex_sha256(request.output_sha256)) {
        return {QualityCertificateError::InvalidDigest, 0U};
    }
    if (request.toolchain_revision.empty() || contains_report_delimiter(request.toolchain_revision)) {
        return {QualityCertificateError::MissingToolchainRevision, 0U};
    }
    if (request.sample_count == 0U) {
        return {QualityCertificateError::ZeroSamples, 0U};
    }
    if (request.sample_count > limits.max_samples) {
        return {QualityCertificateError::SampleBudgetExceeded, 0U};
    }
    if (request.execution_units == 0U) {
        return {QualityCertificateError::ZeroExecutionUnits, 0U};
    }
    if (request.execution_units > limits.max_execution_units) {
        return {QualityCertificateError::ExecutionBudgetExceeded, 0U};
    }
    if (request.metrics.empty()) {
        return {QualityCertificateError::EmptyMetrics, 0U};
    }
    if (request.metrics.size() > limits.max_metrics) {
        return {QualityCertificateError::TooManyMetrics, 0U};
    }

    std::string previous_name;
    for (std::size_t index = 0U; index < request.metrics.size(); ++index) {
        const auto& metric = request.metrics[index];
        if (metric.name.empty() || contains_report_delimiter(metric.name) || !std::isfinite(metric.measured) ||
            !std::isfinite(metric.minimum) || !std::isfinite(metric.maximum) || metric.minimum > metric.maximum) {
            return {QualityCertificateError::InvalidMetric, index};
        }
        if (!previous_name.empty()) {
            if (metric.name == previous_name) {
                return {QualityCertificateError::DuplicateMetric, index};
            }
            if (metric.name < previous_name) {
                return {QualityCertificateError::NonDeterministicMetricOrder, index};
            }
        }
        if (metric.measured < metric.minimum || metric.measured > metric.maximum) {
            return {QualityCertificateError::ThresholdViolation, index};
        }
        previous_name = metric.name;
    }
    return {};
}

std::string canonical_quality_certificate_report(const QualityCertificateRequest& request) {
    std::ostringstream report;
    report.imbue(std::locale::classic());
    report << std::setprecision(std::numeric_limits<double>::max_digits10);
    report << "schema_version=" << request.schema_version << '\n';
    report << "certificate_id=" << request.certificate_id << '\n';
    report << "input_sha256=" << request.input_sha256 << '\n';
    report << "output_sha256=" << request.output_sha256 << '\n';
    report << "toolchain_revision=" << request.toolchain_revision << '\n';
    report << "sample_count=" << request.sample_count << '\n';
    report << "execution_units=" << request.execution_units << '\n';
    for (std::size_t index = 0U; index < request.metrics.size(); ++index) {
        const auto& metric = request.metrics[index];
        report << "metric[" << index << "].name=" << metric.name << '\n';
        report << "metric[" << index << "].measured=" << metric.measured << '\n';
        report << "metric[" << index << "].minimum=" << metric.minimum << '\n';
        report << "metric[" << index << "].maximum=" << metric.maximum << '\n';
    }
    return report.str();
}

QualityCertificateIssueResult issue_quality_certificate(
    const QualityCertificateRequest& request,
    const exporting::ExportRequest& export_request,
    const hybrid::HybridOutputManifest& source_output,
    const exporting::EncodedExportArtifact& export_artifact,
    const QualityCertificateLimits& limits,
    const exporting::ExportLimits& export_limits,
    const exporting::ExportExecutionLimits& export_execution_limits) {
    const QualityCertificateValidation validation = validate_quality_certificate_request(
        request, export_request, source_output, export_artifact, limits, export_limits, export_execution_limits);
    if (!validation.ok()) {
        return {validation, {}};
    }

    const std::string report = canonical_quality_certificate_report(request);
    QualityCertificateArtifact artifact;
    artifact.certificate_id = request.certificate_id;
    artifact.input_sha256 = request.input_sha256;
    artifact.output_sha256 = request.output_sha256;
    artifact.toolchain_revision = request.toolchain_revision;
    artifact.canonical_bytes.assign(report.begin(), report.end());
    artifact.certificate_sha256 = ml::sha256_hex(artifact.canonical_bytes);
    return {validation, std::move(artifact)};
}

CanonicalMetricMeasurement measure_canonical_export_metrics(
    const exporting::ExportRequest& export_request,
    const hybrid::HybridOutputManifest& source_output,
    const exporting::EncodedExportArtifact& export_artifact,
    const exporting::ExportLimits& export_limits,
    const exporting::ExportExecutionLimits& export_execution_limits) {
    const auto artifact_validation = exporting::validate_encoded_export_artifact(
        export_request, source_output, export_artifact, export_limits, export_execution_limits);
    if (!artifact_validation.ok()) {
        return {QualityCertificateError::InvalidExportArtifact, 0U, 0U, 0U, 0U, {}};
    }

    const std::uint64_t output_bytes = static_cast<std::uint64_t>(export_artifact.bytes.size());
    CanonicalMetricMeasurement measurement;
    measurement.sample_count = static_cast<std::uint64_t>(export_request.width) * export_request.height;
    measurement.execution_units = export_artifact.execution_units;
    measurement.peak_memory_bytes = export_artifact.peak_intermediate_bytes;
    measurement.metrics = {
        MetricGate{"export_bytes", static_cast<double>(output_bytes), 1.0,
                   static_cast<double>(export_request.estimated_output_bytes)},
        MetricGate{"peak_intermediate_bytes", static_cast<double>(export_artifact.peak_intermediate_bytes), 1.0,
                   static_cast<double>(export_execution_limits.max_intermediate_bytes)},
        MetricGate{"seam_error", source_output.seam_error, 0.0, 0.01},
        MetricGate{"work_units", static_cast<double>(export_artifact.execution_units), 1.0,
                   static_cast<double>(export_execution_limits.max_execution_units)},
    };

    QualityCertificateRequest measured_request;
    measured_request.schema_version = "vektoryum.quality.measurement.v1";
    measured_request.certificate_id = "measurement";
    measured_request.input_sha256 = export_artifact.source_output_sha256;
    measured_request.output_sha256 = export_artifact.output_sha256;
    measured_request.toolchain_revision = "measurement";
    measured_request.sample_count = measurement.sample_count;
    measured_request.execution_units = measurement.execution_units;
    measured_request.metrics = measurement.metrics;

    const QualityCertificateLimits measurement_limits{
        export_limits.max_pixels,
        export_execution_limits.max_execution_units,
        measurement.metrics.size(),
    };
    const auto validation = validate_quality_certificate_request(
        measured_request, export_request, source_output, export_artifact, measurement_limits, export_limits,
        export_execution_limits);
    measurement.error = validation.error;
    measurement.metric_index = validation.metric_index;
    return measurement;
}

}  // namespace vektoryum::certification
