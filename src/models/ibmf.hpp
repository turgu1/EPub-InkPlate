// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "memory_pool.hpp"
#include "models/font.hpp"
#include "models/fonts_db.hpp"
#include "models/ibmf_font.hpp"

#include <forward_list>
// #include <mutex>
#include <unordered_map>

class IBMF : public Font {
private:
  static constexpr char const *TAG = "IBMF";

  IBMFFont *face;
  // std::mutex mutex;
  IBMFFont::GlyphInfo *glyphData;

  IBMF(const FontFaceDescriptorPtr &descr);

public:
  ~IBMF();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(const FontFaceDescriptorPtr &descr) -> FontPtr {
    return makeUniqueHimem<IBMF>(descr);
  }

  /**
   * @brief Get a glyph object
   *
   * Retrieve a glyph object and convert it to it's bitmap représentation.
   * The cache will be updated to contain the new glyph, if it's not
   * already there.
   *
   * @param charcode Character code as a unicode number.
   * @return Glyph The glyph associated to the unicode character.
   */

  auto getGlyph(uint32_t charcode, int16_t glyphSize) -> Glyph * override;

  auto getGlyph(uint32_t charcode, uint32_t nextCharcode, int16_t glyphSize, int16_t &kern,
                bool &ignoreNext) -> Glyph * override;

  auto adjustLigatureAndKern(Glyph *glyph, uint16_t glyphSize, uint32_t nextCharcode, int16_t &kern,
                             bool &ignoreNext) -> Glyph *;

  /**
   * @brief Face normal line height
   *
   *
   * @return int32_t Normal line height of the face in pixels
   */
  auto getLineHeight(int16_t glyphSize) -> int32_t {
    // std::scoped_lock guard(mutex);
    if (currentFontSize != glyphSize) setFontSize(glyphSize);
    return (face == nullptr) ? 0 : (face->getLineHeight());
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
    return (face == nullptr) ? 0 : (face->getDescenderHeight());
  }

private:
  auto clearFace() -> void;

  /**
   * @brief Set the font face object
   *
   * Get a font from memory loaded and ready to supply glyphs. Note
   * that the buffer will be freed when the face will be removed.
   *
   * @param descr The font face descriptor.
   * @return true The font was found and retrieved.
   * @return false Some error (file not found, unsupported format).
   */
  auto setFontFace(const FontFaceDescriptorPtr &descr) -> bool;

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

  [[nodiscard]] inline auto translate(uint32_t charcode) -> uint32_t {
    return face->translate(charcode);
  }
};
