#ifndef __TTF_HPP__
#define __TTF_HPP__

///#include "global.hpp"

#include "stb_truetype.h"

#include <unordered_map>
#include <string>

class TTF
{
  public:

    struct BitmapGlyph {
      unsigned char * buffer;
      TTF * root;
      int16_t width, rows;
      int16_t xoff, yoff;
      int16_t advance, left_side;
    };
    
  private:
    static constexpr char const * TAG = "TTF";

    bool is_ready;
    stbtt_fontinfo font;
    float scale_factor;
    int32_t ascent, descent, line_gap;        ///< Unscaled values
    int16_t font_line_height, descender, ascender, ppem; ///< Scaled values

  public:
    TTF(const std::string & filename);
    TTF(unsigned char * buffer, int32_t size);
   ~TTF();

    int16_t index;
    
    inline bool ready() const { return is_ready; }

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
    int16_t get_line_height() { 
      return (is_ready) ? font_line_height : 0; 
    }

    /**
     * @brief EM width
     * 
     * @return int16_t EM width in pixels related to the current font size. 
     */
    int16_t get_em_width() { return is_ready ? ppem : 0; }
    
    /**
     * @brief Face descender height
     * 
     * @return int16_t The face descender height in pixels related to
     *                 the current font size.
     */
    int16_t get_descender_height() { return is_ready ? descender : 0; }

    /**
     * @brief Face ascender height
     * 
     * @return int16_t The face ascender height in pixels relative to 
     *                 the current font size.
     */
    int16_t ascender_height() { return is_ready ? ascender : 0; }

    void show_glyph(BitmapGlyph & g);

  private:
    typedef std::unordered_map<int32_t, BitmapGlyph *> Glyphs; ///< Cache for the glyphs'  bitmap 
    typedef std::unordered_map<int16_t, Glyphs> GlyphsCache;
    GlyphsCache glyph_cache;

    unsigned char * memory_font;                      ///< Buffer for memory fonts
    int current_size;

    void clear_face();
    void clear_glyph_cache();

    /**
     * @brief Set the font face object
     * 
     * Get a font file loaded and ready to supply glyphs.
     * 
     * @param font_filename The filename of the font. 
     * @return true The font was found and retrieved
     * @return false Some error (file not found, unsupported format)
     */
    bool set_font_face_from_file(const std::string & font_filename);

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
    bool set_font_face_from_memory(unsigned char * buffer, int32_t buffer_length);
};

#endif