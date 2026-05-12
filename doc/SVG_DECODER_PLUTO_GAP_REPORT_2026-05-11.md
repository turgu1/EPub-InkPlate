# SvgDecoder vs PlutoSVG Gap Analysis

Date: 2026-05-11

## 1) Scope and method
This report compares:
- Current project implementation: components/pictures/src/svg_decoder.cpp and components/pictures/src/svg_decoder.hpp
- Reference implementation: ~/Dev/pluto_svg/plutosvg.c and ~/Dev/pluto_svg/plutosvg.h

Goal:
- Identify what is still missing in the current SvgDecoder for a more complete SVG implementation.
- Prioritize missing pieces by impact and implementation effort.

Note:
- This is analysis only. No runtime code changes were made.

## 2) High-level conclusion
Current SvgDecoder is strong on grayscale raster output and already includes features PlutoSVG does not currently implement (notably text rendering and marker-like endpoint rendering heuristics).

However, compared to PlutoSVG, the biggest functional gaps are in paint servers and stroke styling:
- Missing gradient paint servers (linear/radial with stops and url references)
- Missing stroke dash and join controls
- Missing symbol/use reuse model and nested svg viewport handling
- Limited color/paint parsing compared to currentColor/url/var paint model

These are the key additions needed for a noticeably more complete SVG implementation.

## 3) Feature comparison summary

### 3.1 Geometry and shape tags
Current SvgDecoder supports:
- rect, line, circle, ellipse, path, polyline, polygon
- image (data uri png/jpeg)
- text and tspan-like runs

PlutoSVG supports:
- svg, g, use, symbol, defs
- rect, line, circle, ellipse, path, polyline, polygon
- image (data uri png/jpeg)
- No text pipeline in this implementation

Gap vs Pluto:
- use and symbol handling
- group and nested svg viewport semantics

### 3.2 Paint model
Current SvgDecoder supports:
- fill and stroke solid grayscale colors
- opacity, fill-opacity, stroke-opacity
- fill-rule for paths
- marker-start and marker-end heuristic drawing

PlutoSVG supports:
- solid colors plus currentColor
- paint url fallback model for gradients
- linearGradient and radialGradient with stop list resolution
- gradient inheritance via href chains
- spreadMethod, gradientUnits, gradientTransform

Gap vs Pluto:
- Full paint server support (url id lookup + gradient application)
- currentColor and paint var model
- stop-color and stop-opacity processing

### 3.3 Stroke styling
Current SvgDecoder supports:
- stroke-width
- stroke-linecap round handling (recently added)

PlutoSVG supports:
- stroke-linecap butt/round/square
- stroke-linejoin miter/round/bevel
- stroke-miterlimit
- stroke-dasharray and stroke-dashoffset

Gap vs Pluto:
- linejoin, miterlimit, dasharray, dashoffset
- square and butt distinction in caps

### 3.4 Coordinate systems and viewport behavior
Current SvgDecoder supports:
- viewBox and preserveAspectRatio on root svg
- basic transform parsing and composition

PlutoSVG supports:
- viewBox/preserveAspectRatio across svg and symbol usage context
- nested viewport transforms for svg and symbol
- use placement x/y with referenced element rendering

Gap vs Pluto:
- Nested viewport model for inner svg and symbol
- Reuse rendering flow for use href references

### 3.5 Styling and CSS
Current SvgDecoder supports:
- style attribute lookup by property key
- direct attribute fallback
- class style map structure exists but parseEmbeddedStyles is currently empty

PlutoSVG supports:
- style attribute parsing into normalized attribute entries
- inherited property lookup through element ancestry

Gap vs Pluto:
- Actual parse of embedded style blocks and class selectors
- Stronger style declaration parsing and inheritance consistency

### 3.6 Visibility and structural behavior
Current SvgDecoder supports:
- display none skip
- skips defs, marker, symbol, clipPath, mask containers entirely

PlutoSVG supports:
- display and visibility handling for renderable elements
- defs and symbol used as resource containers for use and paint server lookup

Gap vs Pluto:
- visibility hidden semantics
- Defs resource lookup model instead of hard skip-only behavior

## 4) Areas where current SvgDecoder is already ahead
Compared to the inspected PlutoSVG implementation, current SvgDecoder has:
- Built-in text rendering path with FreeType glyph rasterization and fallback glyph drawing
- Explicit UTF-8 and entity decoding in text pipeline
- Marker-like endpoint drawing logic for line/path endpoints
- Existing antialiasing tuning for several primitives in grayscale renderer

These should be preserved while filling the gaps below.

## 5) Priority roadmap for a more complete implementation

### P0 (highest impact)
1. Add paint server support for gradients
- Parse and store defs resources by id
- Implement url id paint resolution for fill and stroke
- Implement linearGradient and radialGradient with:
  - stop, stop-color, stop-opacity
  - gradientUnits
  - spreadMethod
  - gradientTransform
  - href chaining and fallback

2. Add full stroke style support
- stroke-linejoin (miter, round, bevel)
- stroke-miterlimit
- stroke-dasharray and stroke-dashoffset
- full stroke-linecap set (butt, round, square)

3. Add use and symbol reuse model
- Resolve href references in use
- Render referenced symbol or geometry with x/y and local viewport behavior
- Add cycle guard for nested or recursive use

4. Add visibility semantics
- Support visibility hidden and related inherited behavior

### P1 (important completeness)
1. Implement embedded style parsing
- Parse style blocks into class and selector map
- Apply class-driven declarations robustly

2. Improve color and paint parsing
- Add currentColor handling
- Add broader CSS color parsing consistency
- Add paint fallback parsing after url values

3. Add nested svg viewport mapping
- Apply x/y/width/height/viewBox/preserveAspectRatio on non-root svg elements

### P2 (longer tail)
1. Clip paths and clip-rule
- Pluto code has clip enums but marks clipPath tag as TODO
- Implementing this in current decoder improves real-world SVG compatibility beyond both implementations

2. Masks, patterns, filters
- Not present in either implementation snapshot
- Needed for high SVG fidelity but likely expensive for grayscale raster constraints

## 6) Suggested implementation sequence
1. Resource graph and id registry
- Build defs id lookup for gradients, symbols, and reusable elements.

2. Paint server integration
- Implement gradient objects first (largest visual impact).

3. Stroke model completion
- Dash/join/miter/cap improvements next.

4. use and nested svg semantics
- Reuse model and viewport correctness.

5. Styling hardening
- Embedded styles and stronger inheritance rules.

6. Advanced effects
- Clip paths, then masks and filters if needed.

## 7) Risk and complexity notes
- Gradient and use support require introducing a stable element resource model, not just direct recursive drawing.
- Dash and join behavior needs geometry-aware stroking consistency to avoid regressions in antialias quality.
- Embedded style support can become expensive if full CSS is targeted; start with class and simple selectors used in your fixtures.

## 8) Practical recommendation
For the next iteration aimed at visible compatibility gains, implement only this minimal set first:
- url paint plus linearGradient/radialGradient stops
- stroke dash/join/miter support
- use plus symbol resolution

This gives the largest jump in real SVG compatibility while preserving your current grayscale and text pipeline strengths.
