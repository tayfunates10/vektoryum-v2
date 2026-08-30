# Vektoryum v2

Core Engine progress: **97%**

Completed: Stages 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12.

Active: **Stage 13 — Release hardening**.

Workflow: branch → pull request → CI → merge only after all required checks pass.

## Roadmap

| Stage | Scope | Weight | Status |
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
| 10 | Exporters | 5% | done |
| 11 | Quality/performance certification | 6% | done |
| 12 | Stable CLI/Core API | 3% | done |
| 13 | Release hardening | 3% | active |

## Progress

- 4%: project specification and roadmap established.
- 10%: Stage 1 merged in PR #2 after required CI passed.
- 18%: Stage 2 merged in PR #3 after required CI passed.
- 26%: Stage 3 merged in PR #4 after source hygiene, cross-platform build/tests, sanitizers, deterministic 2x/4x/8x scaling, edge overshoot and checkerboard antialias gates passed.
- 31%: Stage 4 merged in PR #5 after deterministic content routing, mixed-vs-photo precedence, alpha-defined silhouette routing, cross-platform build/tests and ASan/UBSan all passed with zero unresolved review threads.
- 43%: Stage 5 merged in PR #6 after topology-safe raster-to-vector reconstruction, self-intersection and node-complexity safety, SVG-ready curve/path contracts, direct rasterize-back IoU/disagreement certification, adversarial topology regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 55%: Stage 6 merged in PR #7 after deterministic bounded denoise/deblock/sharpen restoration, 2x/4x project-owned non-ML super-resolution, strict validation and output budgets, premultiplied-alpha-safe processing, anti-ringing/halo/overshoot regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 69%: Stage 7 merged in PR #8 after deterministic project-owned ML/DL runtime contracts, byte-verified SHA-256 model provenance, owned bounded artifact loading, strict tensor/preprocess/postprocess interfaces, backend/provider isolation, no-silent-fallback enforcement, deterministic reference execution, adversarial invalid-model/input regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 76%: Stage 8 merged in PR #9 after deterministic dataset manifests/splits, byte-verified sample provenance, fail-closed license/rights authorization and digest-to-rights binding, leakage prevention, deterministic training/model provenance, benchmark metric/result/artifact provenance, canonical reports, bounded resources, adversarial regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 83%: Stage 9 merged in PR #11 after deterministic hybrid planning/routing, immutable vector/raster source provenance, bounded fusion resources, alpha-safe composition, seam/topology protection, no silent raster fallback, deterministic canonical output provenance, adversarial mixed-content execution coverage, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 88%: Stage 10 merged in PR #12 after deterministic SVG/PDF/EPS/DXF request, artifact and encoder contracts; exact hybrid-output and output-digest provenance; bounded dimensions/output/intermediate/execution resources; format structural validation; deterministic identical-input bytes; fail-closed destination/metadata safety; adversarial end-to-end exporter fixtures; cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 94%: Stage 11 merged in PR #13 after exact Stage 10 artifact binding, deterministic certificate bytes/SHA-256 evidence, measured bounded export resource/performance evidence, canonical alpha MAE/vector IoU quality fixtures, exact immutable threshold boundaries, SVG/PDF/EPS/DXF end-to-end repeatability, adversarial provenance/tamper/threshold regressions, cross-platform build/tests and Linux ASan/UBSan all passed with zero unresolved blocking review threads.
- 97%: Stage 12 merged in PR #14 after versioned deterministic Core API request/response contracts, stable process exit semantics, bounded public resource accounting, exact Stage 11 certificate binding, deterministic certified execution, byte-exact adversarial CLI streams, issuance-to-CLI provenance handoff, cross-platform build/tests and Linux ASan/UBSan all passed with zero unresolved blocking review threads.

Next: complete Stage 13 Release hardening with deterministic release identity, allow-listed reproducible packaging, exact package/manifest provenance, hostile-path and residue rejection, packaged CLI contract verification and final fresh exact-head cross-platform acceptance without weakening any Stage 0-12 gate.
