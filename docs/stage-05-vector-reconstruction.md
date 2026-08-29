# Stage 5 — Vector reconstruction

Stage weight: **12%**. Core progress remains **31%** until this stage is merged after all required gates pass.

## Scope

This stage introduces project-owned raster-to-vector reconstruction primitives. The initial production slice converts thresholded coverage masks into deterministic closed grid contours, preserves nested holes through even-odd filling, removes redundant collinear nodes, enforces a hard node budget, rejects degenerate or ambiguous topology, and measures rasterize-back fidelity with binary IoU.

## Safety and topology contract

- Zero-sized, mismatched and coordinate-overflowing inputs fail closed.
- Empty foreground is not treated as a successful vector scene.
- Diagonal-only touching regions that create more than one outgoing contour edge at a vertex are rejected as `TopologyAmbiguity`; the engine does not invent connectivity.
- Node-budget exhaustion is a hard error and cannot silently simplify away topology.
- Contour traversal is deterministic for identical input.
- Nested closed paths are rasterized with the even-odd rule so holes survive round-trip reconstruction.

## Current acceptance gates

- Rectangle contour extraction produces a four-corner closed path after collinear reduction.
- Donut topology produces outer + hole paths and exact rasterize-back IoU.
- Deterministic repeat reconstruction produces identical path nodes.
- Empty masks, shape mismatch, zero dimensions, coordinate overflow and node-budget exhaustion fail closed.
- Diagonal ambiguity fails closed rather than selecting an arbitrary continuation.
- Existing source hygiene, Ubuntu/Windows/macOS builds and tests, and Linux ASan/UBSan remain mandatory.

## Remaining Stage 5 work

Before Stage 5 can be considered complete, the geometry layer must expand beyond binary grid contours to controlled curve/corner fitting, self-intersection detection, topology/component accounting, node-complexity metrics, SVG-ready path semantics, and broader rasterize-back quality fixtures. No acceptance threshold may be weakened to obtain a pass.
