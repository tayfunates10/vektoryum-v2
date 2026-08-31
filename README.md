# Vektoryum v2

Contract/infrastructure roadmap completion: **100%**

Functional end-user product readiness: **corrective R3 complete; R4 is next**.

The Stage 0-13 contract roadmap is complete, but that does **not** mean the end-user raster-to-vector/upscale product is complete. Corrective R1-R3 now establish real Release builds, bounded real PNG/JPEG/WebP/TIFF ingestion, canonical color/alpha normalization, deterministic CLI `--convert` coverage, and a measured resampler artifact correction guarded by PSNR/SSIM and visual-regression acceptance tests. Product readiness will return to 100% only after the remaining R4-R6 acceptance gaps are implemented and independently tested: curve recovery, geometry-backed SVG/PDF/EPS/DXF export, and one complete real-input end-to-end certified conversion chain.

Workflow: branch → pull request → CI → merge only after all required checks pass.

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

## Product-readiness corrective roadmap

The post-roadmap visual audit exposed gaps that the contract-focused acceptance suite did not prove. The corrective program is therefore tracked separately from the completed contract roadmap.

| Corrective stage | Acceptance goal | Status |
|---|---|---|
| R1 | Real Release builds on Linux/macOS/Windows with warnings-as-errors and meaningful tests | complete |
| R2 | PNG/JPEG/WebP/TIFF decode, color/alpha handling and CLI `convert` | complete |
| R3 | Resampler artifact/root-cause fix plus fixture PSNR/SSIM and visual regression gates | complete |
| R4 | Curve recovery as cubic Bézier/arc with fidelity-bounded node reduction | pending |
| R5 | Real scene-backed SVG/PDF/EPS/DXF encoders validated by standard readers | pending |
| R6 | Real input → analysis → upscale/vector → export → quality certificate end-to-end acceptance | pending |

## Verified corrective milestones

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
- R3 implementation acceptance is green on exact HEAD `532483811d8cc33c454b8115fdf79e36d234435a` with `core-ci #294`. This README status commit must itself receive a fresh exact-head green run before R3 is merged.

## Verified contract milestones

- Stage 11 established immutable quality/provenance certification gates.
- Stage 12 established deterministic Core API and CLI contract behavior.
- Stage 13 established deterministic release/package identity, substitution rejection, hostile-path/residue rejection and staged CLI contract verification across Ubuntu, Windows and macOS.

These milestones remain valuable and are not being weakened. They are prerequisites, not substitutes, for a working visual conversion product.

## Current corrective priority

1. Recover real curves as cubic Bézier/arc geometry while reducing node count only under fidelity gates.
2. Connect all four exporters to real scene geometry and validate generated files with standard readers.
3. Certify one complete real-user conversion chain end to end.

UI, account and subscription work remains deferred until functional product readiness reaches 100%.
