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

class TTF : public Font {
private:
  static constexpr char const *TAG = "TTF";

  FT_Face face{nullptr};
  TTF(const FontFaceDescriptorPtr &descr, FT_Library &library);

public:
  ~TTF();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(const FontFaceDescriptorPtr &descr, FT_Library &library) -> FontPtr {
    return makeUniqueHimem<TTF>(descr, library);
  }

  auto getKern(Glyph &glyph, char32_t nextCharcode) -> int16_t;

  /**
   * @brief Face normal line height
   *
   *
   * @return int32_t Normal line height of the face in pixels
   */
  auto getLineHeight(int16_t glyphSize) -> int32_t {
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

  auto getGlyphInternal(Glyph &glyph, char32_t charcode, int16_t glyphSize) -> bool;
};
