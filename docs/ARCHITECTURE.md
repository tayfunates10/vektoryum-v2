# Vektoryum v2 Core Architecture

## Architectural principles

1. **Core-first:** UI, billing, storage service and account logic never enter the image-processing core.
2. **Deterministic by default:** deterministic algorithms and reproducible model execution are preferred; any nondeterministic backend must be explicit and testable.
3. **Content-aware routing:** photo, logo/line-art and mixed content use distinct pipelines.
4. **Quality authority is independent:** processing code cannot self-certify. Acceptance is performed by the quality layer.
5. **Safe fallback:** if an advanced reconstruction path lowers measured quality, the engine must be able to reject it and use a conservative result.
6. **No hidden raster-in-SVG success:** vector output must contain real geometry for vectorizable content.
7. **Bounded resources:** size, memory, tile count and processing limits must be explicit.

## Module boundaries

- `core`: primitive image/matrix/color/alpha/geometry types and invariants.
- `io`: codecs, metadata validation, decode/encode boundaries.
- `analysis`: features, content classification, confidence and routing.
- `resample`: deterministic analytical scaling/reconstruction algorithms.
- `restore`: denoise/deblur/decompression restoration stages.
- `vector`: segmentation, topology, contour extraction, curve fitting and scene graph.
- `ml`: Vektoryum-owned tensor/model/inference components and model contracts.
- `quality`: metrics, acceptance gates and regression authority.
- `export`: serialization of raster/vector results.
- `cli`: developer/headless entry point.
- `api`: stable integration facade used later by product layers.

## Processing contract

Every pipeline eventually returns a result plus structured evidence:

- input metadata,
- selected route and confidence,
- transforms applied,
- deterministic seed/configuration where applicable,
- output metadata,
- quality measurements,
- warnings/fallback reason,
- engine/model version.

The evidence contract will be introduced incrementally as the relevant modules land.

## Change policy

A module may depend only on lower-level contracts, not on UI/product code. Quality gates cannot be bypassed by an exporter or caller. A breaking core API change requires a versioned migration note once Stage 12 starts.
