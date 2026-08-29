#include "vektoryum/certification/quality_certificate.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "vektoryum/export/canonical_encoder.hpp"

namespace {

using vektoryum::certification::CanonicalQualityFixture;
using vektoryum::certification::MetricGate;
using vektoryum::certification::QualityCertificateError;
using vektoryum::certification::QualityCertificateRequest;
using vektoryum::certification::issue_quality_certificate;
using vektoryum::certification::measure_canonical_export_metrics;
using vektoryum::certification::measure_canonical_quality_metrics;
using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::encode_canonical_export;
using vektoryum::hybrid::HybridOutputManifest;

[[nodiscard]] HybridOutputManifest source_output() {
    HybridOutputManifest output{};
    output.output_id = "hybrid-output-stage11-e2e";
    output.output_sha256 = std::string(64U, 'a');
    output.seam_error = 0.005;
    return output;
}

[[nodiscard]] ExportRequest export_request(ExportFormat format, const std::string& export_id) {
    return ExportRequest{
        "vektoryum.export.v1",
        export_id,
        format,
        "hybrid-output-stage11-e2e",
        std::string(64U, 'a'),
        32U,
        32U,
        4096U,
    };
}

[[nodiscard]] CanonicalQualityFixture quality_fixture() {
    return CanonicalQualityFixture{
        std::vector<std::uint8_t>{0U, 64U, 128U, 255U},
        std::vector<std::uint8_t>{0U, 64U, 128U, 255U},
        std::vector<std::uint8_t>{0U, 1U, 1U, 0U},
        std::vector<std::uint8_t>{0U, 1U, 1U, 0U},
    };
}

[[nodiscard]] QualityCertificateRequest measured_request(
    const std::string& certificate_id,
    const vektoryum::exporting::EncodedExportArtifact& artifact,
    const vektoryum::certification::CanonicalMetricMeasurement& performance,
    const vektoryum::certification::CanonicalQualityMeasurement& quality) {
    QualityCertificateRequest request;
    request.schema_version = "quality-certificate-v1";
    request.certificate_id = certificate_id;
    request.input_sha256 = artifact.source_output_sha256;
    request.output_sha256 = artifact.output_sha256;
    request.toolchain_revision = "stage11-e2e-r1";
    request.sample_count = performance.sample_count + quality.sample_count;
    request.execution_units = performance.execution_units;
    request.metrics = {
        quality.metrics[0],
        performance.metrics[0],
        performance.metrics[1],
        performance.metrics[2],
        quality.metrics[1],
        performance.metrics[3],
    };
    return request;
}

[[nodiscard]] bool certify_format(ExportFormat format, const std::string& export_id, const std::string& certificate_id) {
    const HybridOutputManifest source = source_output();
    const ExportRequest request = export_request(format, export_id);
    const auto encoded = encode_canonical_export(request, source);
    if (!encoded.ok()) {
        std::cerr << "canonical export encoding failed for Stage 11 e2e fixture\n";
        return false;
    }

    const auto performance_a = measure_canonical_export_metrics(request, source, encoded.artifact);
    const auto performance_b = measure_canonical_export_metrics(request, source, encoded.artifact);
    const CanonicalQualityFixture fixture = quality_fixture();
    const auto quality_a = measure_canonical_quality_metrics(fixture);
    const auto quality_b = measure_canonical_quality_metrics(fixture);
    if (!performance_a.ok() || !performance_b.ok() || !quality_a.ok() || !quality_b.ok()) {
        std::cerr << "measured Stage 11 evidence rejected valid canonical fixture\n";
        return false;
    }
    if (performance_a.metrics.size() != performance_b.metrics.size() || quality_a.metrics.size() != quality_b.metrics.size()) {
        std::cerr << "Stage 11 measured evidence shape is not repeatable\n";
        return false;
    }
    for (std::size_t index = 0U; index < performance_a.metrics.size(); ++index) {
        if (performance_a.metrics[index].name != performance_b.metrics[index].name ||
            performance_a.metrics[index].measured != performance_b.metrics[index].measured) {
            std::cerr << "Stage 11 performance evidence is not repeatable\n";
            return false;
        }
    }
    for (std::size_t index = 0U; index < quality_a.metrics.size(); ++index) {
        if (quality_a.metrics[index].name != quality_b.metrics[index].name ||
            quality_a.metrics[index].measured != quality_b.metrics[index].measured) {
            std::cerr << "Stage 11 quality evidence is not repeatable\n";
            return false;
        }
    }

    const QualityCertificateRequest certificate_request =
        measured_request(certificate_id, encoded.artifact, performance_a, quality_a);
    const auto issued_a = issue_quality_certificate(certificate_request, request, source, encoded.artifact);
    const auto issued_b = issue_quality_certificate(certificate_request, request, source, encoded.artifact);
    if (!issued_a.ok() || !issued_b.ok()) {
        std::cerr << "measured Stage 11 evidence failed certificate issuance\n";
        return false;
    }
    if (issued_a.artifact.canonical_bytes != issued_b.artifact.canonical_bytes ||
        issued_a.artifact.certificate_sha256 != issued_b.artifact.certificate_sha256) {
        std::cerr << "Stage 11 end-to-end certificate is not deterministic\n";
        return false;
    }

    auto substituted = certificate_request;
    substituted.output_sha256 = std::string(64U, 'b');
    if (issue_quality_certificate(substituted, request, source, encoded.artifact).validation.error !=
        QualityCertificateError::OutputProvenanceMismatch) {
        std::cerr << "Stage 11 end-to-end provenance substitution was accepted\n";
        return false;
    }

    auto tampered = encoded.artifact;
    tampered.bytes.push_back('x');
    if (measure_canonical_export_metrics(request, source, tampered).error != QualityCertificateError::InvalidExportArtifact ||
        issue_quality_certificate(certificate_request, request, source, tampered).validation.error !=
            QualityCertificateError::InvalidExportArtifact) {
        std::cerr << "tampered Stage 10 artifact crossed Stage 11 certification boundary\n";
        return false;
    }

    auto bad_quality = fixture;
    bad_quality.candidate_alpha[1] = 255U;
    if (measure_canonical_quality_metrics(bad_quality).error != QualityCertificateError::ThresholdViolation) {
        std::cerr << "bad measured image quality crossed Stage 11 certification boundary\n";
        return false;
    }

    return true;
}

}  // namespace

int main() {
    if (!certify_format(ExportFormat::Svg, "stage11-e2e-svg", "stage11-cert-svg")) {
        return EXIT_FAILURE;
    }
    if (!certify_format(ExportFormat::Pdf, "stage11-e2e-pdf", "stage11-cert-pdf")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
