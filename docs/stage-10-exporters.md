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

## First safe slice

The export request contract introduces an explicit SVG/PDF/EPS/DXF format enum, stable schema/export identity, exact source hybrid-output identity plus canonical lowercase SHA-256 binding, bounded pixel/output-byte budgets, fail-closed unknown formats and malformed provenance, and a deterministic canonical request report. Adversarial standalone CTest regressions cover unknown enum values, noncanonical digests, zero dimensions, pixel overflow/budget excess and output-byte budget violations. The target is included in normal cross-platform tests and Linux sanitizer builds.

## Remaining acceptance

Implement canonical format encoders, exact deterministic output-byte provenance, bounded encoder memory/execution accounting, format-specific structural validation, safe metadata/path contracts and adversarial end-to-end exporter fixtures before Stage 10 can be completed.
