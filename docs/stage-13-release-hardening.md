# Stage 13 — Release hardening

Weight: **3%**

Status: active. Stage 13 is complete only when release identity, packaging, provenance, reproducibility and final cross-platform acceptance are fail-closed and auditable without weakening any Stage 0-12 gate.

## Required contracts

- Deterministic versioned release manifest bound to the exact product version, stable Core API schema and exact source revision.
- No build timestamp, hostname, workspace path, random identifier or other ambient machine state may alter canonical release identity.
- Release channels are explicit and fail closed; unsupported channels are rejected.
- Release artifacts must be deterministically packaged with cryptographic digests and exact manifest binding.
- Package contents must be allow-listed; source/build residue, secrets, transient files and untracked payloads must not enter a release artifact.
- Installed/extracted CLI behavior must preserve the Stage 12 byte-exact CLI/API contract and validated Stage 11 provenance chain.
- Cross-platform release candidates must be tested on Ubuntu, Windows and macOS, with Linux ASan/UBSan coverage retained for repository-controlled code paths.
- Final release acceptance requires fresh exact-head green CI, mergeable=true and zero unresolved blocking review threads.

## First safe slice — canonical release identity

The first slice introduces `vektoryum.release-manifest.v1`. A release manifest binds the current product version, `vektoryum.core-api.v1`, an exact lowercase 40-character source revision and an explicit `candidate` or `stable` channel. Canonical serialization is deterministic and its SHA-256 digest is derived only from those explicit fields. The manifest deliberately has no timestamp, hostname, workspace path, locale-dependent value or random identifier.

Validation fails closed for a foreign manifest schema, foreign product version, foreign Core API schema, malformed/uppercase source revision and unsupported release channel. Tests prove byte-for-byte manifest repeatability, digest repeatability, stable-channel support and cryptographic separation between candidate and stable identities. The dedicated CTest target is included in normal cross-platform CI and Linux ASan/UBSan builds.

## Second safe slice — allow-listed package inventory

The second slice introduces `vektoryum.package-inventory.v1`. Every inventory is cryptographically bound to one canonical release-manifest SHA-256 and contains only explicit logical package paths, non-zero byte sizes and lowercase SHA-256 file identities. Canonical serialization sorts entries by path, so input enumeration order cannot change package identity. The inventory digest is SHA-256 over those canonical bytes.

The initial package allow-list contains only the production CLI and release metadata logical paths. Duplicate paths, traversal/absolute or otherwise non-allow-listed paths, debug/build residue, malformed digests, empty files, empty inventories and packages that exceed the immutable 512 MiB total budget fail closed. Replacing the bound release-manifest digest changes the package-inventory identity. Dedicated CTest coverage runs on Ubuntu, Windows and macOS and under Linux ASan/UBSan without weakening any Stage 0-12 gate.

## Remaining acceptance

- Add deterministic package assembly/install/extract verification that consumes only a validated package inventory and rejects hostile paths or undeclared residue at the filesystem boundary.
- Prove release package reproducibility from identical explicit inputs and reject manifest/package substitution.
- Execute the packaged CLI contract end-to-end on Ubuntu, Windows and macOS while retaining sanitizer coverage for release-contract code.
- Perform final fresh exact-head Stage 13 acceptance before expected-head merge.
