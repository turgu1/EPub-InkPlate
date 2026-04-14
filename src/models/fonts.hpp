// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "char_pool.hpp"
#include "global.hpp"

#include "models/font.hpp"

#include <mutex>
#include <vector>

class Fonts {
private:
  static constexpr char const *TAG = "Fonts";

public:
  Fonts();
  ~Fonts();

  auto setup() -> bool;

  enum class FaceStyle : uint8_t {
    NORMAL = 0, BOLD, ITALIC, BOLD_ITALIC
  };
  struct FontEntry {
    std::string name;
    Font *font;
    FaceStyle style;
  };

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
  auto get(int16_t index) -> Font * {
    Font *f;
    if (index >= (int16_t)fontCache.size()) {
      LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, fontCache.size());
      f = fontCache.at(1).font;
    } else {
      f = fontCache.at(index).font;
    }
    return f;
  };

  /**
   * @brief Get index of a font
   *
   * @param name Font name
   * @param style Font style (bold, italic, normal)
   * @return Index number related to a font name and a face style.
   *         If not found, returns -1.
   */
  auto getIndex(const std::string &name, FaceStyle style) -> int16_t;

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
      return fontCache[1].name.c_str();
    } else {
      return fontCache[index].name.c_str();
    }
  };

  /**
   * @brief Add a font from a file.
   *
   * @param name Font name
   * @param style Font style (bold, italic, normal)
   * @param filename File name
   * @return true The font was loaded
   * @return false Some error (file does not exists, etc.)
   */
  auto add(const std::string &name, FaceStyle style, const std::string &filename) -> bool;

  /**
   * @brief Add a font from memory buffer
   *
   * @param name Font name
   * @param style Font style (bold, italic, normal)
   * @param buffer Memory space where the font is located
   * @param size Size of buffer
   * @return true The font was added
   * @return false Some error occured
   */
  auto add(const std::string &name, FaceStyle style, MemoryFontPtr buffer, int32_t size,
           const std::string &filename) -> bool;

  auto adjustFontStyle(FaceStyle style, FaceStyle fontStyle, FaceStyle fontWeight) const
      -> FaceStyle;

  auto check(int16_t index, FaceStyle style) -> void const {
    if (fontCache[index].style != style) {
      LOG_E("Hum... font_check failed");
    }
  };

  auto clearGlyphCaches() -> void;

  auto adjustDefaultFont(uint8_t fontIndex) -> void;

  auto replace(int16_t index, const std::string &name, FaceStyle style, const std::string &filename)
      -> bool;

private:
  using FontCache = std::vector<FontEntry>;
  FontCache fontCache;
  std::mutex mutex;

  uint8_t fontCount;
  char *fontNames[8];
  char *regularFname[8];
  char *boldFname[8];
  char *italicFname[8];
  char *boldItalicFname[8];

  CharPoolPtr charPool{nullptr};

  auto getFile(const char *filename, uint32_t size) -> char *;
  auto filterFilename(std::string &fname) -> std::string &;
};

#if __FONTS__
  Fonts fonts;
#else
  extern Fonts fonts;
#endif
