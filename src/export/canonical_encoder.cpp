#include "vektoryum/export/canonical_encoder.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::exporting {
namespace {

[[nodiscard]] std::string encode_bytes(const ExportRequest& request) {
    const std::string width = std::to_string(request.width);
    const std::string height = std::to_string(request.height);
    switch (request.format) {
        case ExportFormat::Svg:
            return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + width + "\" height=\"" + height + "\" viewBox=\"0 0 " + width + " " + height + "\">\n</svg>\n";
        case ExportFormat::Pdf:
            return "%PDF-1.7\n% Vektoryum canonical export\n% width=" + width + " height=" + height + "\n%%EOF\n";
        case ExportFormat::Eps:
            return "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 " + width + " " + height + "\n%%EOF\n";
        case ExportFormat::Dxf:
            return "0\nSECTION\n2\nHEADER\n9\n$EXTMAX\n10\n" + width + "\n20\n" + height + "\n0\nENDSEC\n0\nEOF\n";
    }
    return {};
}

}  // namespace

CanonicalEncodeResult encode_canonical_export(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& request_limits,
    const ExportExecutionLimits& execution_limits) {
    if (!validate_export_request(request, source_output, request_limits).ok()) {
        return {ExportArtifactError::InvalidRequest, {}};
    }

    const std::string encoded = encode_bytes(request);
    if (encoded.empty()) {
        return {ExportArtifactError::InvalidStructure, {}};
    }

    EncodedExportArtifact artifact;
    artifact.format = request.format;
    artifact.export_id = request.export_id;
    artifact.source_output_id = source_output.output_id;
    artifact.source_output_sha256 = source_output.output_sha256;
    artifact.bytes.assign(encoded.begin(), encoded.end());
    artifact.output_sha256 = ml::sha256_hex(artifact.bytes);
    artifact.peak_intermediate_bytes = static_cast<std::uint64_t>(artifact.bytes.size());
    artifact.execution_units = 1U + static_cast<std::uint64_t>(artifact.bytes.size() / 1024U);

    const auto validation = validate_encoded_export_artifact(
        request, source_output, artifact, request_limits, execution_limits);
    if (!validation.ok()) {
        return {validation.error, {}};
    }
    return {ExportArtifactError::None, std::move(artifact)};
}

}  // namespace vektoryum::exporting
