#include <cstdlib>
#include <iostream>
#include <string>

#include "vektoryum/release/package_identity.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

vektoryum::release::ReleaseManifest manifest() {
    return vektoryum::release::make_release_manifest("0123456789abcdef0123456789abcdef01234567", "candidate");
}

vektoryum::release::PackageInventory inventory(const vektoryum::release::ReleaseManifest& release_manifest) {
    return {
        std::string(vektoryum::release::package_inventory_schema),
        vektoryum::release::release_manifest_sha256(release_manifest),
        {
            {"bin/vektoryum_cli", 1234U, std::string(64U, 'a')},
            {"release/release-manifest.txt", 200U, std::string(64U, 'b')},
            {"release/package-inventory.txt", 300U, std::string(64U, 'c')},
        },
    };
}

}  // namespace

int main() {
    using namespace vektoryum::release;
    const auto release_manifest = manifest();
    const auto package_inventory = inventory(release_manifest);
    const auto first = make_package_identity(release_manifest, package_inventory);
    const auto second = make_package_identity(release_manifest, package_inventory);

    require(validate_package_identity(first, release_manifest, package_inventory).ok(), "valid package identity must pass");
    require(canonical_package_identity(first) == canonical_package_identity(second), "identical inputs must produce byte-identical package identity");
    require(package_identity_sha256(first) == package_identity_sha256(second), "identical inputs must produce identical package digest");

    auto reordered = package_inventory;
    std::swap(reordered.entries[0], reordered.entries[2]);
    const auto reordered_identity = make_package_identity(release_manifest, reordered);
    require(canonical_package_identity(first) == canonical_package_identity(reordered_identity), "enumeration order must not alter package identity");

    auto substituted_manifest = release_manifest;
    substituted_manifest.channel = "stable";
    require(validate_package_identity(first, substituted_manifest, package_inventory).error == PackageIdentityError::ReleaseManifestMismatch, "manifest substitution must fail closed");

    auto substituted_inventory = package_inventory;
    substituted_inventory.entries[0].sha256 = std::string(64U, 'd');
    require(validate_package_identity(first, release_manifest, substituted_inventory).error == PackageIdentityError::PackageInventoryMismatch, "inventory substitution must fail closed");

    auto forged_identity = first;
    forged_identity.package_inventory_sha256 = std::string(64U, 'e');
    require(validate_package_identity(forged_identity, release_manifest, package_inventory).error == PackageIdentityError::PackageInventoryMismatch, "forged package digest must fail closed");

    auto foreign_schema = first;
    foreign_schema.schema_version = "vektoryum.package-identity.v2";
    require(validate_package_identity(foreign_schema, release_manifest, package_inventory).error == PackageIdentityError::UnsupportedSchema, "foreign identity schema must fail closed");

    return 0;
}
