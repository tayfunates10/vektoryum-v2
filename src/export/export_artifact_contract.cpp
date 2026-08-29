#include "vektoryum/export/export_artifact_contract.hpp"

#include <algorithm>
#include <sstream>
#include <string>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::exporting {

namespace {

[[nodiscard]] bool is_lower_hex_sha256(const std::string& value) {
    if (value.size() != 64U) {
        return false;
    }
    for (const char character : value) {
        const auto c = static_cast<unsigned char>(character);
        const bool digit = c >= static_cast<unsigned char>('0') && c <= static_cast<unsigned char>('9');
        const bool lower_hex = c >= static_cast<unsigned char>('a') && c <= static_cast<unsigned char>('f');
        if (!digit && !lower_hex) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool starts_with(const std::vector<std::uint8_t>& bytes, const std::string& prefix) {
    if (bytes.size() < prefix.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), bytes.begin(), [](char lhs, std::uint8_t rhs) {
        return static_cast<unsigned char>(lhs) == rhs;
    });
}

[[nodiscard]] bool ends_with(const std::vector<std::uint8_t>& bytes, const std::string& suffix) {
    if (bytes.size() < suffix.size()) {
        return false;
    }
    const auto offset = bytes.end() - static_cast<std::ptrdiff_t>(suffix.size());
    return std::equal(suffix.begin(), suffix.end(), offset, [](char lhs, std::uint8_t rhs) {
        return static_cast<unsigned char>(lhs) == rhs;
    });
}

[[nodiscard]] bool structurally_valid(ExportFormat format, const std::vector<std::uint8_t>& bytes) {
    switch (format) {
        case ExportFormat::Svg:
            return starts_with(bytes, "<svg xmlns=\"http://www.w3.org/2000/svg\"") && ends_with(bytes, "</svg>\n");
        case ExportFormat::Pdf:
            return starts_with(bytes, "%PDF-1.7\n") && ends_with(bytes, "%%EOF\n");
        case ExportFormat::Eps:
            return starts_with(bytes, "%!PS-Adobe-3.0 EPSF-3.0\n") && ends_with(bytes, "%%EOF\n");
        case ExportFormat::Dxf:
            return starts_with(bytes, "0\nSECTION\n") && ends_with(bytes, "0\nEOF\n");
    }
    return false;
}

[[nodiscard]] const char* format_name(ExportFormat format) noexcept {
    switch (format) {
        case ExportFormat::Svg: return "svg";
        case ExportFormat::Pdf: return "pdf";
        case ExportFormat::Eps: return "eps";
        case ExportFormat::Dxf: return "dxf";
    }
    return "unsupported";
}

}  // namespace

ExportArtifactValidation validate_encoded_export_artifact(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const EncodedExportArtifact& artifact,
    const ExportLimits& request_limits,
    const ExportExecutionLimits& execution_limits) {
    if (!validate_export_request(request, source_output, request_limits).ok()) {
        return {ExportArtifactError::InvalidRequest};
    }
    if (artifact.format != request.format) {
        return {ExportArtifactError::FormatMismatch};
    }
    if (artifact.export_id != request.export_id) {
        return {ExportArtifactError::ExportIdentityMismatch};
    }
    if (artifact.source_output_id != request.source_output_id) {
        return {ExportArtifactError::SourceOutputIdentityMismatch};
    }
    if (artifact.source_output_sha256 != request.source_output_sha256) {
        return {ExportArtifactError::SourceOutputDigestMismatch};
    }
    if (artifact.bytes.empty()) {
        return {ExportArtifactError::EmptyOutput};
    }
    if (artifact.bytes.size() > request.estimated_output_bytes || artifact.bytes.size() > request_limits.max_output_bytes) {
        return {ExportArtifactError::OutputBudgetExceeded};
    }
    if (!is_lower_hex_sha256(artifact.output_sha256)) {
        return {ExportArtifactError::InvalidOutputDigest};
    }
    if (artifact.output_sha256 != ml::sha256_hex(artifact.bytes)) {
        return {ExportArtifactError::OutputDigestMismatch};
    }
    if (artifact.peak_intermediate_bytes == 0U) {
        return {ExportArtifactError::ZeroIntermediateBytes};
    }
    if (artifact.peak_intermediate_bytes > execution_limits.max_intermediate_bytes) {
        return {ExportArtifactError::IntermediateBudgetExceeded};
    }
    if (artifact.execution_units == 0U) {
        return {ExportArtifactError::ZeroExecutionUnits};
    }
    if (artifact.execution_units > execution_limits.max_execution_units) {
        return {ExportArtifactError::ExecutionBudgetExceeded};
    }
    if (!structurally_valid(artifact.format, artifact.bytes)) {
        return {ExportArtifactError::InvalidStructure};
    }
    return {ExportArtifactError::None};
}

std::string canonical_export_artifact_report(const EncodedExportArtifact& artifact) {
    std::ostringstream stream;
    stream << "export_id=" << artifact.export_id << '\n'
           << "format=" << format_name(artifact.format) << '\n'
           << "source_output_id=" << artifact.source_output_id << '\n'
           << "source_output_sha256=" << artifact.source_output_sha256 << '\n'
           << "output_sha256=" << artifact.output_sha256 << '\n'
           << "output_bytes=" << artifact.bytes.size() << '\n'
           << "peak_intermediate_bytes=" << artifact.peak_intermediate_bytes << '\n'
           << "execution_units=" << artifact.execution_units << '\n';
    return stream.str();
}

}  // namespace vektoryum::exporting
