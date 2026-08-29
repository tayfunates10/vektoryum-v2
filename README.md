# Vektoryum v2

Core Engine progress: **55%**

Completed: Stages 0, 1, 2, 3, 4, 5, 6.

Active: **Stage 7 — Vektoryum ML/DL model/runtime**.

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
| 7 | Vektoryum ML/DL model/runtime | 14% | active |
| 8 | Training/data/benchmark pipeline | 7% | planned |
| 9 | Hybrid reconstruction | 7% | planned |
| 10 | Exporters | 5% | planned |
| 11 | Quality/performance certification | 6% | planned |
| 12 | Stable CLI/Core API | 3% | planned |
| 13 | Release hardening | 3% | planned |

## Progress

- 4%: project specification and roadmap established.
- 10%: Stage 1 merged in PR #2 after required CI passed.
- 18%: Stage 2 merged in PR #3 after required CI passed.
- 26%: Stage 3 merged in PR #4 after source hygiene, cross-platform build/tests, sanitizers, deterministic 2x/4x/8x scaling, edge overshoot and checkerboard antialias gates passed.
- 31%: Stage 4 merged in PR #5 after deterministic content routing, mixed-vs-photo precedence, alpha-defined silhouette routing, cross-platform build/tests and ASan/UBSan all passed with zero unresolved review threads.
- 43%: Stage 5 merged in PR #6 after topology-safe raster-to-vector reconstruction, self-intersection and node-complexity safety, SVG-ready curve/path contracts, direct rasterize-back IoU/disagreement certification, adversarial topology regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.
- 55%: Stage 6 merged in PR #7 after deterministic bounded denoise/deblock/sharpen restoration, 2x/4x project-owned non-ML super-resolution, strict validation and output budgets, premultiplied-alpha-safe processing, anti-ringing/halo/overshoot regressions, cross-platform build/tests and ASan/UBSan all passed with zero unresolved blocking review threads.

Next: implement Stage 7 Vektoryum ML/DL model/runtime with deterministic runtime contracts, explicit model/version provenance, bounded resource behavior, safe fallback policy, testable tensor interfaces, and cross-platform regression coverage without weakening existing quality gates.
