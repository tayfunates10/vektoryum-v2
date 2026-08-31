# Vektoryum v2

Contract/infrastructure roadmap completion: **100%**

Functional end-user product readiness: **corrective R2 complete; R3 is next**.

The Stage 0-13 contract roadmap is complete, but that does **not** mean the end-user raster-to-vector/upscale product is complete. Corrective R1 and R2 now establish real Release builds plus bounded real PNG/JPEG/WebP/TIFF ingestion, canonical color/alpha normalization and deterministic CLI `--convert` coverage. Product readiness will return to 100% only after the remaining R3-R6 acceptance gaps are implemented and independently tested: resampler quality/root-cause correction, curve recovery, geometry-backed SVG/PDF/EPS/DXF export, and one complete real-input end-to-end certified conversion chain.

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
| R3 | Resampler artifact/root-cause fix plus fixture PSNR/SSIM and visual regression gates | pending |
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
- R2 acceptance was green on exact HEAD `4d819dd83071038df40ae5c2dea912c41adbb34e` with `core-ci #288`; the README update itself must receive a fresh exact-head green run before merge.

## Verified contract milestones

- Stage 11 established immutable quality/provenance certification gates.
- Stage 12 established deterministic Core API and CLI contract behavior.
- Stage 13 established deterministic release/package identity, substitution rejection, hostile-path/residue rejection and staged CLI contract verification across Ubuntu, Windows and macOS.

These milestones remain valuable and are not being weakened. They are prerequisites, not substitutes, for a working visual conversion product.

## Current corrective priority

1. Fix measured logo/resampler artifacts at their algorithmic root cause and add objective PSNR/SSIM/visual regression gates.
2. Recover real curves with bounded node complexity.
3. Connect all four exporters to real scene geometry and validate generated files.
4. Certify one complete real-user conversion chain end to end.

UI, account and subscription work remains deferred until functional product readiness reaches 100%.
