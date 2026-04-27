// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/ttf2.hpp"
#include "models/page_locs.hpp"
#include "viewers/msg_viewer.hpp"

#include "alloc.hpp"
#include "screen.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

TTF::TTF(const FontFaceDescriptorPtr descr, FT_Library &library) : Font() {
  face = nullptr;

  setFontFace(descr, library);
}

TTF::~TTF() {
  ready = false;
  if (face != nullptr) clearFace();
}

auto TTF::clearFace() -> void {
  LOG_D("Clearing TTF face and cache...");
  pageLocs.stopControlTask();
  clearCache();
  if (face != nullptr) {
    FT_Done_Face(face);
    face = nullptr;
  }

  ready           = false;
  currentFontSize = -1;
}

auto TTF::getGlyphInternal(uint32_t charcode, int16_t glyphSize) -> Glyph * {
  int error;
  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  GlyphsCache::iterator cacheIt = cache.find(glyphSize);

  bool found =
      (cacheIt != cache.end()) && ((git = cacheIt->second.find(charcode)) != cacheIt->second.end());

  if (found) {
    return git->second;
  } else {
    if (currentFontSize != glyphSize) setFontSize(glyphSize);

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
        LOG_E("Unable to load glyph for charcode: %" PRIu32, charcode);
        return nullptr;
      }
    }

    Glyph *glyph = bitmapGlyphPool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      MsgViewer::outOfMemory("glyph allocation");
    }

    FT_GlyphSlot slot = face->glyph;

    glyph->dim.width               = slot->metrics.width >> 6;
    glyph->dim.height              = slot->metrics.height >> 6;
    glyph->lineHeight              = face->size->metrics.height >> 6;
    glyph->ligatureAndKernPgmIndex = -1;

    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
      if (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT) {
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
        MsgViewer::outOfMemory("glyph allocation");
      }
      // else {
      //   LOG_D("Allocated %d bytes for glyph.", size)
      // }

      memcpy(glyph->buffer, slot->bitmap.buffer, size);
    } else {
      glyph->buffer = nullptr;
    }

    glyph->xoff    = slot->bitmap_left;
    glyph->yoff    = -slot->bitmap_top;
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

    cache[currentFontSize][charcode] = glyph;

    return glyph;
  }
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
  return true;
}

auto TTF::setFontFace(const FontFaceDescriptorPtr descr, FT_Library &library) -> bool {
  if (face != nullptr) clearFace();

  int error = FT_New_Memory_Face(library, (const FT_Byte *)descr->fontData.get(),
                                 descr->fontDataSize, 0, &face);
  if (error) {
    LOG_E("The memory font format is unsupported or is broken (%d).", error);
    return false;
  }

  ready              = true;
  fontFaceDescriptor = descr;

  return true;
}
