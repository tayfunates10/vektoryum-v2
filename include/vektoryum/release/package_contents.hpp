#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "vektoryum/release/package_inventory.hpp"
#include "vektoryum/release/release_manifest.hpp"

namespace vektoryum::release {

enum class PackageEntryKind : std::uint8_t {
    RegularFile = 0,
    Directory = 1,
    Symlink = 2,
    Other = 3,
};

struct ObservedPackageEntry {
    std::string path;
    PackageEntryKind kind{PackageEntryKind::RegularFile};
    std::uint64_t size_bytes{0U};
    std::string sha256;
};

enum class PackageContentsError : std::uint8_t {
    None = 0,
    InvalidReleaseManifest,
    InvalidPackageInventory,
    ReleaseManifestMismatch,
    UnexpectedEntry,
    DuplicateObservedPath,
    MissingEntry,
    NonRegularEntry,
    SizeMismatch,
    DigestMismatch,
};

struct PackageContentsValidation {
    PackageContentsError error{PackageContentsError::None};

    [[nodiscard]] bool ok() const noexcept { return error == PackageContentsError::None; }
};

[[nodiscard]] PackageContentsValidation validate_package_contents(
    const ReleaseManifest& manifest,
    const PackageInventory& inventory,
    const std::vector<ObservedPackageEntry>& observed) noexcept;
[[nodiscard]] std::string_view package_contents_error_name(PackageContentsError error) noexcept;

}  // namespace vektoryum::release
