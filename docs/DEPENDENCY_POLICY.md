# Dependency & Model Provenance Policy

## Goal

Vektoryum v2 must own the algorithms that create its product value. We do not outsource super-resolution, restoration or vector reconstruction to a hidden third-party engine.

## Production algorithm restrictions

The production core must not embed, wrap, shell out to, download or remotely call a ready-made:

- super-resolution engine,
- image restoration engine,
- raster-to-vector engine,
- pretrained generative/upscaling model,
- proprietary remote image-enhancement API.

Vektoryum algorithms, model architecture, training objectives, inference contracts and production weights must be developed for this repository and have traceable provenance.

## Infrastructure dependencies

A dependency that is only infrastructure may be proposed when reimplementing it would add risk without differentiating the product. Examples include compiler/toolchain, operating-system APIs and build/CI tooling.

Any proposed runtime dependency must document:

1. purpose,
2. exact license,
3. whether it enters the production binary,
4. whether it processes pixels or geometry,
5. replacement/removal strategy,
6. security/update responsibility.

A dependency that performs Vektoryum's reconstruction/vectorization job is rejected regardless of license.

## Models and datasets

No externally trained weights may silently become Vektoryum production weights. Every production model must record:

- architecture revision,
- training code revision,
- dataset manifest/revision,
- degradation pipeline revision,
- seed/configuration,
- training/evaluation metrics,
- artifact checksum,
- license/rights provenance for training data.

## Test policy

Quality thresholds are product requirements, not knobs used to make CI green. A regression must be fixed in code/data/model behavior or explicitly investigated; lowering/removing the gate is not an acceptable bug fix.