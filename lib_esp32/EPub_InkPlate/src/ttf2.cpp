#define STB_TRUETYPE_IMPLEMENTATION

#include "alloc.hpp"

#define STBTT_malloc(x,u)  ((void)(u),allocate(x))
#define STBTT_free(x,u)    ((void)(u),free(x))

#define __TTF__ 1
#include "ttf2.hpp"

#include "screen.hpp"
#include "logging.hpp"

#include <algorithm>
#include <ios>
#include <cstdint>
#include <iostream>

TTF::TTF(const std::string & filename)
{
  is_ready = false;
  memory_font = nullptr;
  is_ready = set_font_face_from_file(filename);
}

TTF::TTF(unsigned char * buffer, int32_t size)
{
  is_ready = false;
  memory_font = nullptr;
  is_ready = set_font_face_from_memory(buffer, size);
}

TTF::~TTF()
{
  if (is_ready) clear_face();
  is_ready = false;
}

void
TTF::clear_face()
{
  clear_glyph_cache();
  free(memory_font);
  current_size = -1;
}

void
TTF::clear_glyph_cache()
{
  for (auto const & entry : glyph_cache) {
    for (auto const & glyph : entry.second) {
      free(glyph.second->buffer);
      free(glyph.second);
    }
  }
  glyph_cache.clear();
  glyph_cache.reserve(50);
}

TTF::BitmapGlyph *
TTF::get_glyph(int32_t charcode)
{
  GlyphsCache::iterator cit;
  Glyphs::iterator git;

  if (!is_ready) return nullptr;

  cit = glyph_cache.find(current_size);
  bool found = (cit != glyph_cache.end()) &&
               ((git = cit->second.find(charcode)) != cit->second.end());

  if (found) {
    return git->second;
  }
  else {
    int glyph_index = stbtt_FindGlyphIndex(&font, charcode);
    if (glyph_index == 0) {
      LOG_E("Charcode not found in face: %d", charcode);
      charcode = ' ';
      if ((cit != glyph_cache.end()) && ((git = cit->second.find(charcode)) != cit->second.end())) {
        return git->second;
      };
      glyph_index = stbtt_FindGlyphIndex(&font, charcode);
    }

    BitmapGlyph * glyph = (BitmapGlyph *) allocate(sizeof(BitmapGlyph));
    glyph->buffer = nullptr;
    glyph->root = this;

    int32_t width, height, xoff, yoff;
    
    #if DEBUGGING
      xoff   = 9999;
      yoff   = 9999;
      width  = 9999;
      height = 9999;
    #endif
    
    glyph->buffer = stbtt_GetGlyphBitmap(
        &font, 0, scale_factor, glyph_index, &width, &height, &xoff, &yoff);

    #if DEBUGGING
      if ((xoff == 9999) || (xoff == 9999)) {
        LOG_E("Hum... Glyph with uninitialized offsets: charcode: %d xoff: %d yoff: %d", charcode, xoff, yoff);
      }
    #endif

    glyph->width = width;
    glyph->rows  = height;
    glyph->xoff  = xoff;
    glyph->yoff  = yoff;

    int32_t advance, left_size;
    stbtt_GetGlyphHMetrics(&font, glyph_index, &advance, &left_size);

    glyph->advance   = advance * scale_factor;
    glyph->left_side = left_size * scale_factor;

    glyph_cache[current_size][charcode] = glyph;
    return glyph;
  }
}

bool 
TTF::set_font_size(int16_t size)
{
  if (!is_ready) return false;
  if (current_size == size) return true;

  // screen resolution per inch ->  72
  //              x             ->  size

  ppem = (Screen::RESOLUTION * size) / 72;

  scale_factor = stbtt_ScaleForMappingEmToPixels(&font, ppem);
  //scale_factor = stbtt_ScaleForPixelHeight(&font, ppem;

  if (scale_factor == 0) {
    LOG_E("set_font_size: scale_factor is 0!: size: %d ppem: %d scale factor: %f", size, ppem, scale_factor);
  }

  ascender         = scale_factor * ascent;
  descender        = scale_factor * descent;
  font_line_height = ascender - descender + (scale_factor * line_gap);
  
  current_size = size;
  return true;
}

bool 
TTF::set_font_face_from_file(const std::string & font_filename)
{
  LOG_D("set_font_face_from_file() ...");

  FILE * font_file;
  if ((font_file = fopen(font_filename.c_str(), "r")) == nullptr) {
    LOG_E("set_font_face_from_file: Unable to open font file '%s'", font_filename.c_str());
    return false;
  }
  else {
    uint8_t * buffer;

    if (fseek(font_file, 0, SEEK_END)) {
      LOG_E("set_font_face_from_file: Unable to seek to the end of file");
      fclose(font_file);
      return false;
    }

    int32_t length = ftell(font_file);
    
    LOG_D("Font File Length: %d", length);

    buffer = (uint8_t *) allocate(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %d", (int32_t) (length + 1));
      return false;
    }

    if (fseek(font_file, 0, SEEK_SET)) {
      LOG_E("set_font_face_from_file: Unable to seek to the beginning of file");
      fclose(font_file);
      free(buffer);
      return false;
    }
    else {
      if (fread(buffer, length, 1, font_file) != 1) {
        LOG_E("set_font_face_from_file: Unable to read file content");
        fclose(font_file);
        free(buffer);
        return false;
      }
    }
    
    fclose(font_file);

    buffer[length] = 0;

    return set_font_face_from_memory(buffer, length);
  }
}

bool 
TTF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_length)
{
  if (is_ready) clear_face();

  int16_t idx = stbtt_GetFontOffsetForIndex(buffer, 0);
  int16_t error;

  if (idx != -1) {
    error = stbtt_InitFont(&font, buffer, idx);
  }
  if ((idx == -1) || (error == 0)) {
    LOG_E("The memory font format is unsupported or is broken.");
    return false;
  }

  stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

  memory_font  = buffer;
  current_size = -1;
  return true;
}

void
TTF::show_glyph(BitmapGlyph & g)
{
  for (int j = 0; j < g.rows; ++j) {
    for (int i = 0; i < g.width; ++i) {
      std::cout << " .:ioVM@"[g.buffer[j * g.width + i] >> 5];
    }
    std::cout << std::endl;
  }
}