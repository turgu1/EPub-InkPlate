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

    struct LigKernStep {
      uint8_t next_char_code;
      union {
        uint8_t char_code;
        int8_t  displacement;  // Ligature: replacement char code, kern: displacement
      } u;
      unsigned int     stop:1;
      unsigned int     tag:1;               // 0 = Ligature, 1 = Kern
    };


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

int16_t lig_kern_indexes[] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0, 23, -1, -1, -1, -1, -1, 18, 
  -1, -1, -1, -1, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, 24, -1, 76, -1, -1, 53, -1, 36, -1, -1, 87, -1, 42, 82, -1, -1, 53, 
  30, -1, 76, -1, 46, -1, 36, 36, 42, 47, -1, -1, -1, -1, -1, -1, 17, 72, 66, 64, 
  -1, -1,  2, 86, 58, -1, -1, 25, -1, 58, 58, 66, 66, -1, -1, -1, 74, 75, 25, 26, 
  -1, 31, -1, 22, -1, -1, -1, -1, 
};

LigKernStep lig_kerns[] = {
  { .next_char_code = 0x6c, .u = { .displacement = -7 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x4c, .u = { .displacement = -9 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x69, .u = { .char_code = 0xc }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x66, .u = { .char_code = 0xb }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x6c, .u = { .char_code = 0xd }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x27, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x3f, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x21, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x29, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x5d, .u = { .displacement = 2 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x69, .u = { .char_code = 0xe }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x6c, .u = { .char_code = 0xf }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x27, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x3f, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x21, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x29, .u = { .displacement = 2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x5d, .u = { .displacement = 2 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x60, .u = { .char_code = 0x5c }, .stop = 1, .tag = 0 },
  { .next_char_code = 0x27, .u = { .char_code = 0x22 }, .stop = 0, .tag = 0 },
  { .next_char_code = 0x3f, .u = { .displacement = 3 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x21, .u = { .displacement = 3 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x2d, .u = { .char_code = 0x7b }, .stop = 1, .tag = 0 },
  { .next_char_code = 0x2d, .u = { .char_code = 0x7c }, .stop = 1, .tag = 0 },
  { .next_char_code = 0x60, .u = { .char_code = 0x3c }, .stop = 1, .tag = 0 },
  { .next_char_code = 0x60, .u = { .char_code = 0x3e }, .stop = 1, .tag = 0 },
  { .next_char_code = 0x61, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x65, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x61, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6f, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x63, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x41, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6f, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x65, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x61, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x2e, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x2c, .u = { .displacement = -2 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x6f, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x65, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x75, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x72, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x61, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x41, .u = { .displacement = -3 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x4f, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x43, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x47, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x51, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x79, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x65, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6f, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x72, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x61, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x41, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x75, .u = { .displacement = -2 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x58, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x57, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x41, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x56, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x59, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x74, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x75, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x62, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x79, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x76, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x77, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x68, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6b, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x65, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6f, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x78, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x64, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x63, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x71, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x76, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x6a, .u = { .displacement = 1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x79, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x77, .u = { .displacement = -1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x74, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x43, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x4f, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x47, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x55, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x51, .u = { .displacement = -1 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x54, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x59, .u = { .displacement = -2 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x56, .u = { .displacement = -3 }, .stop = 0, .tag = 1 },
  { .next_char_code = 0x57, .u = { .displacement = -3 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x6a, .u = { .displacement = 1 }, .stop = 1, .tag = 1 },
  { .next_char_code = 0x49, .u = { .displacement = 1 }, .stop = 1, .tag = 1 },
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

  uint8_t glyph_code;

  // if (translate) {
  //   if ((char_code > ' ') && (char_code < '{')) {
  //     glyph_code = char_code;
  //   }
  //   else {
  //     switch (char_code) {
  //       case 0x2018:
  //       case 0x02BB: glyph_code = 0x60; break;
  //       case 0x2019:
  //       case 0x02BC: glyph_code = 0x27; break;
  //       case 0x201C: glyph_code = 0x5C; break;
  //       case 0x201D: glyph_code = 0x22; break;
  //       default:
  //         glyph_code = ' ';
  //     }
  //   }
  // }
  // else {
    glyph_code = charcode;
  // }

  bool found = (cache_it != cache.end()) &&
               ((git = cache_it->second.find(glyph_code)) != cache_it->second.end());

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

    if (glyph_code == ' ') {
      glyph->dim.width   =  0;
      glyph->dim.height  =  0;
      glyph->line_height =  face->get_line_height();
      glyph->pitch       =  0;
      glyph->xoff        =  0;
      glyph->yoff        =  0;
      glyph->advance     =  6;
      glyph->ligature_and_kern_pgm_index = -1;
    }
    else if (face->get_glyph(glyph_code, &pk_glyph, true)) {
      glyph->dim.width   = pk_glyph->bitmap_width;
      glyph->dim.height  = pk_glyph->bitmap_height;
      glyph->line_height = face->get_line_height();
      glyph->pitch       = (pk_glyph->bitmap_width + 7) >> 3;
      glyph->xoff        = -pk_glyph->horizontal_offset;
      glyph->yoff        = -pk_glyph->vertical_offset;
      glyph->advance     =  advances[glyph_code];
      glyph->ligature_and_kern_pgm_index = lig_kern_indexes[glyph_code];
    }
    else {
      bitmap_glyph_pool.deallocate(glyph);
      LOG_E("Unable to render glyph for glyph_code: %d error: %d", glyph_code, error);
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

Font::Glyph * 
IBMF::adjust_ligature_and_kern(Glyph * glyph, uint16_t glyph_size, uint32_t next_charcode, bool & ignore_next)
{
  if (glyph->ligature_and_kern_pgm_index != -1) {
    LigKernStep * step = &lig_kerns[glyph->ligature_and_kern_pgm_index];
    while (true) {
      if (step->tag == 1) {
        glyph->advance += step->u.displacement;
      }
      else {
        if (step->next_char_code == next_charcode) {
          glyph = get_glyph_internal(step->u.char_code, glyph_size);
          ignore_next = true;
          return glyph;
        }
      }
      if (step->stop == 1) break;
      step++;
    }
  }
  else {
    ignore_next = false;
  }
  return glyph;
}