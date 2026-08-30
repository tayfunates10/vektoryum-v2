#include "vektoryum/release/package_contents.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace vektoryum::release {
namespace {

[[nodiscard]] const PackageInventoryEntry* find_inventory_entry(
    const PackageInventory& inventory,
    std::string_view path) noexcept {
    const auto it = std::find_if(inventory.entries.begin(), inventory.entries.end(), [path](const auto& entry) {
        return entry.path == path;
    });
    return it == inventory.entries.end() ? nullptr : &*it;
}

[[nodiscard]] bool has_duplicate_observed_paths(const std::vector<ObservedPackageEntry>& observed) {
    std::vector<std::string_view> paths;
    paths.reserve(observed.size());
    for (const auto& entry : observed) {
        paths.push_back(entry.path);
    }
    std::sort(paths.begin(), paths.end());
    return std::adjacent_find(paths.begin(), paths.end()) != paths.end();
}

}  // namespace

PackageContentsValidation validate_package_contents(
    const ReleaseManifest& manifest,
    const PackageInventory& inventory,
    const std::vector<ObservedPackageEntry>& observed) noexcept {
    if (!validate_release_manifest(manifest).ok()) {
        return {PackageContentsError::InvalidReleaseManifest};
    }
    if (!validate_package_inventory(inventory).ok()) {
        return {PackageContentsError::InvalidPackageInventory};
    }
    if (inventory.release_manifest_sha256 != release_manifest_sha256(manifest)) {
        return {PackageContentsError::ReleaseManifestMismatch};
    }
    if (has_duplicate_observed_paths(observed)) {
        return {PackageContentsError::DuplicateObservedPath};
    }

    for (const auto& entry : observed) {
        if (!is_allowed_package_path(entry.path)) {
            return {PackageContentsError::UnexpectedEntry};
        }
        const PackageInventoryEntry* expected = find_inventory_entry(inventory, entry.path);
        if (expected == nullptr) {
            return {PackageContentsError::UnexpectedEntry};
        }
        if (entry.kind != PackageEntryKind::RegularFile) {
            return {PackageContentsError::NonRegularEntry};
        }
        if (entry.size_bytes != expected->size_bytes) {
            return {PackageContentsError::SizeMismatch};
        }
        if (entry.sha256 != expected->sha256) {
            return {PackageContentsError::DigestMismatch};
        }
    }

    if (observed.size() != inventory.entries.size()) {
        return {PackageContentsError::MissingEntry};
    }
    return {};
}

std::string_view package_contents_error_name(PackageContentsError error) noexcept {
    switch (error) {
        case PackageContentsError::None:
            return "none";
        case PackageContentsError::InvalidReleaseManifest:
            return "invalid_release_manifest";
        case PackageContentsError::InvalidPackageInventory:
            return "invalid_package_inventory";
        case PackageContentsError::ReleaseManifestMismatch:
            return "release_manifest_mismatch";
        case PackageContentsError::UnexpectedEntry:
            return "unexpected_entry";
        case PackageContentsError::DuplicateObservedPath:
            return "duplicate_observed_path";
        case PackageContentsError::MissingEntry:
            return "missing_entry";
        case PackageContentsError::NonRegularEntry:
            return "non_regular_entry";
        case PackageContentsError::SizeMismatch:
            return "size_mismatch";
        case PackageContentsError::DigestMismatch:
            return "digest_mismatch";
    }
    return "unknown";
}

}  // namespace vektoryum::release
