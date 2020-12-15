// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __TTF_HPP__
#define __TTF_HPP__

#include "global.hpp"
#include "logging.hpp"
#include "memory_pool.hpp"

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <unordered_map>
#include <forward_list>
#include <string>

class TTF
{
  public:
    struct BitmapGlyph {
      unsigned char * buffer;
      TTF   * root;
      Dim     dim;
      int16_t xoff, yoff;
      int16_t advance;
      int16_t pitch;
    };
    
  private:
    static constexpr char const * TAG = "TTF";

    FT_Face face;

  public:
    TTF(const std::string & filename);
    TTF(unsigned char * buffer, int32_t size);
   ~TTF();

    int16_t fonts_cache_index;
    
    inline bool ready() const { return face != nullptr; }
    
    /**
     * @brief Get a glyph object
     * 
     * Retrieve a glyph object and convert it to it's bitmap représentation. 
     * The cache will be updated to contain the new glyph, if it's not 
     * already there.
     * 
     * @param charcode Character code as a unicode number.
     * @return BitmapGlyph The glyph associated to the unicode character.
     */
    BitmapGlyph * get_glyph(int32_t charcode);

    /**
     * @brief Set the font size
     * 
     * Set the font size. This will be used to set various general metrics 
     * required from the font structure. It will also be used to search 
     * for cached glyphs.
     * 
     * @param size The size of the glyphs in points (1/72th of an inch).
     * @return true The font was resized.
     * @return false Not able to resize the font.
     */
    bool set_font_size(int16_t size);

    /**
     * @brief Face normal line height
     * 
     * 
     * @return int32_t Normal line height of the face in pixels
     */
    int32_t get_line_height() { 
      return (face == nullptr) ? 0 : (face->size->metrics.height >> 6); 
    }

    /**
     * @brief EM width
     * 
     * @return int32_t EM width in pixels related to the current font size. 
     */
    int32_t get_em_width() { 
      return (face == nullptr) ? 0 : face->size->metrics.x_ppem; 
    }

    int32_t get_em_height() { 
      return (face == nullptr) ? 0 : face->size->metrics.y_ppem; 
    }

    /**
     * @brief Face descender height
     * 
     * @return int32_t The face descender height in pixels related to
     *                 the current font size.
     */
    int32_t get_descender_height() {
      return (face == nullptr) ? 0 : (face->size->metrics.descender >> 6); 
    }

    void clear_cache();

    void get_size(const char * str, Dim * dim);

  private:
    static constexpr uint16_t BYTE_POOL_SIZE = 16384;

    typedef std::unordered_map<int32_t, BitmapGlyph *> Glyphs; ///< Cache for the glyphs'  bitmap 
    typedef std::unordered_map<int16_t, Glyphs> GlyphsCache;
    typedef uint8_t BytePool[BYTE_POOL_SIZE];
    typedef std::forward_list<BytePool *> BytePools;
    
    GlyphsCache cache;
    GlyphsCache::iterator cache_it;

    MemoryPool<BitmapGlyph> bitmap_glyph_pool;
    
    BytePools byte_pools;
    uint16_t  byte_pool_idx;

    void add_buff_to_byte_pool();
    uint8_t * byte_pool_alloc(uint16_t size);

    unsigned char * memory_font;                      ///< Buffer for memory fonts
    int current_size;

    static FT_Library library;
    void clear_face();
    
    /**
     * @brief Set the font face object
     * 
     * Get a font file loaded and ready to supply glyphs.
     * 
     * @param font_filename The filename of the font. 
     * @return true The font was found and retrieved
     * @return false Some error (file not found, unsupported format)
     */
    bool set_font_face_from_file(const std::string font_filename);

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
    bool set_font_face_from_memory(unsigned char * buffer, int32_t size);

};

#endif