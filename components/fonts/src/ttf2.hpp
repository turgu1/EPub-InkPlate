// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "font.hpp"
#include "himem_pool.hpp"

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "fonts_db.hpp"
#include <forward_list>
// #include <mutex>

#include <unordered_map>

#if 0
class KerningExtractor {

private:
  auto ttUSHORT(const uint8_t *p) -> uint16_t { return (p[0] << 8) + p[1]; }
  auto ttSHORT(const uint8_t *p) -> int16_t { return (p[0] << 8) + p[1]; }
  auto ttULONG(const uint8_t *p) -> uint32_t {
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
  }
  auto ttLONG(const uint8_t *p) -> int32_t {
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
  }

  int32_t GetGlyphGPOSInfoAdvance(const fontinfo *info, int glyph1, int glyph2) {
    uint16_t lookupListOffset;
    uint8_t *lookupList;
    uint16_t lookupCount;
    uint8_t *data;
    int32_t i, sti;

    if (!info->gpos) return 0;

    data = info->data + info->gpos;

    if (ttUSHORT(data + 0) != 1) return 0; // Major version 1
    if (ttUSHORT(data + 2) != 0) return 0; // Minor version 0

    lookupListOffset = ttUSHORT(data + 8);
    lookupList       = data + lookupListOffset;
    lookupCount      = ttUSHORT(lookupList);

    for (i = 0; i < lookupCount; ++i) {
      uint16_t lookupOffset = ttUSHORT(lookupList + 2 + 2 * i);
      uint8_t *lookupTable  = lookupList + lookupOffset;

      uint16_t lookupType      = ttUSHORT(lookupTable);
      uint16_t subTableCount   = ttUSHORT(lookupTable + 4);
      uint8_t *subTableOffsets = lookupTable + 6;
      if (lookupType != 2) // Pair Adjustment Positioning Subtable
        continue;

      for (sti = 0; sti < subTableCount; sti++) {
        uint16_t subtableOffset = ttUSHORT(subTableOffsets + 2 * sti);
        uint8_t *table          = lookupTable + subtableOffset;
        uint16_t posFormat      = ttUSHORT(table);
        uint16_t coverageOffset = ttUSHORT(table + 2);
        int32_t coverageIndex   = GetCoverageIndex(table + coverageOffset, glyph1);
        if (coverageIndex == -1) continue;

        switch (posFormat) {
        case 1: {
          int32_t l, r, m;
          int straw, needle;
          uint16_t valueFormat1 = ttUSHORT(table + 4);
          uint16_t valueFormat2 = ttUSHORT(table + 6);
          if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
            int32_t valueRecordPairSizeInBytes = 2;
            uint16_t pairSetCount              = ttUSHORT(table + 8);
            uint16_t pairPosOffset             = ttUSHORT(table + 10 + 2 * coverageIndex);
            uint8_t *pairValueTable            = table + pairPosOffset;
            uint16_t pairValueCount            = ttUSHORT(pairValueTable);
            uint8_t *pairValueArray            = pairValueTable + 2;

            if (coverageIndex >= pairSetCount) return 0;

            needle = glyph2;
            r      = pairValueCount - 1;
            l      = 0;

            // Binary search.
            while (l <= r) {
              uint16_t secondGlyph;
              uint8_t *pairValue;
              m           = (l + r) >> 1;
              pairValue   = pairValueArray + (2 + valueRecordPairSizeInBytes) * m;
              secondGlyph = ttUSHORT(pairValue);
              straw       = secondGlyph;
              if (needle < straw)
                r = m - 1;
              else if (needle > straw)
                l = m + 1;
              else {
                uint16_t xAdvance = ttSHORT(pairValue + 2);
                return xAdvance;
              }
            }
          } else
            return 0;
          break;
        }

        case 2: {
          uint16_t valueFormat1 = ttUSHORT(table + 4);
          uint16_t valueFormat2 = ttUSHORT(table + 6);
          if (valueFormat1 == 4 && valueFormat2 == 0) { // Support more formats?
            uint16_t classDef1Offset = ttUSHORT(table + 8);
            uint16_t classDef2Offset = ttUSHORT(table + 10);
            int glyph1class          = GetGlyphClass(table + classDef1Offset, glyph1);
            int glyph2class          = GetGlyphClass(table + classDef2Offset, glyph2);

            uint16_t class1Count = ttUSHORT(table + 12);
            uint16_t class2Count = ttUSHORT(table + 14);
            uint8_t *class1Records, *class2Records;
            uint16_t xAdvance;

            if (glyph1class < 0 || glyph1class >= class1Count) return 0; // malformed
            if (glyph2class < 0 || glyph2class >= class2Count) return 0; // malformed

            class1Records = table + 16;
            class2Records = class1Records + 2 * (glyph1class * class2Count);
            xAdvance      = ttSHORT(class2Records + 2 * glyph2class);
            return xAdvance;
          } else
            return 0;
          break;
        }

        default:
          return 0; // Unsupported position format
        }
      }
    }

    return 0;
  }

  auto GetGlyphKernInfoAdvance(const fontinfo *info, int glyph1, int glyph2) -> int {

    uint8_t *data = info->data + info->kern;
    uint32_t needle, straw;
    int l, r, m;

    // we only look at the first table. it must be 'horizontal' and format 0.
    if (!info->kern) return 0;
    if (ttUSHORT(data + 2) < 1) // number of tables, need at least 1
      return 0;
    if (ttUSHORT(data + 8) != 1) // horizontal flag must be set in format
      return 0;

    l      = 0;
    r      = ttUSHORT(data + 10) - 1;
    needle = glyph1 << 16 | glyph2;
    while (l <= r) {
      m     = (l + r) >> 1;
      straw = ttULONG(data + 18 + (m * 6)); // note: unaligned read
      if (needle < straw)
        r = m - 1;
      else if (needle > straw)
        l = m + 1;
      else
        return ttSHORT(data + 22 + (m * 6));
    }
    return 0;
  }

  int32_t GetCoverageIndex(uint8_t *coverageTable, int glyph) {
    uint16_t coverageFormat = ttUSHORT(coverageTable);
    switch (coverageFormat) {
    case 1: {
      uint16_t glyphCount = ttUSHORT(coverageTable + 2);

      // Binary search.
      int32_t l = 0, r  = glyphCount - 1, m;
      int straw, needle = glyph;
      while (l <= r) {
        uint8_t *glyphArray = coverageTable + 4;
        uint16_t glyphID;
        m       = (l + r) >> 1;
        glyphID = ttUSHORT(glyphArray + 2 * m);
        straw   = glyphID;
        if (needle < straw)
          r = m - 1;
        else if (needle > straw)
          l = m + 1;
        else {
          return m;
        }
      }
      break;
    }

    case 2: {
      uint16_t rangeCount = ttUSHORT(coverageTable + 2);
      uint8_t *rangeArray = coverageTable + 4;

      // Binary search.
      int32_t l = 0, r = rangeCount - 1, m;
      int strawStart, strawEnd, needle = glyph;
      while (l <= r) {
        uint8_t *rangeRecord;
        m           = (l + r) >> 1;
        rangeRecord = rangeArray + 6 * m;
        strawStart  = ttUSHORT(rangeRecord);
        strawEnd    = ttUSHORT(rangeRecord + 2);
        if (needle < strawStart)
          r = m - 1;
        else if (needle > strawEnd)
          l = m + 1;
        else {
          uint16_t startCoverageIndex = ttUSHORT(rangeRecord + 4);
          return startCoverageIndex + glyph - strawStart;
        }
      }
      break;
    }

    default:
      return -1; // unsupported
    }

    return -1;
  }

public:
  auto GetGlyphKernAdvance(const fontinfo *info, int g1, int g2) -> int {
    int xAdvance = 0;

    if (info->gpos)
      xAdvance += GetGlyphGPOSInfoAdvance(info, g1, g2);
    else if (info->kern)
      xAdvance += GetGlyphKernInfoAdvance(info, g1, g2);

    return xAdvance;
  }

  auto GetCodepointKernAdvance(const fontinfo *info, int ch1, int ch2) -> int {
    if (!info->kern &&
        !info->gpos) // if no kerning table, don't waste time looking up both codepoint->glyphs
      return 0;
    return GetGlyphKernAdvance(info, FindGlyphIndex(info, ch1), FindGlyphIndex(info, ch2));
  }
};
#endif

class TTF : public Font {
private:
  static constexpr char const *TAG = "TTF";

  enum class SizeMode : uint8_t {
    POINT, PIXEL
  };

  static constexpr auto toCacheKey(int16_t size, SizeMode mode) -> int16_t {
    return (mode == SizeMode::PIXEL) ? static_cast<int16_t>(-size) : size;
  }

  FT_Face face{nullptr};
  SizeMode currentSizeMode{SizeMode::POINT};
  // std::mutex mutex;

  TTF(const FontFaceDescriptorPtr &descr, FT_Library &library);

public:
  ~TTF();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(const FontFaceDescriptorPtr &descr, FT_Library &library) -> FontPtr {
    return makeUniqueHimem<TTF>(descr, library);
  }

  auto getGlyphPx(char32_t charcode, int16_t pixelHeight) -> Glyph * override;
  auto getGlyphPx(char32_t charcode, char32_t nextCharcode, int16_t pixelHeight)
      -> std::tuple<Glyph *, int16_t, bool> override;

  auto getKern(Glyph &glyph, char32_t nextCharcode) -> int16_t;

  /**
   * @brief Face normal line height
   *
   *
   * @return int32_t Normal line height of the face in pixels
   */
  auto getLineHeight(int16_t glyphSize) -> int32_t {
    // std::scoped_lock guard(mutex);
    if (currentFontSize != glyphSize) setFontSize(glyphSize);
    return (face == nullptr) ? 0 : (face->size->metrics.height >> 6);
  }

  /**
   * @brief Face descender height
   *
   * @return int32_t The face descender height in pixels related to
   *                 the current font size.
   */
  auto getDescenderHeight(int16_t glyphSize) -> int32_t {
    // std::scoped_lock guard(mutex);
    if (currentFontSize != glyphSize) setFontSize(glyphSize);
    return (face == nullptr) ? 0 : (face->size->metrics.descender >> 6);
  }

private:
  auto clearFace() -> void;

  /**
   * @brief Set the font face object
   *
   * Get a font from memory loaded and ready to supply glyphs. Note
   * that the buffer will be freed when the face will be removed.
   *
   * @param descr The font descriptor containing the font data.
   * @param library The FreeType library instance.
   * @return true The font was found and retrieved.
   * @return false Some error (file not found, unsupported format).
   */
  auto setFontFace(const FontFaceDescriptorPtr &descr, FT_Library &library) -> bool;

  /**
   * @brief Set the font size
   *
   * Set the font size. This will be used to set various general metrics
   * required from the font structure.
   *
   * @param size The size of the glyphs in points (1/72th of an inch).
   * @return true The font was resized.
   * @return false Not able to resize the font.
   */
  auto setFontSize(int16_t size) -> bool;
  auto setPixelSize(int16_t size) -> bool;

  auto getGlyphInternal(char32_t charcode, int16_t glyphSize) -> Glyph *;
  auto getGlyphInternal(char32_t charcode, int16_t glyphSize, SizeMode mode) -> Glyph *;
};
