#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

#include "vektoryum/release/package_contents.hpp"
#include "vektoryum/release/package_inventory.hpp"
#include "vektoryum/release/release_manifest.hpp"

namespace {

vektoryum::release::ReleaseManifest make_manifest() {
    return vektoryum::release::make_release_manifest(std::string(40U, 'a'), "candidate");
}

vektoryum::release::PackageInventory make_inventory(const vektoryum::release::ReleaseManifest& manifest) {
    return vektoryum::release::PackageInventory{
        std::string(vektoryum::release::package_inventory_schema),
        vektoryum::release::release_manifest_sha256(manifest),
        {
            {"bin/vektoryum_cli", 4096U, std::string(64U, 'b')},
            {"release/release-manifest.txt", 256U, std::string(64U, 'c')},
            {"release/package-inventory.txt", 512U, std::string(64U, 'd')},
        },
    };
}

std::vector<vektoryum::release::ObservedPackageEntry> make_observed(
    const vektoryum::release::PackageInventory& inventory) {
    std::vector<vektoryum::release::ObservedPackageEntry> observed;
    for (const auto& entry : inventory.entries) {
        observed.push_back({
            entry.path,
            vektoryum::release::PackageEntryKind::RegularFile,
            entry.size_bytes,
            entry.sha256,
        });
    }
    return observed;
}

}  // namespace

int main() {
    using namespace vektoryum::release;

    const ReleaseManifest manifest = make_manifest();
    const PackageInventory inventory = make_inventory(manifest);
    const auto observed = make_observed(inventory);

    assert(validate_package_contents(manifest, inventory, observed).ok());

    auto reordered = observed;
    std::reverse(reordered.begin(), reordered.end());
    assert(validate_package_contents(manifest, inventory, reordered).ok());

    auto missing = observed;
    missing.pop_back();
    assert(validate_package_contents(manifest, inventory, missing).error == PackageContentsError::MissingEntry);

    auto unexpected = observed;
    unexpected.push_back({"build/debug.log", PackageEntryKind::RegularFile, 10U, std::string(64U, 'e')});
    assert(validate_package_contents(manifest, inventory, unexpected).error == PackageContentsError::UnexpectedEntry);

    auto traversal = observed;
    traversal.push_back({"../escape", PackageEntryKind::RegularFile, 10U, std::string(64U, 'e')});
    assert(validate_package_contents(manifest, inventory, traversal).error == PackageContentsError::UnexpectedEntry);

    auto symlink = observed;
    symlink.front().kind = PackageEntryKind::Symlink;
    assert(validate_package_contents(manifest, inventory, symlink).error == PackageContentsError::NonRegularEntry);

    auto directory = observed;
    directory.front().kind = PackageEntryKind::Directory;
    assert(validate_package_contents(manifest, inventory, directory).error == PackageContentsError::NonRegularEntry);

    auto wrong_size = observed;
    ++wrong_size.front().size_bytes;
    assert(validate_package_contents(manifest, inventory, wrong_size).error == PackageContentsError::SizeMismatch);

    auto wrong_digest = observed;
    wrong_digest.front().sha256 = std::string(64U, 'f');
    assert(validate_package_contents(manifest, inventory, wrong_digest).error == PackageContentsError::DigestMismatch);

    auto duplicate = observed;
    duplicate.push_back(observed.front());
    assert(validate_package_contents(manifest, inventory, duplicate).error == PackageContentsError::DuplicateObservedPath);

    auto foreign_manifest = manifest;
    foreign_manifest.channel = "stable";
    assert(validate_package_contents(foreign_manifest, inventory, observed).error == PackageContentsError::ReleaseManifestMismatch);

    auto invalid_manifest = manifest;
    invalid_manifest.source_revision = "not-a-revision";
    assert(validate_package_contents(invalid_manifest, inventory, observed).error == PackageContentsError::InvalidReleaseManifest);

    auto invalid_inventory = inventory;
    invalid_inventory.entries.front().path = "tmp/secret.txt";
    assert(validate_package_contents(manifest, invalid_inventory, observed).error == PackageContentsError::InvalidPackageInventory);

    assert(package_contents_error_name(PackageContentsError::UnexpectedEntry) == "unexpected_entry");
    assert(package_contents_error_name(PackageContentsError::NonRegularEntry) == "non_regular_entry");
    assert(package_contents_error_name(PackageContentsError::ReleaseManifestMismatch) == "release_manifest_mismatch");

    return 0;
}
