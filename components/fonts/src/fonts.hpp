// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "char_pool.hpp"
#include "font.hpp"
#include "global.hpp"
#include "himem.hpp"

#include <vector>

#define FT_DEBUG_LOGGING 1

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

const uint8_t ICONS_FONT_INDEX          = 0;
const uint8_t SYSTEM_REGULAR_FONT_INDEX = 1;
const uint8_t SYSTEM_ITALIC_FONT_INDEX  = 2;

using FontEntryPtr = HimemSharedPtr<class FontEntry>;
class FontEntry {
private:
  FontEntry() = default;

public:
  HimemString name;
  FontPtr font;
  FaceStyle style;

  ~FontEntry() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeSharedHimem(Args &&...args) -> HimemSharedPtr<T>;

  static inline auto Make() -> FontEntryPtr { return makeSharedHimem<FontEntry>(); }
};

class Fonts {
private:
  static constexpr char const *TAG = "Fonts";

  FT_Library library{nullptr};

  // Desriptors for book related fonts. This is used to keep track
  // of the fonts loaded from a book, so they can be cleared when the
  // book is closed.

  HimemVector<FontFaceDescriptorPtr> bookFontFaceDescriptors;

  FontsDB *fontsDB{nullptr};
  bool thisIsAppFonts{false};

public:
  Fonts(bool thisIsAppFonts = false);
  ~Fonts();

  auto setup() -> bool;

  /**
   * @brief Clear fonts loaded from a book
   *
   * This will keep the default fonts loaded from the application folder. It will clean
   * all glyphs in all fonts caches.
   *
   * @param all If true, default fonts will also be removed
   */
  auto clear(bool all = false) -> void;
  auto clearEverything() -> void;

  /**
   * @brief Get font at index
   *
   * @param index THe font index number
   * @return Pointer to the font at index. If there is no font at index,
   *         it returns the pointer to the first font in the list.
   */
  auto getFont(int16_t index) -> FontPtr & {
    FontPtr *f;
    if (index >= (int16_t)fontCache.size()) {
      LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, fontCache.size());
      f = &fontCache.at(1)->font;
    } else {
      f = &fontCache.at(index)->font;
    }
    return *f;
  };

  auto getFontEntry(int16_t index) -> FontEntryPtr & {
    FontEntryPtr *f;
    if (index >= (int16_t)fontCache.size()) {
      LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, fontCache.size());
      f = &fontCache.at(1);
    } else {
      f = &fontCache.at(index);
    }
    return *f;
  };

  /**
   * @brief Get index of a font
   *
   * @param name Font name
   * @param style Font style (bold, italic, normal)
   * @return Index number related to a font name and a face style.
   *         If not found, returns -1.
   */
  auto getFontIndex(const HimemString &name, FaceStyle style) -> int16_t;

  auto replace(int16_t index, const FontFaceDescriptorPtr &descr) -> bool;

  /**
   * @brief Get Font name
   *
   * @param name Font name
   * @param style Font style (bold, italic, normal)
   * @return Pointer to the name of the font at index. If there is no font
   *         at index, returns the name of the first in the list.
   */
  auto getName(int16_t index) const -> const char * {
    if (index >= (int16_t)fontCache.size()) {
      LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, fontCache.size());
      return fontCache[1]->name.c_str();
    } else {
      return fontCache[index]->name.c_str();
    }
  };

  auto getStandardName(int16_t index) const -> const char * {
    if (index >= fontsDB->getStandardFontCount()) {
      LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, fontsDB->getStandardFontCount());
      return fontsDB->getStandardFontName(0).c_str();
    } else {
      return fontsDB->getStandardFontName(index).c_str();
    }
  };

  /**
   * @brief Add a font from a font descriptor.
   *
   * @param descr Font descriptor containing name, style, filename, and memory buffer
   * @return true The font was loaded
   * @return false Some error (font data is not good, does not exist, etc.)
   */
  auto add(const FontFaceDescriptorPtr &descr) -> bool;

  auto add(const HimemString &fontFamily, FaceStyle style, FileContentPtr buffer, size_t size,
           const HimemString &filename) -> bool;

  auto adjustFontStyle(FaceStyle style, FaceStyle fontStyle, FaceStyle fontWeight) const
      -> FaceStyle;

  auto check(int16_t index, FaceStyle style) -> bool const {
    if ((index >= (int16_t)fontCache.size()) || (fontCache[index]->style != style)) {
      LOG_E("Hum... font_check failed");
      return false;
    }
    return true;
  };

  auto clearGlyphCaches() -> void;

  auto adjustDefaultFont(uint8_t fontIndex) -> void;

  auto setGlyphLoadMode(Font::GlyphLoadMode mode) -> void;

private:
  using FontCache = std::vector<FontEntryPtr>;
  Font::GlyphLoadMode defaultGlyphLoadMode{Font::GlyphLoadMode::WITH_BITMAP};
  FontCache fontCache;

  CharPoolPtr charPool{nullptr};
};

#if __FONTS__
Fonts appFonts(true);
#else
extern Fonts appFonts;
#endif
