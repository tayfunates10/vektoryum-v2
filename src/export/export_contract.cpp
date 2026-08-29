#include "vektoryum/export/export_contract.hpp"

#include <limits>
#include <sstream>

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

[[nodiscard]] bool safe_report_field(const std::string& value) noexcept {
    return value.find('\n') == std::string::npos && value.find('\r') == std::string::npos;
}

[[nodiscard]] bool supported_format(ExportFormat format) noexcept {
    switch (format) {
        case ExportFormat::Svg:
        case ExportFormat::Pdf:
        case ExportFormat::Eps:
        case ExportFormat::Dxf:
            return true;
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

ExportRequestValidation validate_export_request(
    const ExportRequest& request,
    const hybrid::HybridOutputManifest& source_output,
    const ExportLimits& limits) {
    if (request.schema_version.empty()) {
        return {ExportRequestError::MissingSchemaVersion, 0U};
    }
    if (request.export_id.empty()) {
        return {ExportRequestError::MissingExportIdentity, 0U};
    }
    if (!safe_report_field(request.schema_version) || !safe_report_field(request.export_id) ||
        !safe_report_field(request.source_output_id)) {
        return {ExportRequestError::UnsafeTextField, 0U};
    }
    if (!supported_format(request.format)) {
        return {ExportRequestError::UnsupportedFormat, 0U};
    }
    if (request.source_output_id.empty()) {
        return {ExportRequestError::MissingSourceOutputIdentity, 0U};
    }
    if (!is_lower_hex_sha256(request.source_output_sha256)) {
        return {ExportRequestError::InvalidSourceOutputDigest, 0U};
    }
    if (request.source_output_id != source_output.output_id) {
        return {ExportRequestError::SourceOutputIdentityMismatch, 0U};
    }
    if (request.source_output_sha256 != source_output.output_sha256) {
        return {ExportRequestError::SourceOutputDigestMismatch, 0U};
    }
    if (request.width == 0U || request.height == 0U) {
        return {ExportRequestError::ZeroDimension, 0U};
    }

    const std::uint64_t width = request.width;
    const std::uint64_t height = request.height;
    if (width > std::numeric_limits<std::uint64_t>::max() / height) {
        return {ExportRequestError::PixelBudgetExceeded, 0U};
    }
    const std::uint64_t pixels = width * height;
    if (pixels > limits.max_pixels) {
        return {ExportRequestError::PixelBudgetExceeded, pixels};
    }
    if (request.estimated_output_bytes == 0U) {
        return {ExportRequestError::ZeroEstimatedOutputBytes, pixels};
    }
    if (request.estimated_output_bytes > limits.max_output_bytes) {
        return {ExportRequestError::OutputBudgetExceeded, pixels};
    }
    return {ExportRequestError::None, pixels};
}

std::string canonical_export_request_report(const ExportRequest& request) {
    std::ostringstream stream;
    stream << "schema_version=" << request.schema_version << '\n'
           << "export_id=" << request.export_id << '\n'
           << "format=" << format_name(request.format) << '\n'
           << "source_output_id=" << request.source_output_id << '\n'
           << "source_output_sha256=" << request.source_output_sha256 << '\n'
           << "width=" << request.width << '\n'
           << "height=" << request.height << '\n'
           << "estimated_output_bytes=" << request.estimated_output_bytes << '\n';
    return stream.str();
}

}  // namespace vektoryum::exporting
