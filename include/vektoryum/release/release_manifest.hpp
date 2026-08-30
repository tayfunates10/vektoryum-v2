#pragma once

#include <string>
#include <string_view>

namespace vektoryum::release {

inline constexpr std::string_view release_manifest_schema = "vektoryum.release-manifest.v1";

enum class ReleaseManifestError {
    None = 0,
    UnsupportedSchema,
    UnsupportedProductVersion,
    UnsupportedCoreApiSchema,
    InvalidSourceRevision,
    UnsupportedChannel,
};

struct ReleaseManifest {
    std::string schema_version;
    std::string product_version;
    std::string core_api_schema;
    std::string source_revision;
    std::string channel;
};

struct ReleaseManifestValidation {
    ReleaseManifestError error{ReleaseManifestError::None};

    [[nodiscard]] bool ok() const noexcept { return error == ReleaseManifestError::None; }
};

[[nodiscard]] ReleaseManifest make_release_manifest(
    std::string source_revision,
    std::string channel);
[[nodiscard]] ReleaseManifestValidation validate_release_manifest(const ReleaseManifest& manifest) noexcept;
[[nodiscard]] std::string canonical_release_manifest(const ReleaseManifest& manifest);
[[nodiscard]] std::string release_manifest_sha256(const ReleaseManifest& manifest);
[[nodiscard]] std::string_view release_manifest_error_name(ReleaseManifestError error) noexcept;

}  // namespace vektoryum::release
