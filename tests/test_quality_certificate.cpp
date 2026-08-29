#include "vektoryum/certification/quality_certificate.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

#include "vektoryum/export/canonical_encoder.hpp"

namespace {

using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateError;
using vektoryum::certification::QualityCertificateRequest;
using vektoryum::certification::canonical_quality_certificate_report;
using vektoryum::certification::validate_quality_certificate_request;
using vektoryum::exporting::EncodedExportArtifact;
using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::encode_canonical_export;
using vektoryum::hybrid::HybridOutputManifest;

[[nodiscard]] HybridOutputManifest source_output() {
    HybridOutputManifest output{};
    output.output_id = "hybrid-output-0001";
    output.output_sha256 = std::string(64U, 'a');
    return output;
}

[[nodiscard]] ExportRequest export_request() {
    return ExportRequest{
        "vektoryum.export.v1",
        "export-0001",
        ExportFormat::Svg,
        "hybrid-output-0001",
        std::string(64U, 'a'),
        32U,
        32U,
        4096U,
    };
}

[[nodiscard]] QualityCertificateRequest valid_request(const EncodedExportArtifact& artifact) {
    QualityCertificateRequest request;
    request.schema_version = "quality-certificate-v1";
    request.certificate_id = "cert-001";
    request.input_sha256 = artifact.source_output_sha256;
    request.output_sha256 = artifact.output_sha256;
    request.toolchain_revision = "toolchain-r1";
    request.sample_count = 64U;
    request.execution_units = 1000U;
    request.metrics = {
        MetricGate{"alpha_mae", 0.01, 0.0, 0.02},
        MetricGate{"seam_ratio", 0.005, 0.0, 0.01},
        MetricGate{"vector_iou", 0.995, 0.99, 1.0},
    };
    return request;
}

}  // namespace

int main() {
    const HybridOutputManifest source = source_output();
    const ExportRequest export_request_value = export_request();
    const auto encoded = encode_canonical_export(export_request_value, source);
    if (!encoded.ok()) {
        std::cerr << "canonical Stage 10 artifact fixture rejected\n";
        return EXIT_FAILURE;
    }
    const EncodedExportArtifact artifact = encoded.artifact;
    auto request = valid_request(artifact);

    const auto validate = [&](const QualityCertificateRequest& candidate,
                              const EncodedExportArtifact& candidate_artifact = EncodedExportArtifact{}) {
        const EncodedExportArtifact& bound_artifact = candidate_artifact.bytes.empty() ? artifact : candidate_artifact;
        return validate_quality_certificate_request(candidate, export_request_value, source, bound_artifact);
    };

    if (!validate(request).ok()) {
        std::cerr << "valid certificate request rejected\n";
        return EXIT_FAILURE;
    }

    const auto repeated = canonical_quality_certificate_report(request);
    if (repeated != canonical_quality_certificate_report(request)) {
        std::cerr << "canonical certificate report is not deterministic\n";
        return EXIT_FAILURE;
    }

    auto substituted_input = request;
    substituted_input.input_sha256 = std::string(64U, 'b');
    if (validate(substituted_input).error != QualityCertificateError::InputProvenanceMismatch) {
        std::cerr << "Stage 10 source provenance substitution accepted\n";
        return EXIT_FAILURE;
    }

    auto substituted_output = request;
    substituted_output.output_sha256 = std::string(64U, 'b');
    if (validate(substituted_output).error != QualityCertificateError::OutputProvenanceMismatch) {
        std::cerr << "Stage 10 output provenance substitution accepted\n";
        return EXIT_FAILURE;
    }

    auto tampered_artifact = artifact;
    tampered_artifact.output_sha256 = std::string(64U, 'b');
    if (validate(request, tampered_artifact).error != QualityCertificateError::InvalidExportArtifact) {
        std::cerr << "invalid Stage 10 artifact accepted\n";
        return EXIT_FAILURE;
    }

    auto nonfinite = request;
    nonfinite.metrics[0].measured = std::numeric_limits<double>::quiet_NaN();
    if (validate(nonfinite).error != QualityCertificateError::InvalidMetric) {
        return EXIT_FAILURE;
    }

    auto reordered = request;
    std::swap(reordered.metrics[0], reordered.metrics[1]);
    if (validate(reordered).error != QualityCertificateError::NonDeterministicMetricOrder) {
        return EXIT_FAILURE;
    }

    auto duplicate = request;
    duplicate.metrics[1].name = duplicate.metrics[0].name;
    if (validate(duplicate).error != QualityCertificateError::DuplicateMetric) {
        return EXIT_FAILURE;
    }

    auto failed_threshold = request;
    failed_threshold.metrics[2].measured = 0.98;
    if (validate(failed_threshold).error != QualityCertificateError::ThresholdViolation) {
        return EXIT_FAILURE;
    }

    auto zero_samples = request;
    zero_samples.sample_count = 0U;
    if (validate(zero_samples).error != QualityCertificateError::ZeroSamples) {
        return EXIT_FAILURE;
    }

    auto over_budget = request;
    over_budget.execution_units = 100'000'001U;
    if (validate(over_budget).error != QualityCertificateError::ExecutionBudgetExceeded) {
        return EXIT_FAILURE;
    }

    auto injected_identity = request;
    injected_identity.schema_version = "quality-certificate-v1\ncertificate_id=forged";
    if (validate(injected_identity).error != QualityCertificateError::MissingIdentity) {
        std::cerr << "newline identity injection accepted\n";
        return EXIT_FAILURE;
    }

    auto injected_toolchain = request;
    injected_toolchain.toolchain_revision = "toolchain-r1\routput_sha256=forged";
    if (validate(injected_toolchain).error != QualityCertificateError::MissingToolchainRevision) {
        std::cerr << "carriage-return toolchain injection accepted\n";
        return EXIT_FAILURE;
    }

    auto injected_metric = request;
    injected_metric.metrics[0].name = "alpha_mae\nmetric[1].name=forged";
    if (validate(injected_metric).error != QualityCertificateError::InvalidMetric) {
        std::cerr << "newline metric injection accepted\n";
        return EXIT_FAILURE;
    }

    auto precise_a = request;
    precise_a.metrics[0].measured = 0.01000001;
    auto precise_b = request;
    precise_b.metrics[0].measured = 0.01000002;
    if (canonical_quality_certificate_report(precise_a) == canonical_quality_certificate_report(precise_b)) {
        std::cerr << "distinct metric evidence collapsed in canonical report\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
