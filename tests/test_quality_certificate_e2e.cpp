#include "vektoryum/certification/quality_certificate.hpp"
#include "vektoryum/certification/final_output_evidence.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "vektoryum/export/canonical_encoder.hpp"
#include "vektoryum/ml/artifact_digest.hpp"

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

[[nodiscard]] CanonicalQualityFixture exact_threshold_quality_fixture() {
    CanonicalQualityFixture fixture;
    fixture.reference_alpha.assign(100U, 0U);
    fixture.candidate_alpha.assign(100U, 5U);
    for (std::size_t index = 0U; index < 10U; ++index) {
        fixture.candidate_alpha[index] = 6U;
    }
    fixture.reference_vector_mask.assign(100U, 1U);
    fixture.candidate_vector_mask.assign(100U, 1U);
    fixture.candidate_vector_mask.back() = 0U;
    return fixture;
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

[[nodiscard]] bool verify_exact_threshold_repeatability() {
    const CanonicalQualityFixture fixture = exact_threshold_quality_fixture();
    const auto measured_a = measure_canonical_quality_metrics(fixture);
    const auto measured_b = measure_canonical_quality_metrics(fixture);
    if (!measured_a.ok() || !measured_b.ok() || measured_a.metrics.size() != 2U || measured_b.metrics.size() != 2U) {
        std::cerr << "exact-threshold Stage 11 quality fixture was rejected\n";
        return false;
    }
    if (measured_a.metrics[0].name != "alpha_mae" || measured_a.metrics[0].measured != 0.02 ||
        measured_a.metrics[1].name != "vector_iou" || measured_a.metrics[1].measured != 0.99) {
        std::cerr << "exact-threshold Stage 11 quality evidence drifted\n";
        return false;
    }
    for (std::size_t index = 0U; index < measured_a.metrics.size(); ++index) {
        if (measured_a.metrics[index].name != measured_b.metrics[index].name ||
            measured_a.metrics[index].measured != measured_b.metrics[index].measured ||
            measured_a.metrics[index].minimum != measured_b.metrics[index].minimum ||
            measured_a.metrics[index].maximum != measured_b.metrics[index].maximum) {
            std::cerr << "exact-threshold Stage 11 quality evidence is not repeatable\n";
            return false;
        }
    }

    auto alpha_over = fixture;
    ++alpha_over.candidate_alpha[10];
    if (measure_canonical_quality_metrics(alpha_over).error != QualityCertificateError::ThresholdViolation) {
        std::cerr << "just-over-threshold alpha fixture was accepted\n";
        return false;
    }

    auto iou_under = fixture;
    iou_under.candidate_vector_mask[98] = 0U;
    if (measure_canonical_quality_metrics(iou_under).error != QualityCertificateError::ThresholdViolation) {
        std::cerr << "just-under-threshold vector IoU fixture was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool verify_final_serialized_svg_evidence() {
    const std::string svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\" viewBox=\"0 0 4 4\">"
        "<path d=\"M1 1 L3 1 L3 3 L1 3 Z\" fill=\"#ff0000\" fill-rule=\"evenodd\"/>"
        "</svg>";
    const std::vector<std::uint8_t> svg_bytes(svg.begin(), svg.end());

    std::vector<std::uint8_t> reference_rgba(4U * 4U * 4U, 0U);
    std::vector<std::uint8_t> reference_alpha(16U, 0U);
    std::vector<std::uint8_t> reference_mask(16U, 0U);
    for (std::size_t y = 1U; y < 3U; ++y) {
        for (std::size_t x = 1U; x < 3U; ++x) {
            const std::size_t pixel = y * 4U + x;
            const std::size_t base = pixel * 4U;
            reference_rgba[base] = 255U;
            reference_rgba[base + 3U] = 255U;
            reference_alpha[pixel] = 255U;
            reference_mask[pixel] = 1U;
        }
    }

    const auto evidence = vektoryum::certification::measure_final_serialized_svg_evidence(
        svg_bytes,
        reference_rgba,
        reference_alpha,
        reference_mask,
        4U,
        4U);
    if (!evidence.valid || !evidence.canonical_quality.ok() ||
        evidence.output_sha256 != vektoryum::ml::sha256_hex(svg_bytes) ||
        evidence.color_mae != 0.0 ||
        evidence.reference_components != 1U || evidence.candidate_components != 1U ||
        evidence.reference_holes != 0U || evidence.candidate_holes != 0U ||
        evidence.boundary_p95_pixels != 0.0 ||
        evidence.visible_residual_ratio != 0.0) {
        std::cerr << "final serialized SVG evidence did not reproduce exact artifact bytes\n";
        return false;
    }

    const std::string recolored_svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\" viewBox=\"0 0 4 4\">"
        "<path d=\"M1 1 L3 1 L3 3 L1 3 Z\" fill=\"#0000ff\" fill-rule=\"evenodd\"/>"
        "</svg>";
    const std::vector<std::uint8_t> recolored_bytes(recolored_svg.begin(), recolored_svg.end());
    const auto recolored = vektoryum::certification::measure_final_serialized_svg_evidence(
        recolored_bytes,
        reference_rgba,
        reference_alpha,
        reference_mask,
        4U,
        4U);
    if (!recolored.valid || !recolored.canonical_quality.ok() ||
        recolored.output_sha256 == evidence.output_sha256 ||
        recolored.color_mae <= 0.0 ||
        recolored.visible_residual_ratio <= 0.0) {
        std::cerr << "final serialized SVG color tamper was not reflected in artifact-bound evidence\n";
        return false;
    }

    auto malformed = svg_bytes;
    malformed.pop_back();
    if (vektoryum::certification::measure_final_serialized_svg_evidence(
            malformed,
            reference_rgba,
            reference_alpha,
            reference_mask,
            4U,
            4U).valid) {
        std::cerr << "malformed final serialized SVG produced valid certification evidence\n";
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
    if (!certify_format(ExportFormat::Eps, "stage11-e2e-eps", "stage11-cert-eps")) {
        return EXIT_FAILURE;
    }
    if (!certify_format(ExportFormat::Dxf, "stage11-e2e-dxf", "stage11-cert-dxf")) {
        return EXIT_FAILURE;
    }
    if (!verify_exact_threshold_repeatability()) {
        return EXIT_FAILURE;
    }
    if (!verify_final_serialized_svg_evidence()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
