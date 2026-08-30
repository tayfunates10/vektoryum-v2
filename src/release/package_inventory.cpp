#include "vektoryum/release/package_inventory.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "vektoryum/ml/artifact_digest.hpp"

namespace vektoryum::release {
namespace {

[[nodiscard]] bool is_lower_sha256(std::string_view value) noexcept {
    if (value.size() != 64U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

[[nodiscard]] bool has_duplicate_paths(const std::vector<PackageInventoryEntry>& entries) {
    std::vector<std::string_view> paths;
    paths.reserve(entries.size());
    for (const auto& entry : entries) {
        paths.push_back(entry.path);
    }
    std::sort(paths.begin(), paths.end());
    return std::adjacent_find(paths.begin(), paths.end()) != paths.end();
}

}  // namespace

bool is_allowed_package_path(std::string_view path) noexcept {
    return path == "bin/vektoryum_cli" ||
           path == "release/release-manifest.txt" ||
           path == "release/package-inventory.txt";
}

PackageInventoryValidation validate_package_inventory(const PackageInventory& inventory) noexcept {
    if (inventory.schema_version != package_inventory_schema) {
        return {PackageInventoryError::UnsupportedSchema};
    }
    if (!is_lower_sha256(inventory.release_manifest_sha256)) {
        return {PackageInventoryError::InvalidReleaseManifestDigest};
    }
    if (inventory.entries.empty()) {
        return {PackageInventoryError::EmptyInventory};
    }
    if (inventory.entries.size() > package_inventory_max_entries) {
        return {PackageInventoryError::TooManyEntries};
    }
    if (has_duplicate_paths(inventory.entries)) {
        return {PackageInventoryError::DuplicatePath};
    }

    std::uint64_t total_bytes = 0U;
    for (const auto& entry : inventory.entries) {
        if (!is_allowed_package_path(entry.path)) {
            return {PackageInventoryError::UnsupportedPath};
        }
        if (!is_lower_sha256(entry.sha256)) {
            return {PackageInventoryError::InvalidEntryDigest};
        }
        if (entry.size_bytes == 0U) {
            return {PackageInventoryError::InvalidEntrySize};
        }
        if (entry.size_bytes > package_inventory_max_total_bytes - total_bytes) {
            return {PackageInventoryError::TotalSizeExceeded};
        }
        total_bytes += entry.size_bytes;
    }
    return {};
}

std::string canonical_package_inventory(const PackageInventory& inventory) {
    std::vector<PackageInventoryEntry> entries = inventory.entries;
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.path < rhs.path;
    });

    std::string output;
    output += "schema=";
    output += inventory.schema_version;
    output += '\n';
    output += "release_manifest_sha256=";
    output += inventory.release_manifest_sha256;
    output += '\n';
    for (const auto& entry : entries) {
        output += "file=";
        output += entry.path;
        output += '|';
        output += std::to_string(entry.size_bytes);
        output += '|';
        output += entry.sha256;
        output += '\n';
    }
    return output;
}

std::string package_inventory_sha256(const PackageInventory& inventory) {
    const std::string canonical = canonical_package_inventory(inventory);
    const std::vector<std::uint8_t> bytes(canonical.begin(), canonical.end());
    return ml::sha256_hex(bytes);
}

std::string_view package_inventory_error_name(PackageInventoryError error) noexcept {
    switch (error) {
        case PackageInventoryError::None:
            return "none";
        case PackageInventoryError::UnsupportedSchema:
            return "unsupported_schema";
        case PackageInventoryError::InvalidReleaseManifestDigest:
            return "invalid_release_manifest_digest";
        case PackageInventoryError::EmptyInventory:
            return "empty_inventory";
        case PackageInventoryError::TooManyEntries:
            return "too_many_entries";
        case PackageInventoryError::UnsupportedPath:
            return "unsupported_path";
        case PackageInventoryError::DuplicatePath:
            return "duplicate_path";
        case PackageInventoryError::InvalidEntryDigest:
            return "invalid_entry_digest";
        case PackageInventoryError::InvalidEntrySize:
            return "invalid_entry_size";
        case PackageInventoryError::TotalSizeExceeded:
            return "total_size_exceeded";
    }
    return "unknown";
}

}  // namespace vektoryum::release
