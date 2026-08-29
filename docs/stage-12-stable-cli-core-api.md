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

## Remaining acceptance

- Bind processing/export API operations to exact validated Stage 11 certificate artifacts rather than free-form identifiers.
- Add adversarial CLI parsing and response/error stream fixtures across platforms.
- Add end-to-end repeatability for canonical API/CLI operations and provenance substitution rejection.
- Require fresh exact-head source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan green, mergeable=true and zero unresolved blocking review threads before Stage 12 completion.
