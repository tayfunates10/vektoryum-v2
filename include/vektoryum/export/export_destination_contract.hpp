#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/export/export_artifact_contract.hpp"

namespace vektoryum::exporting {

struct ExportMetadataEntry {
    std::string key;
    std::string value;
};

struct ExportDestination {
    std::string relative_path;
    std::vector<ExportMetadataEntry> metadata;
};

struct ExportDestinationLimits {
    std::size_t max_path_bytes{240U};
    std::size_t max_metadata_entries{32U};
    std::size_t max_metadata_key_bytes{64U};
    std::size_t max_metadata_value_bytes{1024U};
};

enum class ExportDestinationError : std::uint8_t {
    None,
    MissingRelativePath,
    UnsafePath,
    PathTooLong,
    TooManyMetadataEntries,
    EmptyMetadataKey,
    MetadataKeyTooLong,
    MetadataValueTooLong,
    UnsafeMetadataText,
    ReservedMetadataKey,
    DuplicateMetadataKey,
    ArtifactIdentityMismatch,
    ArtifactDigestMismatch,
};

struct ExportDestinationValidation {
    ExportDestinationError error{ExportDestinationError::None};

    [[nodiscard]] bool ok() const noexcept { return error == ExportDestinationError::None; }
};

[[nodiscard]] ExportDestinationValidation validate_export_destination(
    const ExportRequest& request,
    const EncodedExportArtifact& artifact,
    const ExportDestination& destination,
    const ExportDestinationLimits& limits = {});

[[nodiscard]] std::string canonical_export_destination_report(const ExportDestination& destination);

}  // namespace vektoryum::exporting
