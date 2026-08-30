# Vektoryum v2

Contract/infrastructure roadmap completion: **100%**

Functional end-user product readiness: **35%** (30 August 2026 local visual audit baseline).

The Stage 0-13 contract roadmap is complete, but that does **not** mean the end-user raster-to-vector/upscale product is complete. The current CLI still lacks a real image `convert` command, image decoding/input loading, production geometry-backed SVG/PDF/EPS/DXF output, curve recovery quality and a complete real-input end-to-end conversion chain. Product readiness will return to 100% only after those acceptance gaps are implemented and independently tested.

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
| R1 | Real Release builds on Linux/macOS/Windows with warnings-as-errors and meaningful tests | active |
| R2 | PNG/JPEG/WebP/TIFF decode, color/alpha handling and CLI `convert` | pending |
| R3 | Resampler artifact/root-cause fix plus fixture PSNR/SSIM and visual regression gates | pending |
| R4 | Curve recovery as cubic Bézier/arc with fidelity-bounded node reduction | pending |
| R5 | Real scene-backed SVG/PDF/EPS/DXF encoders validated by standard readers | pending |
| R6 | Real input → analysis → upscale/vector → export → quality certificate end-to-end acceptance | pending |

## Verified contract milestones

- Stage 11 established immutable quality/provenance certification gates.
- Stage 12 established deterministic Core API and CLI contract behavior.
- Stage 13 established deterministic release/package identity, substitution rejection, hostile-path/residue rejection and staged CLI contract verification across Ubuntu, Windows and macOS.

These milestones remain valuable and are not being weakened. They are prerequisites, not substitutes, for a working visual conversion product.

## Current corrective priority

1. Make CI build true Release binaries on Unix runners and fix Release-only failures without suppressing warnings.
2. Add real image ingestion and a `convert` command.
3. Fix measured logo/resampler artifacts and add objective quality regression gates.
4. Recover real curves with bounded node complexity.
5. Connect all four exporters to real scene geometry and validate generated files.
6. Certify one complete real-user conversion chain end to end.

UI, account and subscription work remains deferred until functional product readiness reaches 100%.
