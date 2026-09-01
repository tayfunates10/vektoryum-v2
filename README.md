# Vektoryum v2

Contract/infrastructure roadmap completion: **100%**

Functional end-user product readiness: **post-R6 real-user corrective program active; U1 implementation acceptance complete, README exact-head CI pending**.

The Stage 0-13 contract roadmap and corrective R1-R6 implementation are complete, but a 2026-09-01 real-user audit proved that this still does **not** establish general-user raster-to-vector readiness. The measured audit exposed alpha/coverage semantic inconsistency, topology ambiguity on realistic contours, broken serialized SVG hole semantics, loss of source colors, certification that was not derived from the final serialized output, an upscale path that did not feed reconstruction, and cubic recovery that was not exercised by the real CLI path. These are tracked as a strict post-R6 U1-U8 corrective program. Existing quality thresholds, provenance, sanitizer behavior, API/CLI contracts and fail-closed acceptance gates remain immutable unless a later roadmap stage explicitly adds a stricter gate.

Workflow: branch → pull request → exact-head CI → merge only after all required checks pass.

## Contract roadmap

| Stage | Scope | Weight | Contract status |
|---|---|---:|---|
| 0 | Specification and roadmap | 4% | done |
| 1 | Repository foundation and CI | 6% | done |
| 2 | Image Core | 8% | done |
| 3 | Raster resampler | 8% | done |
| 4 | Content analyzer/router | 5% | done |
| 5 | Vector reconstruction | 12% | done |
| 6 | Photo restoration and non-ML SR | 12% | done |
| 7 | Vektoryum ML/DL model/runtime | 14% | done |
| 8 | Training/data/benchmark pipeline | 7% | done |
| 9 | Hybrid reconstruction | 7% | done |
| 10 | Exporter contracts | 5% | done |
| 11 | Quality/performance certification contracts | 6% | done |
| 12 | Stable CLI/Core API contracts | 3% | done |
| 13 | Release hardening contracts | 3% | done |

## Product-readiness corrective roadmap R1-R6

The post-roadmap visual audit exposed gaps that the contract-focused acceptance suite did not prove. The first corrective program is tracked separately from the completed contract roadmap.

| Corrective stage | Acceptance goal | Status |
|---|---|---|
| R1 | Real Release builds on Linux/macOS/Windows with warnings-as-errors and meaningful tests | complete |
| R2 | PNG/JPEG/WebP/TIFF decode, color/alpha handling and CLI `convert` | complete |
| R3 | Resampler artifact/root-cause fix plus fixture PSNR/SSIM and visual regression gates | complete |
| R4 | Curve recovery as cubic Bézier/arc with fidelity-bounded node reduction | complete |
| R5 | Real scene-backed SVG/PDF/EPS/DXF encoders validated by format-aware structural checks | complete |
| R6 | Real input → analysis → upscale/vector → export → quality certificate end-to-end acceptance | complete |

## Post-R6 real-user corrective roadmap U1-U8

| Corrective stage | Acceptance goal | Status |
|---|---|---|
| U1 | One canonical alpha/coverage semantic across reconstruction, rasterize-back, quality and certification; soft-alpha regression | implementation complete; README exact-head CI pending |
| U2 | Deterministic saddle/diagonal topology resolution instead of rejecting realistic contour ambiguity | pending |
| U3 | Compound-path hole hierarchy and serialized SVG hole semantics validated from emitted output | pending |
| U4 | Preserve color regions/layers/fills and honor analyzer routing instead of forcing all inputs through binary vectorization | pending |
| U5 | Derive final certification from independently rasterized serialized output with real alpha/color/component-hole/boundary/residual metrics | pending |
| U6 | Feed the actual upscale result into reconstruction or fail the claimed upscale chain | pending |
| U7 | Enable fidelity-gated cubic fitting on the real CLI production path | pending |
| U8 | Lock the eight real-user fixtures and production component/residual/boundary gates without weakening existing vector gates | pending |

## Verified post-R6 milestones

### U1 — implementation complete; README CI pending

- `canonical_coverage_threshold` is explicitly fixed at 128 and `coverage_is_foreground()` is the shared foreground definition.
- Reconstruction defaults, certification-mask normalization and rasterize-back candidate-mask normalization now use the same canonical `coverage >= 128` semantic; the previous `>=128` versus `!=0` mismatch is removed without changing the threshold.
- Soft-alpha source coverage is preserved as the quality reference for transparent inputs, while opaque binary-vector inputs use the actual reconstruction mask as the alpha reference rather than an unrelated all-255 source-alpha plane.
- Candidate alpha is measured from rasterized reconstructed geometry instead of assigning the source alpha to both reference and candidate.
- Regression coverage locks the 127/128 boundary and proves rasterize-back IoU is exact when the same canonical coverage definition is used.
- Existing vector fidelity gates remain unchanged: `IoU >= 0.995` and disagreement ratio `<= 0.005`. Existing quality, provenance, sanitizer, API/CLI and fail-closed contracts are unchanged.
- U1 implementation acceptance is green on exact HEAD `18baf1dc5976ce7d18c86c3bc06e7bc6f3da98be` with `core-ci #326`; PR #22 was `mergeable=true` with zero unresolved blocking review threads at verification time. This README status commit must receive fresh exact-head green CI before U1 can merge.

## Verified corrective milestones R1-R6

### R1 — complete

- True Release builds are exercised on Linux, macOS and Windows with warnings-as-errors retained.
- Cross-platform CTest and Linux sanitizer coverage remain required acceptance gates.

### R2 — complete

- Bounded real raster ingestion recognizes PNG/JPEG/WebP/TIFF by content rather than file extension and fails closed on invalid/unsupported inputs.
- Accepted raster inputs normalize deterministically to canonical RGBA8, sRGB transfer/primaries and straight alpha semantics.
- PNG acceptance covers supported 8-bit Gray/RGB/Gray+Alpha/RGBA paths, scanline filters, CRC/Adler checks, stored/fixed/dynamic DEFLATE and color-management policy.
- JPEG acceptance covers baseline grayscale and normal three-component color sampling/subsampling paths required by the corrective acceptance suite.
- WebP acceptance exercises real multi-pixel lossless VP8L fixtures including RGB/alpha, palette/transform/predictor/back-reference behavior and deterministic decode.
- TIFF acceptance covers supported Gray/RGB/RGBA, byte-order/strip handling and associated-alpha normalization.
- CLI `--convert INPUT OUTPUT` is exercised across PNG/JPEG/WebP/TIFF and emits deterministic canonical PAM RGBA8 output.

### R3 — complete

- The separable resampler no longer clamps the horizontal intermediate pass; production local-range clamping is applied to the final reconstructed sample against the original 2D contributing source neighborhood, removing the axis-biased clipping mechanism without changing half-pixel mapping, Lanczos-3 reconstruction, normalized weights, anti-alias behavior or production clamping policy.
- Fixture-based measured quality gates require smooth-RGB round-trip PSNR ≥ 35 dB and SSIM ≥ 0.98.
- Premultiplied-alpha regression coverage prevents transparent-edge hidden-RGB leakage and keeps reconstructed RGB bounded by alpha.
- An 8× transpose-invariance visual-regression fixture gates horizontal/vertical artifact bias with a maximum axis delta of 1e-5.
- R3 was merged only after its README status commit received fresh exact-head green CI evidence.

### R4 — complete

- Dense reconstructed contours can be simplified deterministically and fitted to cubic Bézier geometry rather than remaining dense polyline-only output.
- Node reduction is accepted only when the independently rasterized curved candidate passes the existing fidelity certification gate (`IoU ≥ 0.995`, disagreement ratio `≤ 0.005`).
- Failed curve candidates fall back to the exact polygon; acceptance thresholds are never relaxed to force node reduction.
- Circle regression coverage proves cubic geometry emission, node reduction, fidelity preservation and deterministic serialization; strict exact-fidelity coverage proves fail-safe polygon fallback.
- R4 implementation acceptance is green on exact HEAD `efd35aeee4657529b91930a225dcf4120a5c754d` with `core-ci #299`; the subsequent README status commit also received fresh exact-head green evidence before merge.

### R5 — complete

- SVG, PDF, EPS and DXF geometry export is driven by reconstructed `SvgScene` paths rather than placeholder/canonical payloads.
- Linear and cubic path geometry is preserved in each format; even-odd fill semantics are retained for SVG/PDF/EPS and cubic curves are represented as DXF SPLINE entities.
- Geometry export remains behind the existing request/provenance/output-byte contract checks and rejects empty or structurally invalid scenes.
- Format-aware artifact validation rejects malformed SVG/PDF/EPS/DXF structures, while regression coverage proves exported digests change when reconstructed geometry changes.
- R5 implementation acceptance is green on exact HEAD `1b8fdaae750e5a9d4b098c266d5f0212be6e54dc` with `core-ci #303`; its README status commit subsequently received fresh exact-head green evidence before merge.

### R6 — complete

- CLI `--certified-convert INPUT OUTPUT FORMAT` executes a real bounded raster input through decode, content analysis, canonical linear-light premultiplied-alpha preparation, Lanczos3 2× upscale, vector reconstruction, cubic path fitting/fidelity certification, geometry-backed SVG/PDF/EPS/DXF export, measured quality/performance evidence and provenance-bound quality-certificate issuance.
- Upscale evidence is bound into the chain identity with SHA-256; export and certificate identities remain tied to real input-derived provenance.
- Fidelity certification retains the existing `IoU ≥ 0.995` and disagreement ratio `≤ 0.005` gates; binary mask normalization fixes representation mismatches without weakening acceptance.
- CLI integration executes the end-to-end path for SVG, PDF, EPS and DXF and requires real non-empty export/certificate artifacts plus measured evidence.
- R6 was merged after exact-head green CI and merge-gate verification; the later real-user audit is the authority for the U1-U8 corrective work and supersedes any earlier broad product-readiness claim.

## Verified contract milestones

- Stage 11 established immutable quality/provenance certification gates.
- Stage 12 established deterministic Core API and CLI contract behavior.
- Stage 13 established deterministic release/package identity, substitution rejection, hostile-path/residue rejection and staged CLI contract verification across Ubuntu, Windows and macOS.

These milestones remain valuable and are not being weakened. They are prerequisites, not substitutes, for a working visual conversion product.

## Current corrective priority

1. Obtain fresh exact-head green CI evidence for the U1 README status commit.
2. Re-verify `mergeable=true` and zero unresolved blocking review threads.
3. Merge U1 only with expected-head protection after all U1 acceptance evidence remains green.
4. Open U2 only after U1 is merged, then proceed sequentially through U8.

UI, account and subscription work remains deferred until the post-R6 real-user corrective program reaches its required acceptance state.
