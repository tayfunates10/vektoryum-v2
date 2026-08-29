# Stage 9 — Hybrid reconstruction

Weight: **7%**

Status: active foundation; do not count this stage complete until implementation, adversarial regressions, exact-head CI, clean mergeability and zero blocking review threads satisfy every acceptance item.

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

## First implementation slice

The initial contract validates schema/plan identity, bounded dimensions and contribution count, unique contribution identities, canonical z-order, finite coverage/opacity ranges, and presence of both vector and raster contributions. This is foundation only and does not complete Stage 9.
