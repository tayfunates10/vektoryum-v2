#include "vektoryum/export/export_artifact_contract.hpp"

#include <algorithm>
#include <limits>
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

[[nodiscard]] std::string text_of(const std::vector<std::uint8_t>& bytes) {
    return {bytes.begin(), bytes.end()};
}

[[nodiscard]] bool parse_decimal_at(const std::string& text, std::size_t begin, std::size_t& value) {
    if (begin >= text.size() || text[begin] < '0' || text[begin] > '9') {
        return false;
    }
    std::size_t parsed = 0U;
    std::size_t cursor = begin;
    while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
        const std::size_t digit = static_cast<std::size_t>(text[cursor] - '0');
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
        ++cursor;
    }
    if (cursor >= text.size() || text[cursor] != '\n') {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool geometry_svg_valid(const std::string& text) {
    const std::size_t path = text.find("<path ");
    if (path == std::string::npos) {
        return true;
    }
    return text.find("viewBox=\"", 0U) != std::string::npos &&
           text.find("d=\"", path) != std::string::npos &&
           text.find("fill-rule=\"evenodd\"", path) != std::string::npos &&
           text.find("/>\n", path) != std::string::npos;
}

[[nodiscard]] bool geometry_pdf_valid(const std::string& text) {
    if (text.find("% Vektoryum geometry export\n") == std::string::npos) {
        return true;
    }
    const std::size_t object = text.find("1 0 obj\n");
    const std::size_t stream = text.find("stream\n");
    const std::size_t endstream = text.find("endstream\n");
    const std::size_t xref = text.find("xref\n");
    const std::size_t trailer = text.find("trailer\n");
    const std::string startxref_token = "startxref\n";
    const std::size_t startxref = text.find(startxref_token);
    if (object == std::string::npos || stream == std::string::npos || endstream == std::string::npos ||
        xref == std::string::npos || trailer == std::string::npos || startxref == std::string::npos ||
        !(object < stream && stream < endstream && endstream < xref && xref < trailer && trailer < startxref)) {
        return false;
    }
    std::size_t declared_xref = 0U;
    return parse_decimal_at(text, startxref + startxref_token.size(), declared_xref) && declared_xref == xref;
}

[[nodiscard]] bool geometry_eps_valid(const std::string& text) {
    if (text.find("newpath\n") == std::string::npos && text.find("curveto\n") == std::string::npos) {
        return true;
    }
    const std::size_t bounding_box = text.find("%%BoundingBox: ");
    const std::size_t end_comments = text.find("%%EndComments\n");
    const std::size_t newpath = text.find("newpath\n");
    const std::size_t fill = text.find("eofill\n");
    const std::size_t showpage = text.find("showpage\n");
    return bounding_box != std::string::npos && end_comments != std::string::npos &&
           newpath != std::string::npos && fill != std::string::npos && showpage != std::string::npos &&
           bounding_box < end_comments && end_comments < newpath && newpath < fill && fill < showpage;
}

[[nodiscard]] bool geometry_dxf_valid(const std::string& text) {
    const bool has_geometry = text.find("0\nLINE\n") != std::string::npos ||
                              text.find("0\nSPLINE\n") != std::string::npos;
    if (!has_geometry) {
        return true;
    }
    const std::size_t header = text.find("2\nHEADER\n");
    const std::size_t header_end = text.find("0\nENDSEC\n", header);
    const std::size_t entities = text.find("2\nENTITIES\n", header_end);
    const std::size_t entities_end = text.find("0\nENDSEC\n", entities);
    return header != std::string::npos && header_end != std::string::npos &&
           entities != std::string::npos && entities_end != std::string::npos &&
           header < header_end && header_end < entities && entities < entities_end;
}

[[nodiscard]] bool structurally_valid(ExportFormat format, const std::vector<std::uint8_t>& bytes) {
    const std::string text = text_of(bytes);
    switch (format) {
        case ExportFormat::Svg:
            return starts_with(bytes, "<svg xmlns=\"http://www.w3.org/2000/svg\"") &&
                   ends_with(bytes, "</svg>\n") && geometry_svg_valid(text);
        case ExportFormat::Pdf:
            return starts_with(bytes, "%PDF-1.7\n") && ends_with(bytes, "%%EOF\n") && geometry_pdf_valid(text);
        case ExportFormat::Eps:
            return starts_with(bytes, "%!PS-Adobe-3.0 EPSF-3.0\n") &&
                   ends_with(bytes, "%%EOF\n") && geometry_eps_valid(text);
        case ExportFormat::Dxf:
            return starts_with(bytes, "0\nSECTION\n") && ends_with(bytes, "0\nEOF\n") && geometry_dxf_valid(text);
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
