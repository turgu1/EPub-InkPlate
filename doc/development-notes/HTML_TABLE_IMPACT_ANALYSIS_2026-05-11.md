# HTML Table Parsing and Rendering Impact Analysis

Date: 2026-05-11

## Scope
Requested analysis target:
- HTMLInterpreter
- Page
- CSS
- DOM
- Screen

Additional impacted areas are included where the current architecture requires them.

This document is analysis only. No implementation was performed.

## Executive Summary
Adding HTML <table> support requires a non-trivial extension of the current pipeline because the existing renderer is optimized for inline/block text flow and pictures, not 2D grid layout.

Most important findings:
1. Tag model does not include table tags today.
2. CSS model and parser do not support table display semantics beyond inline/block/inline-block.
3. Page layout is line-oriented and paragraph-oriented; there is no table layout phase (column width resolution, row height resolution, cell spanning, border conflict resolution).
4. Screen primitives are sufficient for basic borders/background fills, but a higher-level draw plan for table grid painting is missing.

Recommendation:
- Introduce a dedicated table layout stage (logical table model + measurement + placement) integrated into Page, with HTMLInterpreter building a structured table subtree and CSS extending display/property parsing.

## Current Architecture Snapshot

### 1) DOM tag model and HTML parsing
- Current tag enum and lookup map do not contain table-related tags.
- HTMLInterpreter currently maps known tags to inline/block behavior and text/image handling.

Evidence:
- src/models/dom.hpp (DOM::Tag)
- src/models/dom.cpp (DOM::tags map)
- src/viewers/html_interpreter.cpp (switch over DOM::Tag and block/inline paragraph behavior)

### 2) CSS property/display model
- Display currently supports: NONE, INLINE, BLOCK, INLINE_BLOCK.
- CSS parser maps display to only these values.
- No dedicated properties for table layout behavior are actively modeled in PropertyId (aside from a comment mention of table-layout in docs text).

Evidence:
- src/models/css.hpp (Display enum, PropertyId enum)
- src/models/css.cpp (property map)
- src/models/css_parser.hpp (display decoding)

### 3) Page layout/rendering
- Page is optimized for text flow:
  - addWord/addChar
  - newParagraph/endParagraph/lineBreak
  - line list + display list
- Format contains margins/width/height/font parameters but no table/cell/grid context.
- Rendering commands are glyph/picture and simple primitive helpers; no table command abstraction.

Evidence:
- src/viewers/page.hpp (Format, methods)
- src/viewers/page.cpp (paragraph pipeline, addWord/addChar, adjustFormatFromRules)

### 4) Screen primitives
- Screen has primitives to draw glyphs, pictures, rectangle/round rectangle, and fill region.
- Enough for baseline borders/background rendering, but no dedicated grid/border-collapse helper APIs.

Evidence:
- lib_linux/EPub_InkPlate/src/screen.hpp
- lib_linux/EPub_InkPlate/src/screen.cpp

## Gap Analysis for Table Support

## A) Parsing and DOM gaps
Missing tag coverage:
- table
- thead
- tbody
- tfoot
- tr
- th
- td
- caption
- colgroup
- col

Impact:
- DOM::Tag and DOM::tags must be extended.
- HTMLInterpreter recursion currently treats all content as sequential inline/block flow. Table-specific subtree capture/processing is needed.

Design implication:
- Either:
  1. Build full table subtree in DOM and let Page perform table layout, or
  2. Build an intermediate table model directly in HTMLInterpreter and emit positioned content to Page.
- Option 1 is more maintainable and consistent with existing CSS matching over DOM.

## B) CSS semantics gaps
Missing display semantics:
- table
- inline-table
- table-row-group / table-header-group / table-footer-group
- table-row
- table-cell
- table-caption
- table-column-group / table-column

Likely missing/partial properties relevant for tables:
- border-collapse
- border-spacing
- table-layout
- caption-side
- vertical-align behavior for table-cell context
- min-content/max-content style constraints (if desired later)

Impact:
- Extend Display enum and parser decoding.
- Add PropertyId entries and parsing for table-related properties.
- Extend adjustFormatFromRules semantics; may require a table-specific style structure beyond Page::Format.

## C) Layout engine gaps (largest impact)
Current layout is linear text flow; tables need 2D layout phases:
1. Table width resolution (available width, explicit width, percentage handling)
2. Column count and column width resolution
3. Row formation and cell assignment
4. Cell content measurement (intrinsic minimum/preferred widths, multi-line height)
5. Row height resolution from max cell heights
6. Cell content placement with padding/alignment
7. Border/background painting order
8. Pagination strategy when table exceeds remaining page height

Impact on Page:
- New data structures are needed, for example:
  - TableLayout, RowLayout, CellLayout
  - per-cell content fragments
- New APIs likely needed, for example:
  - beginTable / addCellContent / finalizeTable / emitTable
- Existing addWord/addChar pipeline alone is insufficient for robust table rendering.

## D) Rendering model gaps
Even with layout computed, renderer needs a robust draw plan:
- Cell backgrounds
- Border drawing (including shared border rules if border-collapse is implemented)
- Text clipping/wrapping in cell box
- Vertical alignment within cells

Screen impact:
- Existing drawRectangle/colorizeRegion can support basic grid.
- For higher quality and performance, additional helpers may be useful:
  - drawHorizontalLine / drawVerticalLine with thickness
  - clipped text drawing region (or a temporary viewport discipline in Page)

## E) Pagination and navigation impact
Tables strongly affect pagination:
- A row may not fit remaining page space.
- Need policy for splitting:
  - split by row only (safer first milestone)
  - split inside a cell (advanced)
- Header row repetition across page breaks (thead behavior) may be required for readability.

Impact on page offset logic:
- HTMLInterpreter currently advances currentOffset through streamed content.
- Table preprocessing can buffer more content before emitting, affecting offset-to-visual mapping and page location reproducibility.

Potentially impacted modules beyond requested list:
- src/models/page_locs_retriever.cpp (location mode assumptions)
- src/models/page_locs.cpp and toc/page location consistency when layout algorithm changes

## Detailed Class-Level Impact

### HTMLInterpreter
Current role:
- Streams XML nodes recursively and emits text/images into Page.

Required changes:
1. Detect table subtree root and switch from streaming mode to table-collection mode.
2. Build structured representation for rows/cells and nested inline markup.
3. Coordinate with Page table layout API.
4. Maintain deterministic offset accounting while buffering table content.

Risk:
- Existing pageEnd/startOffset/endOffset behavior can regress if buffering changes offset timing.

### DOM
Current role:
- Lightweight hierarchical tag tree with class/id and parent/sibling relations.

Required changes:
1. Extend DOM::Tag with table-related tags.
2. Extend DOM::tags map with corresponding string mappings.
3. Optional: add lightweight node flags/metadata for table semantics if needed for faster matching.

Risk:
- Low technical risk, but required foundation for CSS matching and parser routing.

### CSS
Current role:
- Parses CSS selectors/properties and matches against DOM nodes.

Required changes:
1. Extend Display enum and parser for table display keywords.
2. Add PropertyId entries and parsing/value decoding for table properties.
3. Ensure selector matching works for newly introduced tags (automatic once tags exist).
4. Define inheritance/default behavior for table properties.

Risk:
- Medium: parser and value handling complexity can grow quickly.

### Page
Current role:
- Text/paragraph-oriented layout and display list generation.

Required changes:
1. Add table layout data structures and measurement logic.
2. Add cell text layout path (reuse existing word wrap where possible, but within cell constraints).
3. Add rendering emission for backgrounds/borders/cell contents.
4. Integrate pagination policy for row overflow.
5. Ensure ComputeMode LOCATION/MOVE/DISPLAY behavior remains consistent and deterministic.

Risk:
- High: this is the main algorithmic and integration hotspot.

### Screen
Current role:
- Final raster draw operations.

Required changes:
1. No mandatory API changes for minimal viable table rendering (rectangles + glyphs are enough).
2. Optional quality/performance additions for line drawing and clipping convenience.

Risk:
- Low for minimal feature set; medium if advanced border models are added.

## Suggested Incremental Delivery Plan

### Phase 1: Structural Table Support (MVP)
- Parse table/tbody/tr/td/th/caption tags.
- CSS display parsing for table/table-row/table-cell.
- Simple fixed algorithm:
  - equal-width columns
  - no colspan/rowspan
  - separate borders only
  - row-level pagination (no cell split)
- Render borders/background and wrapped text in cells.

Expected impact:
- Delivers readable tables for many EPUBs with low/medium complexity.

### Phase 2: CSS and Layout Fidelity
- Add border-collapse/border-spacing/table-layout support.
- Add column width from CSS width and content hints.
- Add thead repetition on page breaks.

### Phase 3: Advanced Semantics
- colspan/rowspan
- more complete caption-side behavior
- optional cell split across pages for very tall content

## Compatibility and Regression Risk Areas
1. Page location computation determinism (LOCATION mode).
2. Memory pressure from buffering table subtrees and per-cell layout structures.
3. Performance on large tables with many cells.
4. Existing inline/block flow should remain unchanged for non-table content.

## Test Strategy (recommended)

### Unit-style parser/CSS tests
- DOM tag mapping for table tags.
- CSS display and table property parsing.
- Selector matching on table descendants.

### Layout tests
- Single-row/single-column table.
- Multi-column width distribution.
- Nested inline tags inside cells.
- Header/body sections.
- Pagination at row boundaries.

### Rendering tests
- Border visibility in one-bit and three-bit modes.
- Alignment and wrapping inside cells.
- Margin/width interactions with surrounding block content.

### Regression tests
- Existing non-table fixtures unchanged.
- Page location consistency checks before/after table feature flag.

## Final Recommendation
Implement table support as a dedicated layout subsystem in Page, driven by explicit table tags in DOM and expanded CSS display/property parsing. Avoid trying to force table behavior into the existing purely paragraph-streaming path in HTMLInterpreter.

This approach minimizes long-term complexity and creates a clear path from MVP support to higher-fidelity table rendering.
