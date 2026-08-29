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

## First safe slice

The first slice introduces `vektoryum.core-api.v1` as the stable API envelope schema, fixed API major/minor constants, stable process exit codes (`0` success, `64` usage, `65` data, `70` software), a bounded request identifier, a version operation, fail-closed unsupported schema/operation handling and deterministic canonical request reporting with delimiter rejection. The existing no-argument CLI behavior remains compatible, while `--version` and `--help` now have explicit deterministic semantics and unsupported command lines return the stable usage exit code. Dedicated CTest coverage verifies schema/version constants, exit codes, canonical repeatability, request-id bounds, delimiter injection rejection and unsupported operations, and is included in Linux ASan/UBSan builds.

## Remaining acceptance

- Add stable response envelopes and error payloads with deterministic canonical serialization.
- Bind processing/export API operations to exact validated Stage 11 certificate artifacts rather than free-form identifiers.
- Add bounded input/output/work accounting at the public API boundary.
- Add adversarial CLI parsing and response/error stream fixtures across platforms.
- Add end-to-end repeatability for canonical API/CLI operations and provenance substitution rejection.
- Require fresh exact-head source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan green, mergeable=true and zero unresolved blocking review threads before Stage 12 completion.
