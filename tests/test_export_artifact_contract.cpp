#include "vektoryum/export/export_artifact_contract.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

using vektoryum::exporting::EncodedExportArtifact;
using vektoryum::exporting::ExportArtifactError;
using vektoryum::exporting::ExportExecutionLimits;
using vektoryum::exporting::ExportFormat;
using vektoryum::exporting::ExportRequest;
using vektoryum::exporting::canonical_export_artifact_report;
using vektoryum::exporting::validate_encoded_export_artifact;
using vektoryum::hybrid::HybridOutputManifest;

namespace {

[[nodiscard]] std::vector<std::uint8_t> bytes(const std::string& value) {
    return std::vector<std::uint8_t>(value.begin(), value.end());
}

[[nodiscard]] HybridOutputManifest source_output() {
    HybridOutputManifest output{};
    output.output_id = "hybrid-output-0001";
    output.output_sha256 = std::string(64U, 'a');
    return output;
}

[[nodiscard]] ExportRequest request(ExportFormat format) {
    return ExportRequest{
        "vektoryum.export.v1",
        "export-0001",
        format,
        "hybrid-output-0001",
        std::string(64U, 'a'),
        32U,
        32U,
        4096U,
    };
}

[[nodiscard]] std::vector<std::uint8_t> valid_payload(ExportFormat format) {
    switch (format) {
        case ExportFormat::Svg:
            return bytes("<svg xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M0 0L1 1\"/></svg>\n");
        case ExportFormat::Pdf:
            return bytes("%PDF-1.7\n1 0 obj\n<<>>\nendobj\n%%EOF\n");
        case ExportFormat::Eps:
            return bytes("%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 1 1\n%%EOF\n");
        case ExportFormat::Dxf:
            return bytes("0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n");
    }
    return {};
}

[[nodiscard]] EncodedExportArtifact artifact(ExportFormat format) {
    EncodedExportArtifact result{};
    result.format = format;
    result.export_id = "export-0001";
    result.source_output_id = "hybrid-output-0001";
    result.source_output_sha256 = std::string(64U, 'a');
    result.bytes = valid_payload(format);
    result.output_sha256 = vektoryum::ml::sha256_hex(result.bytes);
    result.peak_intermediate_bytes = 1024U;
    result.execution_units = 100U;
    return result;
}

}  // namespace

int main() {
    const HybridOutputManifest source = source_output();

    for (const ExportFormat format : {ExportFormat::Svg, ExportFormat::Pdf, ExportFormat::Eps, ExportFormat::Dxf}) {
        const ExportRequest baseline_request = request(format);
        const EncodedExportArtifact baseline = artifact(format);
        assert(validate_encoded_export_artifact(baseline_request, source, baseline).ok());
        assert(canonical_export_artifact_report(baseline) == canonical_export_artifact_report(baseline));
    }

    const ExportRequest svg_request = request(ExportFormat::Svg);
    const EncodedExportArtifact baseline = artifact(ExportFormat::Svg);

    EncodedExportArtifact invalid = baseline;
    invalid.format = ExportFormat::Pdf;
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::FormatMismatch);

    invalid = baseline;
    invalid.export_id = "other-export";
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::ExportIdentityMismatch);

    invalid = baseline;
    invalid.source_output_id = "other-output";
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::SourceOutputIdentityMismatch);

    invalid = baseline;
    invalid.source_output_sha256 = std::string(64U, 'b');
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::SourceOutputDigestMismatch);

    invalid = baseline;
    invalid.bytes.clear();
    invalid.output_sha256 = vektoryum::ml::sha256_hex(invalid.bytes);
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::EmptyOutput);

    invalid = baseline;
    invalid.output_sha256 = std::string(64U, 'b');
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::OutputDigestMismatch);

    invalid = baseline;
    invalid.output_sha256 = std::string(64U, 'A');
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::InvalidOutputDigest);

    invalid = baseline;
    invalid.bytes = bytes("<svg xmlns=\"http://www.w3.org/2000/svg\">unterminated");
    invalid.output_sha256 = vektoryum::ml::sha256_hex(invalid.bytes);
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::InvalidStructure);

    invalid = baseline;
    invalid.peak_intermediate_bytes = 0U;
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::ZeroIntermediateBytes);

    ExportExecutionLimits execution_limits{};
    execution_limits.max_intermediate_bytes = 512U;
    assert(validate_encoded_export_artifact(svg_request, source, baseline, {}, execution_limits).error == ExportArtifactError::IntermediateBudgetExceeded);

    invalid = baseline;
    invalid.execution_units = 0U;
    assert(validate_encoded_export_artifact(svg_request, source, invalid).error == ExportArtifactError::ZeroExecutionUnits);

    execution_limits = ExportExecutionLimits{};
    execution_limits.max_execution_units = 99U;
    assert(validate_encoded_export_artifact(svg_request, source, baseline, {}, execution_limits).error == ExportArtifactError::ExecutionBudgetExceeded);

    ExportRequest tight_request = svg_request;
    tight_request.estimated_output_bytes = static_cast<std::uint64_t>(baseline.bytes.size() - 1U);
    assert(validate_encoded_export_artifact(tight_request, source, baseline).error == ExportArtifactError::OutputBudgetExceeded);

    return 0;
}
