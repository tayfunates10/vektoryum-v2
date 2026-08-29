# Image Core Contracts

Stage 2 defines the low-level invariants every later Vektoryum pipeline must obey.

## Pixel and image specification

`core::ImageSpec` records dimensions, pixel layout, channel type, transfer function, color primaries and alpha mode explicitly. No processing stage may infer alpha semantics or bit depth from buffer size alone.

Supported foundation channel representations:

- unsigned 8-bit,
- unsigned 16-bit,
- 32-bit floating point.

Supported foundation layouts:

- Gray,
- Gray + Alpha,
- RGB,
- RGBA.

The initial color-primary contract is sRGB. The type system already separates transfer function from primaries so wider-gamut support can be added without conflating the two concepts.

## Alpha invariant

Layouts without an alpha channel must use `AlphaMode::None`. Layouts with alpha must state either `Straight` or `Premultiplied`.

Filtering and reconstruction stages that mix neighboring pixels are expected to operate in linear-light, premultiplied-alpha form unless a later algorithm documents a different mathematically justified representation. This prevents transparent-edge RGB contamination and dark/bright halos.

A fully transparent premultiplied pixel unpremultiplies to transparent black. Hidden RGB under zero alpha is intentionally discarded at this boundary rather than allowed to leak into later interpolation.

## Color transfer invariant

sRGB encode/decode functions are explicit and separate. Later resamplers must not interpolate gamma-encoded RGB when a linear-light operation is required.

## Safe byte sizing

Decoded buffer size is computed with checked multiplication. Overflow is an error, never wraparound. Invalid alpha/layout combinations and zero dimensions are rejected before allocation.

## Decode resource limits

`io::DecodeLimits` is a pre-allocation boundary for hostile or accidentally huge inputs. It enforces:

- maximum width,
- maximum height,
- maximum pixel count,
- maximum decoded byte count.

File-format-specific decoders added later must validate the prospective decoded `ImageSpec` against this contract before allocating the full pixel buffer.

## Tile contract

`core::plan_tiles()` splits an image into non-overlapping **core** rectangles and overlap-expanded **processing** rectangles.

Required invariants:

1. Core rectangles cover the image exactly once with no gaps.
2. Expanded rectangles are clipped to image bounds.
3. Every expanded rectangle contains its core rectangle.
4. Neighboring expanded rectangles overlap when overlap is non-zero, providing context for seam-safe processing.
5. Tile count is resource-bounded.
6. Tiny images remain a single bounded tile.

The overlap does not by itself guarantee seam-free output. Later tiled algorithms must crop/blend only the valid core region or use a documented weighting policy. Stage 2 guarantees the geometry needed to do that safely.

## Stage 2 acceptance evidence

The unit suite covers:

- bit-depth/channel invariants,
- invalid alpha/layout combinations,
- exact byte sizing and overflow rejection,
- sRGB transfer round-trip,
- straight ↔ premultiplied alpha round-trip,
- zero-alpha RGB leak prevention,
- tile coverage and overlap geometry,
- tile resource bounds,
- decode dimension/pixel/byte budgets.

Cross-platform CI and sanitizers from Stage 1 remain mandatory for this stage.
