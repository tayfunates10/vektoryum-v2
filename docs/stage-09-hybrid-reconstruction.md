# Stage 9 — Hybrid reconstruction

Weight: **7%**

Status: active; do not count this stage complete until implementation, adversarial regressions, exact-head CI, clean mergeability and zero blocking review threads satisfy every acceptance item.

## Required contracts

- Deterministic hybrid reconstruction plan with explicit schema and stable contribution ordering.
- Explicit vector/raster contribution identity and provenance; no silent source substitution.
- At least one vector and one raster contribution for a hybrid plan.
- Bounded pixel, contribution, intermediate-memory and execution resources.
- Fail-closed malformed, duplicate, non-finite, out-of-range or noncanonical contribution metadata.
- Deterministic routing/fusion decisions for identical inputs/configuration.
- Alpha-safe composition with no hidden-RGB leakage through transparent pixels.
- Seam safety across vector/raster boundaries without weakening existing quality thresholds.
- Topology preservation for vector-owned regions and no raster fallback that silently replaces rejected vector geometry.
- Deterministic output provenance identifying all contributing reconstruction paths and revisions.
- Adversarial mixed-content fixtures for ordering, overlaps, alpha boundaries, tiny vector features, high-frequency raster detail, resource overflow and malformed hybrid plans.
- Existing source hygiene, Ubuntu, Windows, macOS and Linux ASan/UBSan gates remain mandatory.

## Implemented slices

The planning contract validates schema/plan identity, bounded dimensions and contribution count, unique contribution identities, canonical z-order, finite coverage/opacity ranges, and presence of both vector and raster contributions.

The routing/provenance contract adds deterministic content-class routing: geometry is vector-owned and photographic detail is raster-owned. Every contribution must carry an immutable source identity, source revision and canonical lowercase SHA-256 digest. Aggregate intermediate-memory and execution-unit accounting is overflow-safe and bounded. Missing provenance, route/kind mismatches, zero resource declarations and aggregate budget overflow all fail closed with adversarial regressions.

The alpha-composition contract validates every RGBA channel as finite and within [0,1], rejects hidden RGB on fully transparent source pixels, performs deterministic source-over composition through premultiplied intermediates, and guarantees fully transparent output has zero RGB. Empty layer sets, non-finite/out-of-range channels and hidden-RGB inputs fail closed. Standalone adversarial CTest regressions and sanitizer coverage exercise these contracts.

## Remaining acceptance

Seam safety without threshold weakening, topology preservation/no silent raster fallback, deterministic final output provenance, and broader adversarial mixed-content execution fixtures remain material acceptance items. Stage 9 is therefore not complete yet.
