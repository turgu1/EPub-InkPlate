// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/font.hpp"
#include "helpers/char_pool.hpp"

#include <vector>
#include <mutex>

class Fonts
{
  private:
    static constexpr char const * TAG = "Fonts";

  public:
    Fonts();
   ~Fonts();
    
    bool setup();

    enum class FaceStyle : uint8_t { NORMAL = 0, BOLD, ITALIC, BOLD_ITALIC };
    struct FontEntry {
      std::string name;
      Font *      font;
      FaceStyle   style;
    };

    /**
     * @brief Clear fonts loaded from a book
     * 
     * This will keep the default fonts loaded from the application folder. It will clean
     * all glyphs in all fonts caches.
     * 
     * @param all If true, default fonts will also be removed
     */
    void clear(bool all = false);

    /**
     * @brief Get font at index
     * 
     * @param index THe font index number
     * @return Pointer to the font at index. If there is no font at index,
     *         it returns the pointer to the first font in the list.
     */
    Font * get(int16_t index) {
      Font * f; 
      if (index >= font_cache.size()) {
        LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, font_cache.size());
        f = font_cache.at(1).font;
      }
      else {
        f = font_cache.at(index).font;
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
    int16_t get_index(const std::string & name, FaceStyle style);

    /**
     * @brief Get Font name
     * 
     * @param name Font name
     * @param style Font style (bold, italic, normal)
     * @return Pointer to the name of the font at index. If there is no font
     *         at index, returns the name of the first in the list.
     */
    const char * get_name(int16_t index) const {
      if (index >= font_cache.size()) {
        LOG_E("Fonts.get(): Wrong index: %d vs size: %u", index, font_cache.size());
        return font_cache[1].name.c_str(); 
      }
      else {
        return font_cache[index].name.c_str(); 
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
    bool add(const std::string & name, 
             FaceStyle           style, 
             const std::string & filename);
    
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
    bool add(const std::string & name, 
             FaceStyle           style, 
             unsigned char     * buffer, 
             int32_t             size,
             const std::string & filename);

    FaceStyle adjust_font_style(FaceStyle style, FaceStyle font_style, FaceStyle font_weight) const;

    void check(int16_t index, FaceStyle style) const {
      if (font_cache[index].style != style) {
        LOG_E("Hum... font_check failed");
      } 
    };

    void clear_glyph_caches();

    void adjust_default_font(uint8_t font_index);

    bool replace(int16_t             index,
                 const std::string & name, 
                 FaceStyle           style,
                 const std::string & filename);
  private:
    typedef std::vector<FontEntry> FontCache;
    FontCache font_cache;
    std::mutex mutex;

    uint8_t       font_count;
    char *        font_names[8];
    char *     regular_fname[8];
    char *        bold_fname[8];
    char *      italic_fname[8];
    char * bold_italic_fname[8];

    CharPool char_pool;

    char * get_file(const char * filename, uint32_t size);
};

#if __FONTS__
  Fonts fonts;
#else
  extern Fonts fonts;
#endif
