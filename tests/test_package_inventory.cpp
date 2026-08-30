#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "vektoryum/release/package_inventory.hpp"

namespace {

using vektoryum::release::PackageInventory;
using vektoryum::release::PackageInventoryEntry;
using vektoryum::release::PackageInventoryError;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

PackageInventory valid_inventory() {
    return PackageInventory{
        std::string(vektoryum::release::package_inventory_schema),
        std::string(64U, 'a'),
        {
            PackageInventoryEntry{"release/release-manifest.txt", 256U, std::string(64U, 'b')},
            PackageInventoryEntry{"bin/vektoryum_cli", 4096U, std::string(64U, 'c')},
        },
    };
}

}  // namespace

int main() {
    const auto inventory = valid_inventory();
    expect(vektoryum::release::validate_package_inventory(inventory).ok(), "valid inventory must pass");

    const std::string canonical_a = vektoryum::release::canonical_package_inventory(inventory);
    const std::string canonical_b = vektoryum::release::canonical_package_inventory(inventory);
    expect(canonical_a == canonical_b, "canonical inventory must be repeatable");
    expect(vektoryum::release::package_inventory_sha256(inventory) ==
               vektoryum::release::package_inventory_sha256(inventory),
           "inventory digest must be repeatable");

    auto reordered = inventory;
    std::reverse(reordered.entries.begin(), reordered.entries.end());
    expect(vektoryum::release::canonical_package_inventory(reordered) == canonical_a,
           "entry order must not change canonical bytes");
    expect(vektoryum::release::package_inventory_sha256(reordered) ==
               vektoryum::release::package_inventory_sha256(inventory),
           "entry order must not change inventory identity");

    auto substituted_release = inventory;
    substituted_release.release_manifest_sha256 = std::string(64U, 'd');
    expect(vektoryum::release::package_inventory_sha256(substituted_release) !=
               vektoryum::release::package_inventory_sha256(inventory),
           "release-manifest substitution must change inventory identity");

    auto duplicate = inventory;
    duplicate.entries.push_back(duplicate.entries.front());
    expect(vektoryum::release::validate_package_inventory(duplicate).error == PackageInventoryError::DuplicatePath,
           "duplicate paths must fail closed");

    auto hostile = inventory;
    hostile.entries.front().path = "../vektoryum_cli";
    expect(vektoryum::release::validate_package_inventory(hostile).error == PackageInventoryError::UnsupportedPath,
           "hostile path must fail closed");

    auto residue = inventory;
    residue.entries.front().path = "debug/vektoryum.pdb";
    expect(vektoryum::release::validate_package_inventory(residue).error == PackageInventoryError::UnsupportedPath,
           "non-allow-listed residue must fail closed");

    auto malformed_digest = inventory;
    malformed_digest.entries.front().sha256 = std::string(64U, 'G');
    expect(vektoryum::release::validate_package_inventory(malformed_digest).error == PackageInventoryError::InvalidEntryDigest,
           "malformed entry digest must fail closed");

    auto empty_file = inventory;
    empty_file.entries.front().size_bytes = 0U;
    expect(vektoryum::release::validate_package_inventory(empty_file).error == PackageInventoryError::InvalidEntrySize,
           "zero-byte inventory entries must fail closed");

    auto oversized = inventory;
    oversized.entries.front().size_bytes = vektoryum::release::package_inventory_max_total_bytes;
    expect(vektoryum::release::validate_package_inventory(oversized).error == PackageInventoryError::TotalSizeExceeded,
           "total package budget excursion must fail closed");

    auto empty = inventory;
    empty.entries.clear();
    expect(vektoryum::release::validate_package_inventory(empty).error == PackageInventoryError::EmptyInventory,
           "empty inventory must fail closed");

    return EXIT_SUCCESS;
}
