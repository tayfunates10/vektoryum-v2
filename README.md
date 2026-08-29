# Vektoryum v2

Core Engine progress: **18%**

Completed: Stages 0, 1, 2.

Active: **Stage 3 — high-quality raster resampler**.

Workflow: branch → pull request → CI → merge only after all required checks pass.

## Roadmap

| Stage | Scope | Weight | Status |
|---|---|---:|---|
| 0 | Specification and roadmap | 4% | done |
| 1 | Repository foundation and CI | 6% | done |
| 2 | Image Core | 8% | done |
| 3 | Raster resampler | 8% | active |
| 4 | Content analyzer/router | 5% | planned |
| 5 | Vector reconstruction | 12% | planned |
| 6 | Photo restoration and non-ML SR | 12% | planned |
| 7 | Vektoryum ML/DL model/runtime | 14% | planned |
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

Next: implement Stage 3 with deterministic 2x/4x/8x raster scaling and quality tests.
