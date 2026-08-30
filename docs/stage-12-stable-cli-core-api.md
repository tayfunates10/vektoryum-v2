# Stage 12 — Stable CLI/Core API

Weight: **3%**

Status: completed in PR #14 after fresh exact-head cross-platform and Linux sanitizer acceptance, mergeable=true and zero unresolved blocking review threads.

## Required contracts

- Explicit versioned Core API request/response schema with stable operation identifiers.
- Stable CLI commands, deterministic stdout/stderr and documented exit/error semantics.
- Bounded request identifiers, payloads, outputs, execution work and memory.
- Exact binding to validated Stage 11 certificate/provenance evidence for processing operations.
- No silent fallback, provenance substitution, stale evidence acceptance or ambiguous serialization.
- Deterministic repeatability across canonical operations and supported platforms.
- Adversarial malformed, unsupported, oversized and stale request coverage.
- Existing Stage 0-11 quality, topology, alpha, provenance, exporter, certification and sanitizer gates remain mandatory.

## Completed slices

The first slice introduced `vektoryum.core-api.v1` as the stable API envelope schema, fixed API major/minor constants, stable process exit codes (`0` success, `64` usage, `65` data, `70` software), a bounded request identifier, a version operation, fail-closed unsupported schema/operation handling and deterministic canonical request reporting with delimiter rejection. The existing no-argument CLI behavior remains compatible, while `--version` and `--help` have explicit deterministic semantics and unsupported command lines return the stable usage exit code. Dedicated CTest coverage verifies schema/version constants, exit codes, canonical repeatability, request-id bounds, delimiter injection rejection and unsupported operations, and is included in Linux ASan/UBSan builds.

The second slice added an explicit stable response envelope with status, exit-code and request-error fields plus deterministic canonical serialization. Stable textual names are defined for every current request error and response status, success/error response fixtures are byte-for-byte repeatable, and no Stage 0-11 gate was weakened.

The third slice added explicit public-boundary resource accounting to each request envelope: declared input bytes, output bytes and work units are serialized deterministically and validated against immutable default ceilings of 64 MiB input, 256 MiB output and 1,000,000 work units. Exact-boundary requests are accepted, while a one-unit excursion beyond any ceiling fails closed with a stable request error.

The fourth slice introduced the processing/export operation `certified_export` and made the ordinary envelope fail closed with `missing_certificate_evidence`. A certified operation request carries the exact lowercase Stage 11 certificate digest and is validated alongside the concrete `QualityCertificateArtifact`. Validation requires bounded request accounting, a structurally valid certificate artifact, SHA-256 recomputation over canonical certificate bytes, and exact request/artifact digest equality. Regressions reject missing evidence, digest substitution, tampered canonical bytes, malformed certificate digests and operation substitution.

The fifth slice locked the CLI process boundary with adversarial, byte-exact stream fixtures on Ubuntu, Windows and macOS and again against the ASan/UBSan Linux binary. It proves deterministic `--help`, `--version`, no-argument compatibility and fail-closed malformed/unsupported command lines with stable exit code 64, empty stdout and exact canonical stderr.

The sixth slice replaced synthetic certificate evidence at the public API boundary with a certificate produced by the real Stage 11 issuance path. Repeated issuance returns byte-identical canonical certificate bytes and identical digests, and the exact artifact is handed into `validate_certified_operation_request(...)`. Request-digest substitution, artifact-digest substitution, canonical-byte tampering, malformed digest data and operation substitution remain fail closed.

The seventh slice added deterministic certified processing execution at the stable Core API boundary. `execute_certified_operation(...)` reuses exact certificate validation and returns a canonical certified response that carries the validated Stage 11 certificate digest only on success. Repeated execution with independently issued but byte-identical Stage 11 certificates produces byte-identical response bytes; rejected provenance is never reflected as accepted identity.

The eighth slice completed certified processing CLI transport. `--certified-export REQUEST_ID [CERTIFICATE_SHA256]` constructs the deterministic Stage 10 export fixture, issues a real Stage 11 certificate, routes the request through `execute_certified_operation(...)`, and writes `canonical_certified_response_report(...)` directly to stdout. Omitting the optional digest binds exact issued identity; supplying the same digest is byte-identical, while substitution fails closed with exit code 65 and empty reflected certificate identity.

## Acceptance result

Stage 12 passed fresh exact-head source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan CI on PR #14 with mergeable=true and zero unresolved blocking review threads. Stage 12 is closed; Stage 13 release hardening must preserve every Stage 0-12 gate.
