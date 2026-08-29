#include "vektoryum/certification/quality_certificate.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

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

}  // namespace vektoryum::certification
