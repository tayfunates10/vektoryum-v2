# Stage 10 — Exporters

Weight: **5%**

Status: active; do not count this stage complete until deterministic exporters, adversarial regressions, exact-head CI, clean mergeability and zero blocking review threads satisfy every acceptance item.

## Required contracts

- Explicit supported-format policy for SVG, PDF, EPS and DXF.
- Deterministic export request identity and canonical serialization.
- Exact binding to the validated hybrid output identity and canonical SHA-256 provenance.
- Bounded dimensions, pixels, output bytes, intermediate memory and execution resources.
- Fail-closed malformed, unknown-format, noncanonical provenance and resource-overflow requests.
- Deterministic bytes for identical validated input/configuration.
- Format-specific structural validity without weakening Stage 0-9 quality or topology guarantees.
- Safe metadata/path handling with no uncontrolled filesystem traversal or hidden source substitution.
- Adversarial format, size, metadata, path and provenance regressions.
- Existing source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan gates remain mandatory.

## Implemented slices

The export request contract introduces an explicit SVG/PDF/EPS/DXF format enum, stable schema/export identity, exact source hybrid-output identity plus canonical lowercase SHA-256 binding, bounded pixel/output-byte budgets, fail-closed unknown formats and malformed provenance, and a deterministic canonical request report. Adversarial standalone CTest regressions cover unknown enum values, noncanonical digests, zero dimensions, pixel overflow/budget excess, output-byte budget violations, stale hybrid-output identity/digest substitution and newline/carriage-return serialization collisions.

The encoded export artifact contract adds exact request/source identity binding for produced bytes, canonical SHA-256 output provenance using the repository SHA-256 implementation, output-size enforcement against the validated request budget, bounded peak intermediate memory and execution units, and fail-closed format-specific structural envelopes for SVG, PDF, EPS and DXF. Adversarial CTest regressions cover format/identity/source substitution, malformed or mismatched output digests, empty/oversized outputs, zero/excess execution resources and malformed format structure. The target is included in normal cross-platform tests and Linux sanitizer builds.

## Remaining acceptance

Implement canonical SVG/PDF/EPS/DXF byte encoders that generate the validated structures, prove identical validated input/configuration yields identical output bytes, add safe metadata/path contracts, and add adversarial end-to-end exporter fixtures before Stage 10 can be completed.
