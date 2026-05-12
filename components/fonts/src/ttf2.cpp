// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "ttf2.hpp"

#include "screen.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

TTF::TTF(const FontFaceDescriptorPtr &descr, FT_Library &library) : Font() {
  face = nullptr;

  setFontFace(descr, library);
}

TTF::~TTF() {
  ready = false;
  if (face != nullptr) clearFace();
}

auto TTF::clearFace() -> void {
  LOG_D("Clearing TTF face and cache...");
  if (face != nullptr && face->internal != nullptr) {
    FT_Done_Face(face);
    face = nullptr;
  }

  clearCache();
  ready           = false;
  currentFontSize = -1;
}

auto TTF::getGlyphPx(char32_t charcode, int16_t pixelHeight) -> Glyph * {
  if (!ready || pixelHeight <= 0) return nullptr;
  return getGlyphInternal(charcode, pixelHeight, SizeMode::PIXEL);
}

auto TTF::getGlyphPx(char32_t charcode, char32_t nextCharcode, int16_t pixelHeight)
    -> std::tuple<Glyph *, int16_t, bool> {

  if (!ready || pixelHeight <= 0) return std::make_tuple(nullptr, 0, false);

  bool ignoreNext = false;
  int16_t kern    = 0;

  char32_t resultCharcode = charcode;

  if (nextCharcode != 0) {
    for (int i = 0; i < ligaturesSize; ++i) {
      const Ligature &lig = ligatures[i];

      if (lig.firstChar > charcode) break;

      if ((lig.firstChar == charcode) && (lig.nextChar == nextCharcode)) {
        resultCharcode = lig.replacement;
        ignoreNext     = true;
        break;
      }
    }
  }

  Glyph *glyph = getGlyphInternal(resultCharcode, pixelHeight, SizeMode::PIXEL);

  if (glyph == nullptr) {
    ignoreNext = false;
    glyph      = getGlyphInternal(charcode, pixelHeight, SizeMode::PIXEL);
  }

  if ((glyph != nullptr) && (nextCharcode != 0)) {
    kern = getKern(*glyph, nextCharcode);
  }

  return std::make_tuple(glyph, kern, ignoreNext);
}

auto TTF::getGlyphInternal(char32_t charcode, int16_t glyphSize) -> Glyph * {
  return getGlyphInternal(charcode, glyphSize, SizeMode::POINT);
}

auto TTF::getGlyphInternal(char32_t charcode, int16_t glyphSize, SizeMode mode) -> Glyph * {
  int error;
  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  const bool useCache = (mode == SizeMode::POINT);

  const int16_t cacheKey = toCacheKey(glyphSize, mode);

  if (useCache) {
    GlyphsCache::iterator cacheIt = cache.find(cacheKey);
    bool found = (cacheIt != cache.end()) &&
                 ((git = cacheIt->second.find(charcode)) != cacheIt->second.end());
    if (found) return git->second;
  }

  {
    if ((currentFontSize != glyphSize) || (currentSizeMode != mode)) {
      bool resized = (mode == SizeMode::PIXEL) ? setPixelSize(glyphSize) : setFontSize(glyphSize);
      if (!resized) return nullptr;
    }

    int glyphIndex = FT_Get_Char_Index(face, charcode);
    if (glyphIndex == 0) {
      LOG_D("Charcode not found in face: %" PRIu32 ", font_index: %" PRIi16, charcode,
            fontsCacheIndex);
      return nullptr;
    } else {
      error = FT_Load_Glyph(face,             /* handle to face object */
                            glyphIndex,       /* glyph index           */
                            FT_LOAD_DEFAULT); /* load flags            */
      if (error) {
        LOG_E("Unable to load glyph for charcode: %" PRIu32 " from face: %s", charcode,
              face->family_name ? face->family_name : "unknown");
        return nullptr;
      }
    }

    Glyph *glyph = bitmapGlyphPool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      return nullptr;
    }

    FT_GlyphSlot slot = face->glyph;

    glyph->index      = glyphIndex;
    glyph->dim.width  = slot->metrics.width >> 6;
    glyph->dim.height = slot->metrics.height >> 6;
    glyph->lineHeight = face->size->metrics.height >> 6;

    if (glyphLoadMode_ == GlyphLoadMode::WITH_BITMAP) {
      if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        const bool useMonoRender =
            (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT) &&
            !isAntialiasingPreferred();
        if (useMonoRender) {
          error = FT_Render_Glyph(face->glyph,          // glyph slot
                                  FT_RENDER_MODE_MONO); // render mode
        } else {
          error = FT_Render_Glyph(face->glyph,            // glyph slot
                                  FT_RENDER_MODE_NORMAL); // render mode
        }

        if (error) {
          LOG_E("Unable to render glyph for charcode: %" PRIu32 " error: %d", charcode, error);
          return nullptr;
        }
      }

      glyph->pitch      = slot->bitmap.pitch;
      glyph->dim.height = slot->bitmap.rows;
      glyph->dim.width  = slot->bitmap.width;
      glyph->lineHeight = face->size->metrics.height >> 6;

      int32_t size = glyph->pitch * glyph->dim.height;

      if (size > 0) {
        glyph->buffer = bytePoolAlloc(size);

        if (glyph->buffer == nullptr) {
          LOG_E("Unable to allocate memory for glyph.");
          return nullptr;
        }

        memcpy(glyph->buffer, slot->bitmap.buffer, size);
      } else {
        glyph->buffer = nullptr;
      }

      glyph->xoff = slot->bitmap_left;
      glyph->yoff = -slot->bitmap_top;
    } else {
      // METRICS_ONLY: skip rasterization and buffer allocation
      glyph->buffer = nullptr;
      glyph->pitch  = 0;
      glyph->xoff   = slot->metrics.horiBearingX >> 6;
      glyph->yoff   = -(slot->metrics.horiBearingY >> 6);
    }

    glyph->advance = slot->advance.x >> 6;

    // std::cout << "Glyph: " <<
    //   " w:"  << glyph->dim.width <<
    //   " bw:" << slot->bitmap.width <<
    //   " h:"  << glyph->dim.height <<
    //   " br:" << slot->bitmap.rows <<
    //   " p:"  << glyph->pitch <<
    //   " x:"  << glyph->xoff <<
    //   " y:"  << glyph->yoff <<
    //   " a:"  << glyph->advance << std::endl;

    if (useCache) {
      getOrCreateGlyphs(cacheKey)[charcode] = glyph;
    }

    return glyph;
  }
}

auto TTF::getKern(Glyph &glyph, char32_t nextCharcode) -> int16_t {

  int16_t kern{0};

  int glyphIndex = FT_Get_Char_Index(face, nextCharcode);
  if (glyphIndex != 0) {
    FT_Vector kerning;
    int error = FT_Get_Kerning(face, glyph.index, glyphIndex, FT_KERNING_DEFAULT, &kerning);
    if (error) {
      LOG_E("Unable to get kerning for charcode: %" PRIu32 " error: %d", nextCharcode, error);
    } else {
      kern = kerning.x >> 6;

      // if (kerning.x != 0) {
      //   LOG_I("Kerning between : %" PRIu32 " and : %" PRIu32 " is: %d (%d)", glyph.index,
      //         glyphIndex, kerning.x, kern);
      // }
    }

  } else {
    LOG_W("Next charcode not found in face: %" PRIu32 ", font_index: %" PRIi16, nextCharcode,
          fontsCacheIndex);
  }

  return kern;
}

auto TTF::setFontSize(int16_t size) -> bool {
  int error = FT_Set_Char_Size(face,                // handle to face object
                               0,                   // char_width in 1/64th of points
                               size * 64,           // char_height in 1/64th of points
                               Screen::RESOLUTION,  // horizontal device resolution
                               Screen::RESOLUTION); // vertical device resolution

  if (error) {
    LOG_E("Unable to set font size.");
    return false;
  }

  currentFontSize = size;
  currentSizeMode = SizeMode::POINT;
  return true;
}

auto TTF::setPixelSize(int16_t size) -> bool {
  int error = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size));

  if (error) {
    LOG_E("Unable to set pixel font size.");
    return false;
  }

  currentFontSize = size;
  currentSizeMode = SizeMode::PIXEL;
  return true;
}

auto TTF::setFontFace(const FontFaceDescriptorPtr &descr, FT_Library &library) -> bool {
  if (face != nullptr) clearFace();

  int error = FT_New_Memory_Face(library, (const FT_Byte *)descr->fontData.get(),
                                 descr->fontDataSize, 0, &face);
  if (error) {
    LOG_E("The memory font format is unsupported or is broken (%d).", error);
    return false;
  }

  ready = true;

  return true;
}
