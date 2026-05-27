# SvgDecoder int32_t Feasibility Analysis

Date: 2026-05-11

## Goal
Assess which parts of SvgDecoder can be moved from float to int32_t without breaking rendering correctness.

Scope analyzed:
- components/pictures/src/svg_decoder.hpp
- components/pictures/src/svg_decoder.cpp

## Executive Summary
Direct replacement of float with int32_t is only safe in a limited subset of the current code.

Short answer:
1. Safe and useful int32_t conversions exist mainly in final raster-space loops and some sampling arithmetic.
2. Most geometry parsing, transforms, path flattening, and antialias coverage math should remain float.
3. If you want larger float reduction, fixed-point (for example Q16.16) is a better strategy than raw int32_t replacement.

## Current float-heavy zones
Major float usage zones are in:
1. Coordinate transforms and mapping:
- mapCoordX/mapCoordY/mapLenX/mapLenY
- Transform matrix and viewport values in PaintState and SvgDecoder state

2. Geometry parsing and path construction:
- parseLength/parsePathNumber/parseTransform
- flattenQuadratic/flattenCubic
- renderPath arc handling and winding tests

3. Antialiasing and coverage:
- drawLine Wu AA and round caps
- renderRect SSAA coverage
- renderCircle/renderEllipse coverage math
- fillPolygon antialias branch

4. Text positioning:
- renderText anchor/baseline/pixelScale computations

## Conversion candidates by feasibility

### A) Safe now (low risk)
These can be converted to int32_t with minimal behavior risk.

1. Raster loop sample offsets in fixed grids
- In renderRect SSAA and fillPolygon 2x2/4x4 sample offsets currently use float literals.
- You can precompute integer subpixel offsets with a fixed denominator (for example 4 or 16) and do integer arithmetic.
- Benefit: removes many float adds/divides in tight loops.
- Risk: very low if equivalent sampling points are preserved.

2. Bounding-box expansion constants
- Patterns like floor(x - r - 1.0), ceil(x + r + 1.0) can be handled via integer helper utilities after one final float-to-int conversion.
- Benefit: minor cleanup and fewer repeated float ops.
- Risk: low.

3. Marker triangle integerization after final transform
- In renderLine/renderPath marker drawing, orientation is float-based (must remain), but post-computed vertices are already rounded to int32_t.
- Additional intermediate values in final stage can be represented as int32_t once angle math is done.
- Benefit: minor.
- Risk: low.

### B) Conditional (medium risk, needs careful validation)
These are possible, but require precision and regression checks.

1. drawLine thin AA path
- Current Wu implementation is float-based.
- Could be replaced by integer/fixed-point Wu variant.
- Raw int32_t replacement is not enough; needs fixed-point fractional part handling.
- Benefit: substantial float reduction in line-heavy SVGs.
- Risk: medium-high visual drift (aliasing/weight changes).

2. Circle and ellipse distance tests
- Current code uses sqrt and normalized float coverage.
- Could be reworked to squared-distance integer approximations plus lookup/edge ramps.
- Benefit: medium.
- Risk: medium-high edge quality regression.

3. Path fill winding/intersection
- insideAt uses float intersections for robust edge crossing.
- Converting to integer can work with fixed-point edge functions, not plain ints.
- Benefit: medium on complex paths.
- Risk: medium for corner/collinear edge cases.

### C) Not recommended for int32_t replacement
These should stay float (or move to fixed-point if you want a bigger redesign).

1. Transform and viewport model
- PaintState::Transform a..f and mapCoord/mapLen pipeline.
- SVG transform semantics depend on non-integer values (scale, rotate, skew, translate with fractions).
- Integer-only replacement would materially reduce fidelity.

2. Parsing numerical attributes
- parseLength, parsePathNumber, parseTransformNumbers
- SVG content commonly uses decimals and percentages; int32_t parse would be incorrect.

3. Path construction and flattening in user space
- flattenQuadratic/flattenCubic and arcTo are inherently fractional geometry.
- Integer-only here creates visible shape distortion unless redesigned in fixed-point.

4. Text anchor/baseline and glyph advance accumulation
- renderText uses fractional pen progression and baseline alignment.
- int32_t-only would degrade spacing and alignment.

## Practical reduction plan (if objective is fewer float ops)

### Phase 1: Low-risk wins
1. Introduce integer subpixel helpers for SSAA sample loops (renderRect, fillPolygon).
2. Consolidate float-to-int boundary conversions with helper functions to avoid repeated floor/ceil patterns.
3. Keep geometry/model layer float, only optimize raster inner loops.

Expected outcome:
- Small but safe CPU reduction in per-pixel hot paths.

### Phase 2: Targeted fixed-point upgrades
1. Convert Wu line math to fixed-point.
2. Convert circle/ellipse edge ramps to fixed-point approximations.

Expected outcome:
- Larger float reduction while preserving quality if calibrated.

### Phase 3: Optional deep redesign
1. Fixed-point transform pipeline (Q16.16).
2. Fixed-point path flatten and winding.

Expected outcome:
- Maximum float reduction, highest implementation and regression cost.

## Concrete answer to the question
Yes, some portions can be transformed to use int32_t, but mostly in raster-space helper arithmetic and loop internals. The core SVG geometry, transforms, and AA math should not be directly converted to plain int32_t if you want to preserve output quality.

For larger gains on no-FPU targets, use fixed-point in selected hot paths rather than replacing float with int32_t wholesale.
