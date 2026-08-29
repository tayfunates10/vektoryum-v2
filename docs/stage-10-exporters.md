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

The canonical encoder slice adds deterministic SVG/PDF/EPS/DXF byte generation from the same validated request/source-output pair, routes every produced artifact back through the Stage 10 artifact validator, records the actual output SHA-256, and proves repeated identical validated input produces identical bytes and digest for all four formats. Invalid source provenance and caller output-byte budget violations fail closed. The dedicated target is part of normal cross-platform CTest and Linux sanitizer coverage.

The destination-safety slice adds a fail-closed relative-path and metadata contract after canonical encoding. Absolute, drive-qualified, backslash, empty-component, dot-component and parent-traversal paths are rejected; path and metadata sizes/counts are bounded; newline/carriage-return/NUL metadata injection is rejected; reserved provenance keys and duplicate metadata keys are rejected so metadata cannot silently substitute source or output identity. The destination must also match the exact validated request/artifact source identity and digest. Its canonical report is deterministic and length-annotated. A cross-platform/sanitizer end-to-end fixture runs all four canonical encoders through the destination validator and adversarially covers traversal, absolute/drive paths, provenance-key substitution, metadata serialization injection, duplicate keys and artifact digest substitution.

## Remaining acceptance

No known material Stage 10 contract gap remains after the destination-safety and hostile-input end-to-end slice. Stage 10 remains incomplete until this exact HEAD is fully green across mandatory CI, mergeability is clean, and zero blocking review threads remain after final review.
