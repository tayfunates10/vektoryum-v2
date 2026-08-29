# Stage 3 — Analytic Raster Resampler

## Scope

This stage implements Vektoryum's own deterministic scalar CPU resampler. It does not call OpenCV, ImageMagick, PIL, libvips, a super-resolution package, or an external interpolation engine.

## Sampling contract

- Pixel centers use half-pixel mapping: `(dst + 0.5) / scale - 0.5`.
- Resampling is separable: horizontal pass, then vertical pass.
- Default reconstruction kernel is Lanczos-3.
- Bicubic (Catmull-Rom) and bilinear kernels exist as deterministic conservative alternatives.
- Downscaling widens kernel support by the inverse scale to act as an anti-alias low-pass filter.
- Kernel weights are normalized per destination sample so constant fields remain constant.
- Border addressing clamps to the nearest valid source sample.
- Identity resize has an exact-copy fast path.

## Ringing policy

Lanczos has negative lobes and can overshoot around hard edges. The default production option therefore clamps each reconstructed sample to the local contributing-sample range. This is not a global `[0,1]` clamp: HDR/extended linear values remain possible when present in the source, while newly introduced extrema are blocked.

Callers may disable the local-range clamp only for controlled analysis/benchmark work. Production routing will keep it enabled unless a later quality evaluator proves another policy superior.

## Color and alpha boundary

The resampler operates on linear-light float samples. Alpha-bearing images must be premultiplied before interpolation and may be unpremultiplied after reconstruction. This prevents transparent pixels from leaking hidden RGB into visible edge pixels.

## Stage acceptance gates

- identity resize is bit-exact
- 2×, 4× and 8× dimensions are exact
- constant fields are invariant
- hard 0→1 edges create no new under/overshoot with the production clamp
- linear ramp interior reconstruction error stays within the committed regression bound
- high-frequency checkerboard downscale converges close to its 0.5 mean and suppresses alias contrast
- same input/options produce bit-identical output in repeated runs on the same build
- malformed sample counts, zero targets and invalid channel counts are rejected
- Ubuntu, Windows, macOS and Linux ASan/UBSan CI all pass

## Deferred to later stages

This stage is an analytic reconstruction baseline, not the final photographic super-resolution solution. Edge-directed restoration, learned priors, content-aware routing, tile execution, GPU backends and the final quality evaluator are implemented in later roadmap stages without weakening these baseline invariants.
