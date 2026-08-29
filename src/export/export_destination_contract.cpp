#include "vektoryum/export/export_destination_contract.hpp"

#include <algorithm>
#include <string_view>
#include <unordered_set>

namespace vektoryum::exporting {
namespace {

[[nodiscard]] bool has_unsafe_text(std::string_view text) noexcept {
    return text.find('\n') != std::string_view::npos || text.find('\r') != std::string_view::npos ||
           text.find('\0') != std::string_view::npos;
}

[[nodiscard]] bool is_reserved_metadata_key(std::string_view key) noexcept {
    return key == "schema_version" || key == "export_id" || key == "source_output_id" ||
           key == "source_output_sha256" || key == "output_sha256" || key == "format";
}

[[nodiscard]] bool unsafe_relative_path(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/' || path.front() == '\\') {
        return true;
    }
    if (path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos ||
        path.find('\0') != std::string_view::npos || path.find('\n') != std::string_view::npos ||
        path.find('\r') != std::string_view::npos) {
        return true;
    }
    std::size_t begin = 0U;
    while (begin <= path.size()) {
        const std::size_t end = path.find('/', begin);
        const std::size_t count = end == std::string_view::npos ? path.size() - begin : end - begin;
        const std::string_view component = path.substr(begin, count);
        if (component.empty() || component == "." || component == "..") {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return false;
}

}  // namespace

ExportDestinationValidation validate_export_destination(
    const ExportRequest& request,
    const EncodedExportArtifact& artifact,
    const ExportDestination& destination,
    const ExportDestinationLimits& limits) {
    if (destination.relative_path.empty()) {
        return {ExportDestinationError::MissingRelativePath};
    }
    if (destination.relative_path.size() > limits.max_path_bytes) {
        return {ExportDestinationError::PathTooLong};
    }
    if (unsafe_relative_path(destination.relative_path)) {
        return {ExportDestinationError::UnsafePath};
    }
    if (artifact.export_id != request.export_id || artifact.source_output_id != request.source_output_id) {
        return {ExportDestinationError::ArtifactIdentityMismatch};
    }
    if (artifact.source_output_sha256 != request.source_output_sha256) {
        return {ExportDestinationError::ArtifactDigestMismatch};
    }
    if (destination.metadata.size() > limits.max_metadata_entries) {
        return {ExportDestinationError::TooManyMetadataEntries};
    }

    std::unordered_set<std::string> keys;
    for (const ExportMetadataEntry& entry : destination.metadata) {
        if (entry.key.empty()) {
            return {ExportDestinationError::EmptyMetadataKey};
        }
        if (entry.key.size() > limits.max_metadata_key_bytes) {
            return {ExportDestinationError::MetadataKeyTooLong};
        }
        if (entry.value.size() > limits.max_metadata_value_bytes) {
            return {ExportDestinationError::MetadataValueTooLong};
        }
        if (has_unsafe_text(entry.key) || has_unsafe_text(entry.value)) {
            return {ExportDestinationError::UnsafeMetadataText};
        }
        if (is_reserved_metadata_key(entry.key)) {
            return {ExportDestinationError::ReservedMetadataKey};
        }
        if (!keys.insert(entry.key).second) {
            return {ExportDestinationError::DuplicateMetadataKey};
        }
    }
    return {};
}

std::string canonical_export_destination_report(const ExportDestination& destination) {
    std::string report;
    report += "path_bytes=" + std::to_string(destination.relative_path.size()) + "\n";
    report += "path=" + destination.relative_path + "\n";
    report += "metadata_count=" + std::to_string(destination.metadata.size()) + "\n";
    for (std::size_t index = 0U; index < destination.metadata.size(); ++index) {
        const ExportMetadataEntry& entry = destination.metadata[index];
        report += "metadata[" + std::to_string(index) + "].key_bytes=" + std::to_string(entry.key.size()) + "\n";
        report += "metadata[" + std::to_string(index) + "].key=" + entry.key + "\n";
        report += "metadata[" + std::to_string(index) + "].value_bytes=" + std::to_string(entry.value.size()) + "\n";
        report += "metadata[" + std::to_string(index) + "].value=" + entry.value + "\n";
    }
    return report;
}

}  // namespace vektoryum::exporting
