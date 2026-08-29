# Stage 11 — Quality/performance certification

Weight: **6%**

Status: active; do not count this stage complete until deterministic quality/performance evidence, exact provenance, bounded resources, adversarial regressions, exact-head CI, clean mergeability and zero blocking review threads satisfy every acceptance item.

## Required contracts

- Fixed, explicit quality and performance metric definitions with immutable acceptance thresholds.
- Exact input/output/toolchain provenance binding for every certificate.
- Fail-closed nonfinite, incomplete, stale, reordered, duplicated or threshold-violating evidence.
- Bounded sample counts, execution work, memory and benchmark artifact resources.
- Deterministic canonical certificate serialization and repeatable certificate identity.
- End-to-end certification over canonical Stage 0-10 outputs without weakening prior topology, alpha, provenance, exporter or sanitizer gates.
- Adversarial quality/performance fixtures, including near-threshold values, malformed evidence and provenance substitution.
- Existing source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan gates remain mandatory.

## First safe slice

The initial quality-certificate request contract requires schema/certificate identity, canonical lowercase input/output SHA-256 provenance, an explicit toolchain revision, bounded sample/execution budgets and a deterministically ordered metric-gate list. Metric values and bounds must all be finite, metric names must be unique and strictly ordered, and every measured value must remain within its immutable minimum/maximum gate. Validation fails closed on malformed digest provenance, missing toolchain identity, zero/excess resource evidence, empty/oversized metric sets, nonfinite metrics, duplicates, nondeterministic order and any threshold violation. Canonical request reporting is deterministic. Dedicated adversarial CTest coverage is wired into normal cross-platform tests and Linux sanitizer builds.

## Remaining acceptance

Material work remains: bind certification requests to exact validated Stage 10 artifacts rather than free-form digests; add deterministic certificate artifact digests/signature-ready evidence; define and exercise concrete image/vector/performance metric implementations over canonical outputs; add peak-memory/wall-work accounting without environment-dependent acceptance; prove repeatability across fixtures; and add broader adversarial end-to-end certification scenarios.
