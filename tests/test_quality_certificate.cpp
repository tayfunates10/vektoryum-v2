#include "vektoryum/certification/quality_certificate.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "vektoryum/export/canonical_encoder.hpp"
#include "vektoryum/ml/artifact_digest.hpp"

namespace {

using vektoryum::certification::CanonicalQualityFixture;
using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateError;
using vektoryum::certification::QualityCertificateRequest;
using vektoryum::certification::canonical_quality_certificate_report;
using vektoryum::certification::issue_quality_certificate;
using vektoryum::certification::measure_canonical_export_metrics;
using vektoryum::certification::measure_canonical_quality_metrics;
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
    output.seam_error = 0.005;
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

    const CanonicalQualityFixture perfect_quality{
        std::vector<std::uint8_t>{0U, 64U, 128U, 255U},
        std::vector<std::uint8_t>{0U, 64U, 128U, 255U},
        std::vector<std::uint8_t>{0U, 1U, 1U, 0U},
        std::vector<std::uint8_t>{0U, 1U, 1U, 0U},
    };
    const auto quality_a = measure_canonical_quality_metrics(perfect_quality);
    const auto quality_b = measure_canonical_quality_metrics(perfect_quality);
    if (!quality_a.ok() || !quality_b.ok() || quality_a.sample_count != 4U || quality_a.metrics.size() != 2U ||
        quality_a.metrics[0].name != "alpha_mae" || quality_a.metrics[0].measured != 0.0 ||
        quality_a.metrics[0].maximum != 0.02 || quality_a.metrics[1].name != "vector_iou" ||
        quality_a.metrics[1].measured != 1.0 || quality_a.metrics[1].minimum != 0.99) {
        std::cerr << "canonical quality fixture measurement mismatch\n";
        return EXIT_FAILURE;
    }
    if (quality_a.sample_count != quality_b.sample_count || quality_a.metrics.size() != quality_b.metrics.size()) {
        std::cerr << "canonical quality repeatability failed\n";
        return EXIT_FAILURE;
    }
    for (std::size_t i = 0U; i < quality_a.metrics.size(); ++i) {
        if (quality_a.metrics[i].name != quality_b.metrics[i].name ||
            quality_a.metrics[i].measured != quality_b.metrics[i].measured ||
            quality_a.metrics[i].minimum != quality_b.metrics[i].minimum ||
            quality_a.metrics[i].maximum != quality_b.metrics[i].maximum) {
            std::cerr << "canonical quality evidence is not repeatable\n";
            return EXIT_FAILURE;
        }
    }

    auto bad_alpha = perfect_quality;
    bad_alpha.candidate_alpha[1] = 255U;
    if (measure_canonical_quality_metrics(bad_alpha).error != QualityCertificateError::ThresholdViolation) {
        std::cerr << "excessive alpha error passed canonical quality gate\n";
        return EXIT_FAILURE;
    }

    CanonicalQualityFixture bad_vector;
    bad_vector.reference_alpha.assign(100U, 255U);
    bad_vector.candidate_alpha.assign(100U, 255U);
    bad_vector.reference_vector_mask.assign(100U, 1U);
    bad_vector.candidate_vector_mask.assign(100U, 1U);
    bad_vector.candidate_vector_mask[0] = 0U;
    bad_vector.candidate_vector_mask[1] = 0U;
    if (measure_canonical_quality_metrics(bad_vector).error != QualityCertificateError::ThresholdViolation) {
        std::cerr << "sub-threshold vector IoU passed canonical quality gate\n";
        return EXIT_FAILURE;
    }

    auto malformed_quality = perfect_quality;
    malformed_quality.candidate_alpha.pop_back();
    if (measure_canonical_quality_metrics(malformed_quality).error != QualityCertificateError::InvalidQualityFixture) {
        std::cerr << "mismatched canonical quality fixture accepted\n";
        return EXIT_FAILURE;
    }

    malformed_quality = perfect_quality;
    malformed_quality.candidate_vector_mask[0] = 2U;
    if (measure_canonical_quality_metrics(malformed_quality).error != QualityCertificateError::InvalidQualityFixture) {
        std::cerr << "non-binary vector mask accepted\n";
        return EXIT_FAILURE;
    }

    if (measure_canonical_quality_metrics(perfect_quality, 3U).error != QualityCertificateError::SampleBudgetExceeded) {
        std::cerr << "canonical quality sample budget not enforced\n";
        return EXIT_FAILURE;
    }

    const auto measured_a = measure_canonical_export_metrics(export_request_value, source, artifact);
    const auto measured_b = measure_canonical_export_metrics(export_request_value, source, artifact);
    if (!measured_a.ok() || !measured_b.ok()) {
        std::cerr << "canonical metric measurement rejected valid Stage 10 artifact\n";
        return EXIT_FAILURE;
    }
    if (measured_a.sample_count != 1024U || measured_a.execution_units != artifact.execution_units ||
        measured_a.peak_memory_bytes != artifact.peak_intermediate_bytes || measured_a.metrics.size() != 4U) {
        std::cerr << "canonical metric resource evidence mismatch\n";
        return EXIT_FAILURE;
    }
    if (measured_a.metrics[0].name != "export_bytes" ||
        measured_a.metrics[0].measured != static_cast<double>(artifact.bytes.size()) ||
        measured_a.metrics[1].name != "peak_intermediate_bytes" ||
        measured_a.metrics[1].measured != static_cast<double>(artifact.peak_intermediate_bytes) ||
        measured_a.metrics[2].name != "seam_error" || measured_a.metrics[2].measured != source.seam_error ||
        measured_a.metrics[3].name != "work_units" ||
        measured_a.metrics[3].measured != static_cast<double>(artifact.execution_units)) {
        std::cerr << "canonical metric values do not bind Stage 10 output\n";
        return EXIT_FAILURE;
    }
    if (measured_a.metrics.size() != measured_b.metrics.size()) {
        std::cerr << "canonical metric repeatability failed\n";
        return EXIT_FAILURE;
    }
    for (std::size_t i = 0U; i < measured_a.metrics.size(); ++i) {
        if (measured_a.metrics[i].name != measured_b.metrics[i].name ||
            measured_a.metrics[i].measured != measured_b.metrics[i].measured ||
            measured_a.metrics[i].minimum != measured_b.metrics[i].minimum ||
            measured_a.metrics[i].maximum != measured_b.metrics[i].maximum) {
            std::cerr << "canonical metric evidence is not repeatable\n";
            return EXIT_FAILURE;
        }
    }

    auto tampered_measurement_artifact = artifact;
    tampered_measurement_artifact.bytes.push_back('x');
    if (measure_canonical_export_metrics(export_request_value, source, tampered_measurement_artifact).error !=
        QualityCertificateError::InvalidExportArtifact) {
        std::cerr << "tampered artifact produced quality metrics\n";
        return EXIT_FAILURE;
    }

    auto excessive_seam_source = source;
    excessive_seam_source.seam_error = 0.02;
    if (measure_canonical_export_metrics(export_request_value, excessive_seam_source, artifact).error !=
        QualityCertificateError::ThresholdViolation) {
        std::cerr << "excessive seam evidence passed concrete metric gate\n";
        return EXIT_FAILURE;
    }

    const auto repeated = canonical_quality_certificate_report(request);
    if (repeated != canonical_quality_certificate_report(request)) {
        std::cerr << "canonical certificate report is not deterministic\n";
        return EXIT_FAILURE;
    }

    const auto issued_a = issue_quality_certificate(request, export_request_value, source, artifact);
    const auto issued_b = issue_quality_certificate(request, export_request_value, source, artifact);
    if (!issued_a.ok() || !issued_b.ok()) {
        std::cerr << "valid certificate issuance failed\n";
        return EXIT_FAILURE;
    }
    if (issued_a.artifact.canonical_bytes != issued_b.artifact.canonical_bytes ||
        issued_a.artifact.certificate_sha256 != issued_b.artifact.certificate_sha256) {
        std::cerr << "certificate artifact is not deterministic\n";
        return EXIT_FAILURE;
    }
    if (issued_a.artifact.certificate_id != request.certificate_id ||
        issued_a.artifact.input_sha256 != request.input_sha256 ||
        issued_a.artifact.output_sha256 != request.output_sha256 ||
        issued_a.artifact.toolchain_revision != request.toolchain_revision) {
        std::cerr << "certificate artifact lost provenance\n";
        return EXIT_FAILURE;
    }
    if (vektoryum::ml::sha256_hex(issued_a.artifact.canonical_bytes) != issued_a.artifact.certificate_sha256) {
        std::cerr << "certificate artifact digest does not bind canonical bytes\n";
        return EXIT_FAILURE;
    }

    auto substituted_input = request;
    substituted_input.input_sha256 = std::string(64U, 'b');
    if (validate(substituted_input).error != QualityCertificateError::InputProvenanceMismatch) {
        std::cerr << "Stage 10 source provenance substitution accepted\n";
        return EXIT_FAILURE;
    }
    if (issue_quality_certificate(substituted_input, export_request_value, source, artifact).ok()) {
        std::cerr << "invalid provenance issued a certificate artifact\n";
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
