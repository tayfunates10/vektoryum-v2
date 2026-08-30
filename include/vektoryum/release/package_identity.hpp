#pragma once

#include <string>
#include <string_view>

#include "vektoryum/release/package_inventory.hpp"
#include "vektoryum/release/release_manifest.hpp"

namespace vektoryum::release {

inline constexpr std::string_view package_identity_schema = "vektoryum.package-identity.v1";

struct PackageIdentity {
    std::string schema_version;
    std::string release_manifest_sha256;
    std::string package_inventory_sha256;
};

enum class PackageIdentityError {
    None = 0,
    UnsupportedSchema,
    InvalidReleaseManifest,
    InvalidPackageInventory,
    ReleaseManifestMismatch,
    PackageInventoryMismatch,
};

struct PackageIdentityValidation {
    PackageIdentityError error{PackageIdentityError::None};
    [[nodiscard]] bool ok() const noexcept { return error == PackageIdentityError::None; }
};

[[nodiscard]] PackageIdentity make_package_identity(const ReleaseManifest& manifest, const PackageInventory& inventory);
[[nodiscard]] PackageIdentityValidation validate_package_identity(const PackageIdentity& identity, const ReleaseManifest& manifest, const PackageInventory& inventory) noexcept;
[[nodiscard]] std::string canonical_package_identity(const PackageIdentity& identity);
[[nodiscard]] std::string package_identity_sha256(const PackageIdentity& identity);
[[nodiscard]] std::string_view package_identity_error_name(PackageIdentityError error) noexcept;

}  // namespace vektoryum::release
