#include "vektoryum/release/package_identity.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::release {

PackageIdentity make_package_identity(const ReleaseManifest& manifest, const PackageInventory& inventory) {
    return {
        std::string(package_identity_schema),
        release_manifest_sha256(manifest),
        package_inventory_sha256(inventory),
    };
}

PackageIdentityValidation validate_package_identity(const PackageIdentity& identity, const ReleaseManifest& manifest, const PackageInventory& inventory) noexcept {
    if (identity.schema_version != package_identity_schema) {
        return {PackageIdentityError::UnsupportedSchema};
    }
    if (!validate_release_manifest(manifest).ok()) {
        return {PackageIdentityError::InvalidReleaseManifest};
    }
    if (!validate_package_inventory(inventory).ok()) {
        return {PackageIdentityError::InvalidPackageInventory};
    }
    const std::string manifest_digest = release_manifest_sha256(manifest);
    if (inventory.release_manifest_sha256 != manifest_digest || identity.release_manifest_sha256 != manifest_digest) {
        return {PackageIdentityError::ReleaseManifestMismatch};
    }
    if (identity.package_inventory_sha256 != package_inventory_sha256(inventory)) {
        return {PackageIdentityError::PackageInventoryMismatch};
    }
    return {};
}

std::string canonical_package_identity(const PackageIdentity& identity) {
    std::string output;
    output += "schema=";
    output += identity.schema_version;
    output += '\n';
    output += "release_manifest_sha256=";
    output += identity.release_manifest_sha256;
    output += '\n';
    output += "package_inventory_sha256=";
    output += identity.package_inventory_sha256;
    output += '\n';
    return output;
}

std::string package_identity_sha256(const PackageIdentity& identity) {
    const std::string canonical = canonical_package_identity(identity);
    const std::vector<std::uint8_t> bytes(canonical.begin(), canonical.end());
    return ml::sha256_hex(bytes);
}

std::string_view package_identity_error_name(PackageIdentityError error) noexcept {
    switch (error) {
        case PackageIdentityError::None: return "none";
        case PackageIdentityError::UnsupportedSchema: return "unsupported_schema";
        case PackageIdentityError::InvalidReleaseManifest: return "invalid_release_manifest";
        case PackageIdentityError::InvalidPackageInventory: return "invalid_package_inventory";
        case PackageIdentityError::ReleaseManifestMismatch: return "release_manifest_mismatch";
        case PackageIdentityError::PackageInventoryMismatch: return "package_inventory_mismatch";
    }
    return "unknown";
}

}  // namespace vektoryum::release
