// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _IBMF_ 1
#include "models/ibmf.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

static constexpr uint8_t advances[] = {
  16, 22, 20, 18, 17, 20, 19, 20, 19, 20, 
  19, 15, 14, 14, 22, 22,  7,  8, 13, 13, 
  13, 13, 13, 20, 11, 13, 19, 20, 13, 24, 
  27, 20,  7,  7, 13, 22, 13, 22, 20,  7, 
  10, 10, 13, 20,  7,  8,  7, 13, 13, 13, 
  13, 13, 13, 13, 13, 13, 13, 13,  7,  7,
   7, 20, 12, 12, 20, 20, 19, 19, 20, 18, 
  17, 21, 20,  9, 13, 20, 16, 24, 20, 20, 
  18, 20, 19, 14, 19, 20, 20, 27, 20, 20, 
  16,  7, 13,  7, 13,  7,  7, 13, 14, 11, 
  14, 11,  8, 13, 14,  7,  8, 14,  7, 22, 
  14, 13, 14, 14, 10, 10, 10, 14, 14, 19, 
  14, 14, 11, 13, 26, 13, 13, 13, 
};

static constexpr uint8_t depths[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0, 5, 1, 1, 0, 0, 6, 6, 0, 2, 5, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 5, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 
};


IBMF::IBMF(const std::string & filename) : Font()
{
  face  = nullptr;

  set_font_face_from_file(filename);
}

IBMF::IBMF(unsigned char * buffer, int32_t buffer_size) : Font()
{
  face  = nullptr;
 
  set_font_face_from_memory(buffer, buffer_size);
}

IBMF::~IBMF()
{
  if (face != nullptr) clear_face();
}

void
IBMF::clear_face()
{
  clear_cache();
  if (face != nullptr) {
    delete face;
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
#if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
  IBMF::get_glyph_internal(int32_t charcode, int16_t glyph_size, bool debugging)
#else
  IBMF::get_glyph_internal(int32_t charcode, int16_t glyph_size)
#endif
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
    #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
      static bool first_debug = true;
      if (debugging && first_debug) {
        LOG_D("get_glyph_internal()");
        ESP::show_heaps_info();
      }
    #endif

    if (current_font_size != glyph_size) set_font_size(glyph_size);

    Glyph * glyph = bitmap_glyph_pool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      msg_viewer.out_of_memory("glyph allocation");
    }

    PKFont::Glyph * pk_glyph = nullptr;
    uint8_t glyph_code;
    if (charcode == ' ') {
      glyph->dim.width   =  0;
      glyph->dim.height  =  0;
      glyph->line_height =  face->get_line_height();
      glyph->pitch       =  0;
      glyph->xoff        =  0;
      glyph->yoff        =  0;
      glyph->advance     =  6;
    }
    else if (face->get_glyph(charcode, &pk_glyph, true, glyph_code)) {
      glyph->dim.width   = pk_glyph->bitmap_width;
      glyph->dim.height  = pk_glyph->bitmap_height;
      glyph->line_height = face->get_line_height();
      glyph->pitch       = (pk_glyph->bitmap_width + 7) >> 3;
      glyph->xoff    = -pk_glyph->horizontal_offset;
      glyph->yoff    = -pk_glyph->vertical_offset;
      glyph->advance =  advances[glyph_code];

    }
    else {
      bitmap_glyph_pool.deallocate(glyph);
      LOG_E("Unable to render glyph for charcode: %d error: %d", charcode, error);
      return nullptr;
    }

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

      memcpy(glyph->buffer, pk_glyph->bitmap, size);
    }
    else {
      glyph->buffer = nullptr;
    }      
    
    face->release_bitmap();

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

    #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
      if (debugging && first_debug) {
        LOG_D("end of get_glyph_internal()");
        ESP::show_heaps_info();
        first_debug = false;
      }
    #endif

    return glyph;
  }
}

bool 
IBMF::set_font_size(int16_t size)
{
  // TBD
  
  current_font_size = size;
  return true;
}

bool 
IBMF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_size)
{
  if (face != nullptr) clear_face();

  face = new PKFont(buffer, buffer_size);
  if (face == nullptr) {
    LOG_E("The memory font format is unsupported or is broken.");
    if (buffer != nullptr) free(buffer);
    return false;
  }

  ready       = true;
  memory_font = buffer;
  return true;
}
