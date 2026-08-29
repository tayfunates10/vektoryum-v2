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

The hybrid-output safety contract binds the final output to the exact plan identity and canonical output SHA-256, requires an explicit vector-topology revision, enforces the existing seam limit without threshold relaxation, requires expected and actual vector component counts to match, and rejects any declaration that raster fallback replaced vector-owned geometry. Final-output provenance must contain exactly the plan's contributions in canonical order with matching contribution identities, source revisions and source SHA-256 digests. Missing, substituted or drifted provenance fails closed. The canonical output report deterministically records the output, plan, topology revision and every contributing source revision/digest. Adversarial CTest regressions and sanitizer coverage exercise seam overflow, non-finite seam evidence, topology loss, silent raster fallback and provenance substitution/drift.

## Remaining acceptance

Broader adversarial mixed-content execution fixtures covering overlapping vector/raster boundaries, tiny vector features and high-frequency raster detail remain material acceptance items. Exact-head cross-platform and sanitizer CI must also validate the new output-safety slice before Stage 9 can be completed.
