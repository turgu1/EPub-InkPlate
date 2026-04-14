// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "memory_pool.hpp"
#include "models/font.hpp"

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <forward_list>
#include <mutex>
#include <unordered_map>

class TTF : public Font {
private:
  static constexpr char const *TAG = "TTF";

  FT_Face face;
  std::mutex mutex;

public:
  TTF(const std::string &filename);
  TTF(MemoryFontPtr buffer, int32_t size);
  ~TTF();

  // /**
  //  * @brief Get a glyph object
  //  *
  //  * Retrieve a glyph object and convert it to it's bitmap représentation.
  //  * The cache will be updated to contain the new glyph, if it's not
  //  * already there.
  //  *
  //  * @param charcode Character code as a unicode number.
  //  * @return Glyph The glyph associated to the unicode character.
  //  */
  // #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
  //   Glyph * getGlyph(int32_t charcode, int16_t glyph_size, bool debugging = false);
  // #else
  //   Glyph * getGlyph(int32_t charcode, int16_t glyph_size);
  // #endif

  auto adjustLigatureAndKern(Glyph *glyph, uint16_t glyphSize, uint32_t nextCharcode, int16_t &kern,
                             bool &ignoreNext) -> Glyph * {
    kern       = 0;
    ignoreNext = false;
    return glyph;
  }

  /**
   * @brief Face normal line height
   *
   *
   * @return int32_t Normal line height of the face in pixels
   */
  auto getLineHeight(int16_t glyphSize) -> int32_t {
    std::scoped_lock guard(mutex);
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
    std::scoped_lock guard(mutex);
    if (currentFontSize != glyphSize) setFontSize(glyphSize);
    return (face == nullptr) ? 0 : (face->size->metrics.descender >> 6);
  }

private:
  static FT_Library library;
  auto clearFace() -> void;

  /**
   * @brief Set the font face object
   *
   * Get a font from memory loaded and ready to supply glyphs. Note
   * that the buffer will be freed when the face will be removed.
   *
   * @param buffer The buffer containing the font.
   * @param size   The buffer size in bytes.
   * @return true The font was found and retrieved.
   * @return false Some error (file not found, unsupported format).
   */
  auto setFontFaceFromMemory(MemoryFontPtr buffer, int32_t size) -> bool;

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

  auto getGlyphInternal(uint32_t charcode, int16_t glyphSize) -> Glyph *;
};
