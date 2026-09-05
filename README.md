# Vektoryum v2

Contract/infrastructure roadmap completion: **100%**

Functional end-user product readiness: **post-R6 real-user corrective program U1-U8 implementation acceptance complete; U8 README exact-head CI pending**.

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
| U1 | One canonical alpha/coverage semantic across reconstruction, rasterize-back, quality and certification; soft-alpha regression | complete |
| U2 | Deterministic saddle/diagonal topology resolution instead of rejecting realistic contour ambiguity | complete |
| U3 | Compound-path hole hierarchy and serialized SVG hole semantics validated from emitted output | complete |
| U4 | Preserve color regions/layers/fills and honor analyzer routing instead of forcing all inputs through binary vectorization | complete |
| U5 | Derive final certification from independently rasterized serialized output with real alpha/color/component-hole/boundary/residual metrics | complete |
| U6 | Feed the actual upscale result into reconstruction or fail the claimed upscale chain | complete |
| U7 | Enable fidelity-gated cubic fitting on the real CLI production path | complete |
| U8 | Lock the eight real-user fixtures and production component/residual/boundary gates without weakening existing vector gates | implementation complete; README exact-head CI pending |

## Verified post-R6 milestones

### U1 — complete

- `canonical_coverage_threshold` is explicitly fixed at 128 and `coverage_is_foreground()` is the shared foreground definition.
- Reconstruction defaults, certification-mask normalization and rasterize-back candidate-mask normalization use the same canonical `coverage >= 128` semantic.
- Candidate alpha is measured from rasterized reconstructed geometry instead of assigning the source alpha to both reference and candidate.
- Regression coverage locks the 127/128 boundary and proves rasterize-back IoU is exact when the same canonical coverage definition is used.
- Existing vector fidelity gates remain unchanged: `IoU >= 0.995` and disagreement ratio `<= 0.005`.

### U2 — complete

- Checkerboard/saddle contour junctions select the next unused edge with deterministic orientation-aware priority and stable coordinate tie-break.
- Diagonal-only touching foreground components remain separate contours.
- Regression requires deterministic contour ordering and exact canonical rasterize-back fidelity.
- Existing vector fidelity gates remain unchanged.

### U3 — complete

- SVG geometry serialization emits reconstructed contours as subpaths of one compound `<path>`.
- `fill-rule="evenodd"` preserves nested outer fill → hole → nested island parity.
- Regression verifies compound path and hole/island subpaths in serialized output.
- Existing vector fidelity gates remain unchanged.

### U4 — complete

- Certified CLI content analysis honors the analyzer route and proceeds to binary reconstruction only for `VectorReconstruction`.
- Source foreground colors are partitioned deterministically into bounded paint regions and serialized as independent compound paint layers.
- Regression proves vector routing, non-vector fail-closed routing and multi-color source preservation.
- Existing vector fidelity gates remain unchanged.

### U5 — complete

- Final SVG certification consumes the real serialized `encoded.artifact.bytes` through an independent parser/rasterizer.
- Final-output evidence binds exact output SHA-256 to independently measured alpha/vector fidelity, source-color MAE, component/hole topology, boundary p95 and visible-residual ratio.
- Regression proves final-byte digest binding, malformed-final-SVG rejection, fill-color sensitivity and compound/even-odd hole topology.
- U5 merged with expected-head protection after exact-head green `core-ci #372`; merge commit `080784419d7c9d6e4dc957645362334a6179f0d8`.

### U6 — complete

- The certified CLI passes the actual `upscaled.image` into `reconstruct_from_upscaled_rgba()` instead of reconstructing geometry from the original-resolution source mask.
- The reconstruction helper treats the upscale output as the sole geometry authority and fails closed on invalid surface contracts.
- Regression changes only the supplied upscale surface and requires reconstructed production geometry to change, proving the upscale result is behaviorally connected to reconstruction.
- Existing vector fidelity gates remain `IoU >= 0.995` and disagreement ratio `<= 0.005`; no quality, provenance, sanitizer or API/CLI threshold was weakened.
- U6 merged with expected-head protection from exact HEAD `453e40a614455e29091594e3c3656ac73a19a992`; merge commit `c599d3df8395e3305fcdd64b9c390b97dd7755af`.

### U7 — complete

- Production upscale reconstruction invokes `recover_curves_certified()` instead of unconditional polygon-only fitting.
- Cubic candidates are accepted only through the existing `certify_svg_scene()` fidelity gate; candidate radius may decrease deterministically, but certification thresholds remain unchanged.
- If no cubic candidate passes, the exact polygon remains authoritative, preserving fail-closed fidelity behavior.
- The production CLI regression executes a real TIFF fixture through `--certified-convert ... svg`, requires emitted `<path>` geometry to contain a cubic `C` command, and requires the quality-certificate artifact to remain present.
- U7 merged after exact-head green CI with expected-head protection; merge commit `25fa97604196a806d2fbfd2a1f3b4df935410074`.
- Existing vector fidelity gates remain `IoU >= 0.995` and disagreement ratio `<= 0.005`; provenance, sanitizer, API/CLI contracts and acceptance thresholds are unchanged.

### U8 — implementation complete; README CI pending

- Final serialized SVG evidence now measures component IoU and exposes immutable production gates: component IoU `>= 0.95`, visible residual ratio `<= 0.01`, and boundary p95 `<= 0.75 px`.
- The real `--certified-convert ... svg` production path enforces `passes_u8_production_gates()` fail-closed before certificate issuance.
- Source-space paint is rebound from the decoded raster while geometry remains derived from the actual upscale reconstruction, preserving U6 geometry provenance and reducing interpolation-only paint residual.
- The eight-fixture production regression pack executes eight representative user-style logo/mark cases through the real CLI, requires SVG and quality-certificate artifacts, and reruns each fixture to prove deterministic output.
- The U8 regression is wired into CTest and Linux ASan/UBSan. Exact-head `core-ci #403` passed source hygiene, Ubuntu/macOS/Windows build-test, CLI contracts, packaged/smoke checks, and Linux sanitizers at implementation HEAD `c8863cb3dd24d27d35c9da5a1f6e8988d2d66138`.
- Existing vector fidelity gates remain `IoU >= 0.995` and disagreement ratio `<= 0.005`; U8 did not weaken provenance, sanitizer behavior, API/CLI contracts or prior acceptance gates.
- Blocking review thread count was zero before this README status update. This README commit must receive fresh exact-head green CI before U8 can merge.

## Verified corrective milestones R1-R6

### R1 — complete

- True Release builds are exercised on Linux, macOS and Windows with warnings-as-errors retained.
- Cross-platform CTest and Linux sanitizer coverage remain required acceptance gates.

### R2 — complete

- Bounded real raster ingestion recognizes PNG/JPEG/WebP/TIFF by content and normalizes accepted inputs to canonical RGBA8.
- CLI `--convert INPUT OUTPUT` is exercised across PNG/JPEG/WebP/TIFF and emits deterministic canonical PAM RGBA8 output.

### R3 — complete

- The separable resampler applies production local-range clamping to the final reconstructed sample against the original 2D contributing source neighborhood.
- Fixture quality gates require smooth-RGB round-trip PSNR ≥ 35 dB and SSIM ≥ 0.98.
- Premultiplied-alpha and transpose-invariance regressions guard edge leakage and axis bias.

### R4 — complete

- Dense reconstructed contours can be simplified deterministically and fitted to cubic Bézier geometry.
- Node reduction is accepted only when the independently rasterized curved candidate passes `IoU ≥ 0.995` and disagreement ratio `≤ 0.005`.
- Failed curve candidates fall back to the exact polygon.

### R5 — complete

- SVG, PDF, EPS and DXF geometry export is driven by reconstructed `SvgScene` paths.
- Linear and cubic geometry plus required fill semantics are preserved behind provenance and structural validation contracts.

### R6 — complete

- CLI `--certified-convert INPUT OUTPUT FORMAT` executes real raster input through decode, analysis, upscale/vector reconstruction, export, measured quality/performance evidence and provenance-bound quality-certificate issuance.
- Fidelity certification retains `IoU ≥ 0.995` and disagreement ratio `<= 0.005`.
- The later real-user audit is the authority for U1-U8 and supersedes any earlier broad product-readiness claim.

## Verified contract milestones

- Stage 11 established immutable quality/provenance certification gates.
- Stage 12 established deterministic Core API and CLI contract behavior.
- Stage 13 established deterministic release/package identity, substitution rejection, hostile-path/residue rejection and staged CLI contract verification across Ubuntu, Windows and macOS.

These milestones remain prerequisites, not substitutes, for working visual conversion quality.

## Current corrective priority

1. Obtain fresh exact-head green CI evidence for this U8 README status commit.
2. Re-verify `mergeable=true` and zero unresolved blocking review threads.
3. Merge U8 only with expected-head protection after all U8 acceptance evidence remains green.
4. After canonical merged-state verification, the post-R6 U1-U8 corrective roadmap is complete and its monitoring automation can be disabled.

UI, account and subscription work remains deferred until the post-R6 real-user corrective program reaches its required acceptance state.
