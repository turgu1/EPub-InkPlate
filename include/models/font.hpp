// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "memory_pool.hpp"

#include <unordered_map>
#include <forward_list>
#include <mutex>

class Font
{
  public:
    struct Glyph {
      Dim             dim;
      int16_t         xoff, yoff;
      int16_t         advance;
      int16_t         pitch;
      int16_t         line_height;
      int16_t         ligature_and_kern_pgm_index;
      unsigned char * buffer;
    };
    
  private:
    static constexpr char const * TAG = "Font";

    std::mutex mutex;

  public:
    Font();
   ~Font();

    inline bool is_ready() const { return ready; }

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
    #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
      virtual Glyph * get_glyph(int32_t charcode, int16_t glyph_size, bool debugging = false);
    #else
      virtual Glyph * get_glyph(int32_t charcode, int16_t glyph_size);
    #endif

    virtual Glyph * get_glyph(int32_t   charcode, 
                              int32_t   next_charcode, 
                              int16_t   glyph_size,
                              int16_t & kern,  
                              bool    & ignore_next);

    void clear_cache();

    void get_size(const char * str, Dim * dim, int16_t glyph_size);

    inline void    set_fonts_cache_index(int16_t index) { fonts_cache_index = index; }
    inline int16_t get_fonts_cache_index()              { return fonts_cache_index;  }

      /**
     * @brief Face normal line height
     * 
     * 
     * @return int32_t Normal line height of the face in pixels
     */
    virtual int32_t get_line_height(int16_t glyph_size) = 0;

    /**
     * @brief Face descender height
     * 
     * @return int32_t The face descender height in pixels related to
     *                 the current font size.
     */
    virtual int32_t get_descender_height(int16_t glyph_size) = 0;

protected:
    static constexpr uint16_t BYTE_POOL_SIZE = 16384*2;

    typedef std::unordered_map<int32_t, Glyph *> Glyphs; ///< Cache for the glyphs'  bitmap 
    typedef std::unordered_map<int16_t, Glyphs>        GlyphsCache;
    typedef uint8_t                                    BytePool[BYTE_POOL_SIZE];
    typedef std::forward_list<BytePool *>              BytePools;
    
    GlyphsCache             cache;
    int16_t                 fonts_cache_index;
    int8_t                  current_font_size;
    bool                    ready;
    
    MemoryPool<Glyph> bitmap_glyph_pool;
    
    BytePools               byte_pools;
    uint16_t                byte_pool_idx;

    void      add_buff_to_byte_pool();
    uint8_t * byte_pool_alloc(uint16_t size);

    unsigned char * memory_font;  ///< Buffer for memory fonts

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
     * @brief Set the font size
     * 
     * Set the font size. This will be used to set various general metrics 
     * required from the font structure. 
     * 
     * @param size The size of the glyphs in points (1/72th of an inch).
     * @return true The font was resized.
     * @return false Not able to resize the font.
     */
    bool set_font_size(int16_t size);

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
    virtual bool set_font_face_from_memory(unsigned char * buffer, int32_t size) = 0;

    #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
      virtual Glyph * get_glyph_internal(int32_t charcode, int16_t glyph_size, bool debugging = false) = 0;
    #else
      virtual Glyph *      get_glyph_internal(int32_t charcode, int16_t glyph_size) = 0;
    #endif

    virtual Glyph * adjust_ligature_and_kern(Glyph   * glyph, 
                                             uint16_t  glyph_size, 
                                             uint32_t  next_charcode, 
                                             int16_t & kern, 
                                             bool    & ignore_next) = 0;
};
