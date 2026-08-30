#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vektoryum::release {

inline constexpr std::string_view package_inventory_schema = "vektoryum.package-inventory.v1";
inline constexpr std::size_t package_inventory_max_entries = 16U;
inline constexpr std::uint64_t package_inventory_max_total_bytes = 512U * 1024U * 1024U;

struct PackageInventoryEntry {
    std::string path;
    std::uint64_t size_bytes{0U};
    std::string sha256;
};

struct PackageInventory {
    std::string schema_version;
    std::string release_manifest_sha256;
    std::vector<PackageInventoryEntry> entries;
};

enum class PackageInventoryError {
    None = 0,
    UnsupportedSchema,
    InvalidReleaseManifestDigest,
    EmptyInventory,
    TooManyEntries,
    UnsupportedPath,
    DuplicatePath,
    InvalidEntryDigest,
    InvalidEntrySize,
    TotalSizeExceeded,
};

struct PackageInventoryValidation {
    PackageInventoryError error{PackageInventoryError::None};

    [[nodiscard]] bool ok() const noexcept { return error == PackageInventoryError::None; }
};

[[nodiscard]] bool is_allowed_package_path(std::string_view path) noexcept;
[[nodiscard]] PackageInventoryValidation validate_package_inventory(const PackageInventory& inventory) noexcept;
[[nodiscard]] std::string canonical_package_inventory(const PackageInventory& inventory);
[[nodiscard]] std::string package_inventory_sha256(const PackageInventory& inventory);
[[nodiscard]] std::string_view package_inventory_error_name(PackageInventoryError error) noexcept;

}  // namespace vektoryum::release
