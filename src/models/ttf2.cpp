// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _TTF_ 1
#include "models/ttf2.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

FT_Library TTF::library{ nullptr };

TTF::TTF(const std::string & filename) : Font()
{
  face  = nullptr;

  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
      std::abort();
    }
  }

  set_font_face_from_file(filename);
}

TTF::TTF(unsigned char * buffer, int32_t buffer_size) : Font()
{
  face  = nullptr;
 
  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
    }
  }

  set_font_face_from_memory(buffer, buffer_size);
}

TTF::~TTF()
{
  ready = false;
  if (face != nullptr) clear_face();
}

void
TTF::clear_face()
{
  clear_cache();
  if (face != nullptr) {
    FT_Done_Face(face);
    face = nullptr;
  }
  if (memory_font != nullptr) {
    free(memory_font);
    memory_font = nullptr;
  }
  
  ready             = false;
  current_font_size = -1;
}

Font::Glyph *
TTF::get_glyph_internal(uint32_t charcode, int16_t glyph_size)
{
  int error;
  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  GlyphsCache::iterator cache_it = cache.find(glyph_size);

  bool found = (cache_it != cache.end()) &&
               ((git = cache_it->second.find(charcode)) != cache_it->second.end());

  if (found) {
    return git->second;
  }
  else {
    if (current_font_size != glyph_size) set_font_size(glyph_size);

    int glyph_index = FT_Get_Char_Index(face, charcode);
    if (glyph_index == 0) {
      LOG_D("Charcode not found in face: %" PRIu32 ", font_index: %" PRIi16, charcode, fonts_cache_index);
      return nullptr;
    }
    else {
      error = FT_Load_Glyph(
            face,             /* handle to face object */
            glyph_index,      /* glyph index           */
            FT_LOAD_DEFAULT); /* load flags            */
      if (error) {
        LOG_E("Unable to load glyph for charcode: %" PRIu32, charcode);
        return nullptr;
      }
    }

    Glyph * glyph = bitmap_glyph_pool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      msg_viewer.out_of_memory("glyph allocation");
    }

    FT_GlyphSlot slot = face->glyph;

    glyph->dim.width   = slot->metrics.width  >> 6;
    glyph->dim.height  = slot->metrics.height >> 6;
    glyph->line_height = face->size->metrics.height >> 6;
    glyph->ligature_and_kern_pgm_index = -1;

    if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
      if (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) {
        error = FT_Render_Glyph(face->glyph,            // glyph slot
                                FT_RENDER_MODE_MONO);   // render mode
      }
      else {
        error = FT_Render_Glyph(face->glyph,            // glyph slot
                                FT_RENDER_MODE_NORMAL); // render mode
      }
      
      if (error) {
        LOG_E("Unable to render glyph for charcode: %" PRIu32 " error: %d", charcode, error);
        return nullptr;
      }
    }

    glyph->pitch       = slot->bitmap.pitch;
    glyph->dim.height  = slot->bitmap.rows;
    glyph->dim.width   = slot->bitmap.width;
    glyph->line_height = face->size->metrics.height >> 6;

    int32_t size = glyph->pitch * glyph->dim.height;

    if (size > 0) {
      glyph->buffer = byte_pool_alloc(size);

      if (glyph->buffer == nullptr) {
        LOG_E("Unable to allocate memory for glyph.");
        msg_viewer.out_of_memory("glyph allocation");
      }
      // else {
      //   LOG_D("Allocated %d bytes for glyph.", size)
      // }

      memcpy(glyph->buffer, slot->bitmap.buffer, size);
    }
    else {
      glyph->buffer = nullptr;
    }

    glyph->xoff    =  slot->bitmap_left;
    glyph->yoff    = -slot->bitmap_top;
    glyph->advance =  slot->advance.x >>  6;

    // std::cout << "Glyph: " <<
    //   " w:"  << glyph->dim.width <<
    //   " bw:" << slot->bitmap.width <<
    //   " h:"  << glyph->dim.height <<
    //   " br:" << slot->bitmap.rows <<
    //   " p:"  << glyph->pitch <<
    //   " x:"  << glyph->xoff <<
    //   " y:"  << glyph->yoff <<
    //   " a:"  << glyph->advance << std::endl;

    cache[current_font_size][charcode] = glyph;

    return glyph;
  }
}

bool 
TTF::set_font_size(int16_t size)
{
  int error = FT_Set_Char_Size(
          face,                 // handle to face object
          0,                    // char_width in 1/64th of points
          size * 64,            // char_height in 1/64th of points
          Screen::RESOLUTION,   // horizontal device resolution
          Screen::RESOLUTION);  // vertical device resolution

  if (error) {
    LOG_E("Unable to set font size.");
    return false;
  }

  current_font_size = size;
  return true;
}

bool 
TTF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_size)
{
  if (face != nullptr) clear_face();

  int error = FT_New_Memory_Face(library, (const FT_Byte *) buffer, buffer_size, 0, &face);
  if (error) {
    LOG_E("The memory font format is unsupported or is broken (%d).", error);
    if (buffer != nullptr) free(buffer);
    return false;
  }

  ready       = true;
  memory_font = buffer;
  return true;
}
