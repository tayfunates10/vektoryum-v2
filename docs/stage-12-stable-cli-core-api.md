# Stage 12 — Stable CLI/Core API

Weight: **3%**

Status: active; do not count this stage complete until the stable Core API and CLI contracts are versioned, deterministic, bounded, fail closed, exactly preserve validated Stage 11 provenance, pass adversarial regressions, and satisfy fresh exact-head cross-platform plus Linux sanitizer acceptance.

## Required contracts

- Explicit versioned Core API request/response schema with stable operation identifiers.
- Stable CLI commands, deterministic stdout/stderr and documented exit/error semantics.
- Bounded request identifiers, payloads, outputs, execution work and memory.
- Exact binding to validated Stage 11 certificate/provenance evidence for processing operations.
- No silent fallback, provenance substitution, stale evidence acceptance or ambiguous serialization.
- Deterministic repeatability across canonical operations and supported platforms.
- Adversarial malformed, unsupported, oversized and stale request coverage.
- Existing Stage 0-11 quality, topology, alpha, provenance, exporter, certification and sanitizer gates remain mandatory.

## First safe slices

The first slice introduces `vektoryum.core-api.v1` as the stable API envelope schema, fixed API major/minor constants, stable process exit codes (`0` success, `64` usage, `65` data, `70` software), a bounded request identifier, a version operation, fail-closed unsupported schema/operation handling and deterministic canonical request reporting with delimiter rejection. The existing no-argument CLI behavior remains compatible, while `--version` and `--help` now have explicit deterministic semantics and unsupported command lines return the stable usage exit code. Dedicated CTest coverage verifies schema/version constants, exit codes, canonical repeatability, request-id bounds, delimiter injection rejection and unsupported operations, and is included in Linux ASan/UBSan builds.

The second slice adds an explicit stable response envelope with status, exit-code and request-error fields plus deterministic canonical serialization. Stable textual names are defined for every current request error and response status, success/error response fixtures are byte-for-byte repeatable, and no Stage 0-11 or existing Stage 12 gate is weakened.

The third slice adds explicit public-boundary resource accounting to each request envelope: declared input bytes, output bytes and work units are serialized deterministically and validated against immutable default ceilings of 64 MiB input, 256 MiB output and 1,000,000 work units. Exact-boundary requests are accepted, while a one-unit excursion beyond any ceiling fails closed with a stable request error. Existing request-id, schema, operation, response, sanitizer and Stage 0-11 gates remain unchanged.

The fourth slice introduces the first processing/export operation as `certified_export` and makes the ordinary envelope fail closed with `missing_certificate_evidence`; it cannot be accepted through the version-only validation path. A certified operation request must carry the exact lowercase Stage 11 certificate digest and must be validated alongside the concrete `QualityCertificateArtifact`. Validation requires bounded request accounting, a structurally valid certificate artifact, an exact SHA-256 recomputation over the artifact's canonical certificate bytes, and exact equality between the request evidence identity and the artifact digest. Canonical certified-request serialization includes that digest deterministically. Regressions reject missing certificate evidence, digest substitution, tampered canonical certificate bytes, malformed certificate digests and operation substitution. These tests remain in the existing Stage 12 CTest target and therefore run in normal cross-platform CI and Linux ASan/UBSan coverage.

The fifth slice locks the CLI process boundary with adversarial, byte-exact stream fixtures. A CMake-driven CLI contract test runs on Ubuntu, Windows and macOS and again against the ASan/UBSan instrumented Linux binary. It proves `--help` emits exactly one canonical stdout line with empty stderr, no-argument output is byte-identical to `--version`, repeated `--version` output is stable, and malformed/unsupported forms (`--unknown`, near-match `--help=1`, an explicit empty argument and multi-argument input) all fail closed with exit code 64, empty stdout and the exact canonical stderr line. Repeating the same invalid command must reproduce the same error bytes. No existing API, provenance, resource, sanitizer or Stage 0-11 gate is weakened.

## Remaining acceptance

- Add end-to-end repeatability for canonical API/CLI operations and provenance substitution rejection, including issuance-to-public-API handoff using actual Stage 11 certification fixtures rather than synthetic certificate objects.
- Require fresh exact-head source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan green, mergeable=true and zero unresolved blocking review threads before Stage 12 completion.
