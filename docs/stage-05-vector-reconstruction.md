# Stage 5 — Vector reconstruction

Stage weight: **12%**. Core progress remains **31%** until this stage is merged after all required gates pass.

## Scope

This stage introduces project-owned raster-to-vector reconstruction primitives. The production slice converts thresholded coverage masks into deterministic closed grid contours, preserves nested holes through even-odd filling, removes redundant collinear nodes, enforces a hard node budget, rejects degenerate or ambiguous topology, and measures rasterize-back fidelity with binary IoU.

The geometry layer now also exposes an SVG-ready command contract with explicit move/line/cubic/close semantics and deterministic optional cubic corner candidates. Zero-radius fitting preserves the certified polygon exactly. Positive-radius curves are candidates only and must not replace certified polygon geometry in a production path until a rasterize-back quality gate accepts them.

## Safety and topology contract

- Zero-sized, mismatched and coordinate-overflowing inputs fail closed.
- Empty foreground is not treated as a successful vector scene.
- Diagonal-only touching regions that create more than one outgoing contour edge at a vertex are rejected as `TopologyAmbiguity`; the engine does not invent connectivity.
- Node-budget exhaustion is enforced while collecting edges so hostile masks cannot allocate unbounded contour state before rejection.
- Contour traversal is deterministic for identical input.
- Nested closed paths are rasterized with the even-odd rule so holes survive round-trip reconstruction.
- Self-intersecting paths are detected by the path-quality gate.
- Scene certification enforces node-complexity, pixel-budget, IoU and disagreement limits.
- Fidelity-gated simplification may remove nodes only when the certified raster result remains within the configured quality gates.
- SVG-ready conversion rejects open/degenerate paths and invalid curve radii.

## Current acceptance gates

- Rectangle contour extraction produces a four-corner closed path after collinear reduction.
- Donut topology produces outer + hole paths and exact rasterize-back IoU.
- Deterministic repeat reconstruction produces identical path nodes.
- Empty masks, shape mismatch, zero dimensions, coordinate overflow and node-budget exhaustion fail closed.
- Diagonal ambiguity fails closed rather than selecting an arbitrary continuation.
- Self-intersection and scene-complexity checks fail closed.
- Fidelity-gated simplification is certified against the source mask.
- SVG-ready path conversion preserves hole metadata and deterministic command ordering.
- Optional rounded corners emit deterministic cubic commands without changing the certified polygon by default.
- Existing source hygiene, Ubuntu/Windows/macOS builds and tests, and Linux ASan/UBSan remain mandatory.

## Remaining Stage 5 work

Before Stage 5 can be considered complete, curved candidates still need direct rasterize-back certification before they can become production-selected geometry, and broader topology/component and adversarial fidelity fixtures must prove the full reconstruction contract. No acceptance threshold may be weakened to obtain a pass.
