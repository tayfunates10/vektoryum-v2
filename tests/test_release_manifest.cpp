#include <iostream>
#include <string>

#include "vektoryum/release/release_manifest.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace vektoryum::release;

    const std::string revision(40U, 'a');
    const ReleaseManifest first = make_release_manifest(revision, "candidate");
    const ReleaseManifest second = make_release_manifest(revision, "candidate");

    expect(validate_release_manifest(first).ok(), "canonical release manifest must validate");
    expect(canonical_release_manifest(first) == canonical_release_manifest(second),
           "canonical release manifest must be byte-repeatable");
    expect(release_manifest_sha256(first) == release_manifest_sha256(second),
           "release manifest digest must be repeatable");
    expect(release_manifest_sha256(first).size() == 64U,
           "release manifest digest must be lowercase SHA-256 hex length");

    ReleaseManifest exact_copy = first;
    expect(canonical_release_manifest(first) == canonical_release_manifest(exact_copy),
           "identical release identity must not gain ambient build metadata");

    ReleaseManifest bad_revision = first;
    bad_revision.source_revision = std::string(39U, 'a');
    expect(validate_release_manifest(bad_revision).error == ReleaseManifestError::InvalidSourceRevision,
           "short source revision must fail closed");

    bad_revision = first;
    bad_revision.source_revision[0] = 'A';
    expect(validate_release_manifest(bad_revision).error == ReleaseManifestError::InvalidSourceRevision,
           "uppercase source revision must fail closed");

    ReleaseManifest bad_schema = first;
    bad_schema.schema_version = "vektoryum.release-manifest.v2";
    expect(validate_release_manifest(bad_schema).error == ReleaseManifestError::UnsupportedSchema,
           "unsupported release manifest schema must fail closed");

    ReleaseManifest bad_version = first;
    bad_version.product_version = "9.9.9";
    expect(validate_release_manifest(bad_version).error == ReleaseManifestError::UnsupportedProductVersion,
           "foreign product version must fail closed");

    ReleaseManifest bad_api = first;
    bad_api.core_api_schema = "vektoryum.core-api.v999";
    expect(validate_release_manifest(bad_api).error == ReleaseManifestError::UnsupportedCoreApiSchema,
           "foreign Core API schema must fail closed");

    ReleaseManifest bad_channel = first;
    bad_channel.channel = "nightly";
    expect(validate_release_manifest(bad_channel).error == ReleaseManifestError::UnsupportedChannel,
           "unsupported release channel must fail closed");

    ReleaseManifest stable = make_release_manifest(revision, "stable");
    expect(validate_release_manifest(stable).ok(), "stable release channel must validate");
    expect(release_manifest_sha256(stable) != release_manifest_sha256(first),
           "release channel must be cryptographically bound into release identity");

    if (failures != 0) {
        return 1;
    }
    std::cout << "release manifest contract tests passed\n";
    return 0;
}
