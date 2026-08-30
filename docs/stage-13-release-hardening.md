# Stage 13 — Release hardening

Weight: **3%**

Status: **completed**. Stage 13 release identity, packaging, provenance, reproducibility and cross-platform acceptance are fail-closed and auditable without weakening any Stage 0-12 gate.

## Required contracts

- Deterministic versioned release manifest bound to the exact product version, stable Core API schema and exact source revision.
- No build timestamp, hostname, workspace path, random identifier or other ambient machine state may alter canonical release identity.
- Release channels are explicit and fail closed; unsupported channels are rejected.
- Release artifacts are deterministically identified with cryptographic digests and exact manifest binding.
- Package contents are allow-listed; source/build residue, secrets, transient files and untracked payloads cannot enter an accepted release artifact.
- Installed/extracted CLI behavior preserves the Stage 12 byte-exact CLI/API contract and validated Stage 11 provenance chain.
- Cross-platform release candidates are tested on Ubuntu, Windows and macOS, with Linux ASan/UBSan coverage retained for repository-controlled code paths.
- Final release acceptance requires fresh exact-head green CI, mergeable=true and zero unresolved blocking review threads.

## Canonical release identity

`vektoryum.release-manifest.v1` binds the current product version, `vektoryum.core-api.v1`, an exact lowercase 40-character source revision and an explicit `candidate` or `stable` channel. Canonical serialization is deterministic and its SHA-256 digest is derived only from those explicit fields. The manifest deliberately has no timestamp, hostname, workspace path, locale-dependent value or random identifier.

Validation fails closed for a foreign manifest schema, foreign product version, foreign Core API schema, malformed/uppercase source revision and unsupported release channel. Revision hex validation is explicitly ASCII-only so locale state cannot alter acceptance.

## Allow-listed package inventory

`vektoryum.package-inventory.v1` is cryptographically bound to one canonical release-manifest SHA-256 and contains only explicit logical package paths, non-zero byte sizes and lowercase SHA-256 file identities. Canonical serialization sorts entries by path, so input enumeration order cannot change package identity.

Duplicate paths, traversal/absolute or otherwise non-allow-listed paths, debug/build residue, malformed digests, empty files, empty inventories and packages that exceed the immutable 512 MiB total budget fail closed. Digest validation is ASCII-only for locale-independent cross-platform behavior.

## Extracted package contents verification

Concrete package contents observed after assembly or extraction are checked against the validated release manifest and package inventory. Every observed payload must match one declared allow-listed path, be a regular file, and exactly match the declared byte size and SHA-256 identity.

Validation is order-independent but exact-set: missing entries, duplicate observed paths, undeclared residue, traversal/debug paths, symlinks, directories, other non-regular entries, size substitution, digest substitution and release-manifest substitution all fail closed.

## Complete package identity

`vektoryum.package-identity.v1` binds the exact canonical release-manifest SHA-256 and canonical package-inventory SHA-256 into one deterministic package identity. Identical explicit inputs produce byte-identical identity bytes and SHA-256; inventory enumeration order cannot alter identity. Release-manifest substitution, package-inventory substitution, forged package digest and foreign identity schema all fail closed.

## Staged packaged CLI contract

The Stage 12 CLI contract runner accepts an explicit staged executable path without changing any expected stdout/stderr bytes or exit codes. CI copies the built CLI into a clean `release-package/bin` staging tree and reruns the full byte-exact CLI contract from that staged location on Ubuntu, Windows and macOS.

The gate proves that the release-staged executable preserves `--help`, `--version`, unsupported-command fail-closed behavior, certified-export determinism and Stage 11 certificate provenance after packaging relocation. Linux ASan/UBSan coverage remains enabled for repository-controlled release-contract code.

## Final acceptance

Stage 13 is accepted only after a fresh exact-head CI pass on the final documentation head, `mergeable=true`, and zero unresolved blocking review threads. Once those conditions are satisfied, PR #15 may be merged with expected-head protection and the core-engine roadmap is 100% complete.
