#include "vektoryum/release/release_manifest.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "vektoryum/api/stable_api.hpp"
#include "vektoryum/ml/artifact_digest.hpp"
#include "vektoryum/version.hpp"

namespace vektoryum::release {
namespace {

[[nodiscard]] bool is_lower_hex_revision(std::string_view value) noexcept {
    if (value.size() != 40U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char ch) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        return std::isdigit(byte) != 0 || (ch >= 'a' && ch <= 'f');
    });
}

[[nodiscard]] bool is_supported_channel(std::string_view value) noexcept {
    return value == "candidate" || value == "stable";
}

}  // namespace

ReleaseManifest make_release_manifest(std::string source_revision, std::string channel) {
    return ReleaseManifest{
        std::string(release_manifest_schema),
        std::string(vektoryum::version_string()),
        std::string(api::stable_schema_version),
        std::move(source_revision),
        std::move(channel),
    };
}

ReleaseManifestValidation validate_release_manifest(const ReleaseManifest& manifest) noexcept {
    if (manifest.schema_version != release_manifest_schema) {
        return {ReleaseManifestError::UnsupportedSchema};
    }
    if (manifest.product_version != vektoryum::version_string()) {
        return {ReleaseManifestError::UnsupportedProductVersion};
    }
    if (manifest.core_api_schema != api::stable_schema_version) {
        return {ReleaseManifestError::UnsupportedCoreApiSchema};
    }
    if (!is_lower_hex_revision(manifest.source_revision)) {
        return {ReleaseManifestError::InvalidSourceRevision};
    }
    if (!is_supported_channel(manifest.channel)) {
        return {ReleaseManifestError::UnsupportedChannel};
    }
    return {};
}

std::string canonical_release_manifest(const ReleaseManifest& manifest) {
    std::string output;
    output.reserve(256U);
    output += "schema=";
    output += manifest.schema_version;
    output += '\n';
    output += "product_version=";
    output += manifest.product_version;
    output += '\n';
    output += "core_api_schema=";
    output += manifest.core_api_schema;
    output += '\n';
    output += "source_revision=";
    output += manifest.source_revision;
    output += '\n';
    output += "channel=";
    output += manifest.channel;
    output += '\n';
    return output;
}

std::string release_manifest_sha256(const ReleaseManifest& manifest) {
    const std::string canonical = canonical_release_manifest(manifest);
    const std::vector<std::uint8_t> bytes(canonical.begin(), canonical.end());
    return ml::sha256_hex(bytes);
}

std::string_view release_manifest_error_name(ReleaseManifestError error) noexcept {
    switch (error) {
        case ReleaseManifestError::None:
            return "none";
        case ReleaseManifestError::UnsupportedSchema:
            return "unsupported_schema";
        case ReleaseManifestError::UnsupportedProductVersion:
            return "unsupported_product_version";
        case ReleaseManifestError::UnsupportedCoreApiSchema:
            return "unsupported_core_api_schema";
        case ReleaseManifestError::InvalidSourceRevision:
            return "invalid_source_revision";
        case ReleaseManifestError::UnsupportedChannel:
            return "unsupported_channel";
    }
    return "unknown";
}

}  // namespace vektoryum::release
