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

typedef int16_t FIX16;

    struct LigKernStep {
      uint8_t next_char_code;
      union {
        uint8_t char_code;
        uint8_t  kern_idx;  // Ligature: replacement char code, kern: displacement
      } u;
      unsigned int     stop:1;
      unsigned int     tag:1;               // 0 = Ligature, 1 = Kern
    };

static constexpr FIX16 advances[] = {
  /* 0 */  1079, 1439, 1343, 1198, 1151, 1294, 1247, 1343, 1247, 1343, 
  /* 10 */  1247, 1007, 959, 959, 1439, 1439, 479, 527, 863, 863, 
  /* 20 */  863, 863, 863, 1294, 767, 863, 1247, 1343, 863, 1559, 
  /* 30 */  1750, 1343, 479, 479, 863, 1439, 863, 1439, 1343, 479, 
  /* 40 */  671, 671, 863, 1343, 479, 575, 479, 863, 863, 863, 
  /* 50 */  863, 863, 863, 863, 863, 863, 863, 863, 479, 479, 
  /* 60 */  479, 1343, 815, 815, 1343, 1294, 1223, 1247, 1319, 1175, 
  /* 70 */  1127, 1355, 1294, 623, 887, 1342, 1079, 1582, 1294, 1343, 
  /* 80 */  1175, 1343, 1271, 959, 1247, 1294, 1294, 1774, 1294, 1294, 
  /* 90 */  1055, 479, 863, 479, 863, 479, 479, 863, 959, 767, 
  /* 100 */  959, 767, 527, 863, 959, 479, 527, 911, 479, 1439, 
  /* 110 */  959, 863, 959, 911, 671, 681, 671, 959, 911, 1247, 
  /* 120 */  911, 911, 767, 863, 1727, 863, 863, 863, 
};
static constexpr FIX16 depths[] = {
  /* 0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 10 */  0, 0, 0, 0, 0, 0, 0, 343, 0, 0, 
  /* 20 */  0, 0, 0, 0, 300, 0, 0, 0, 171, 0, 
  /* 30 */  0, 85, 0, 0, 0, 343, 98, 98, 0, 0, 
  /* 40 */  441, 441, 0, 134, 343, 0, 0, 441, 0, 0, 
  /* 50 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 343, 
  /* 60 */  343, -245, 343, 0, 0, 0, 0, 0, 0, 0, 
  /* 70 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 80 */  0, 343, 0, 0, 0, 0, 0, 0, 0, 0, 
  /* 90 */  0, 441, 0, 441, 0, 0, 0, 0, 0, 0, 
  /* 100 */  0, 0, 0, 343, 0, 0, 343, 0, 0, 0, 
  /* 110 */  0, 0, 343, 343, 0, 0, 0, 0, 0, 0, 
  /* 120 */  0, 343, 0, 0, 0, 0, 0, 0, 
};

static constexpr LigKernStep lig_kerns[] = {
/* 0 */  { .next_char_code = 0x6c, .u = { .kern_idx = 0 }, .stop = 0, .tag = 1 },
/* 1 */  { .next_char_code = 0x4c, .u = { .kern_idx = 1 }, .stop = 1, .tag = 1 },
/* 2 */  { .next_char_code = 0x69, .u = { .char_code = 0xc }, .stop = 0, .tag = 0 },
/* 3 */  { .next_char_code = 0x66, .u = { .char_code = 0xb }, .stop = 0, .tag = 0 },
/* 4 */  { .next_char_code = 0x6c, .u = { .char_code = 0xd }, .stop = 0, .tag = 0 },
/* 5 */  { .next_char_code = 0x27, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 6 */  { .next_char_code = 0x3f, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 7 */  { .next_char_code = 0x21, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 8 */  { .next_char_code = 0x29, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 9 */  { .next_char_code = 0x5d, .u = { .kern_idx = 2 }, .stop = 1, .tag = 1 },
/* 10 */  { .next_char_code = 0x69, .u = { .char_code = 0xe }, .stop = 0, .tag = 0 },
/* 11 */  { .next_char_code = 0x6c, .u = { .char_code = 0xf }, .stop = 0, .tag = 0 },
/* 12 */  { .next_char_code = 0x27, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 13 */  { .next_char_code = 0x3f, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 14 */  { .next_char_code = 0x21, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 15 */  { .next_char_code = 0x29, .u = { .kern_idx = 2 }, .stop = 0, .tag = 1 },
/* 16 */  { .next_char_code = 0x5d, .u = { .kern_idx = 2 }, .stop = 1, .tag = 1 },
/* 17 */  { .next_char_code = 0x60, .u = { .char_code = 0x5c }, .stop = 1, .tag = 0 },
/* 18 */  { .next_char_code = 0x27, .u = { .char_code = 0x22 }, .stop = 0, .tag = 0 },
/* 19 */  { .next_char_code = 0x3f, .u = { .kern_idx = 3 }, .stop = 0, .tag = 1 },
/* 20 */  { .next_char_code = 0x21, .u = { .kern_idx = 3 }, .stop = 1, .tag = 1 },
/* 21 */  { .next_char_code = 0x2d, .u = { .char_code = 0x7b }, .stop = 1, .tag = 0 },
/* 22 */  { .next_char_code = 0x2d, .u = { .char_code = 0x7c }, .stop = 1, .tag = 0 },
/* 23 */  { .next_char_code = 0x60, .u = { .char_code = 0x3c }, .stop = 1, .tag = 0 },
/* 24 */  { .next_char_code = 0x60, .u = { .char_code = 0x3e }, .stop = 1, .tag = 0 },
/* 25 */  { .next_char_code = 0x61, .u = { .kern_idx = 4 }, .stop = 0, .tag = 1 },
/* 26 */  { .next_char_code = 0x65, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 27 */  { .next_char_code = 0x61, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 28 */  { .next_char_code = 0x6f, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 29 */  { .next_char_code = 0x63, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 30 */  { .next_char_code = 0x41, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 31 */  { .next_char_code = 0x6f, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 32 */  { .next_char_code = 0x65, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 33 */  { .next_char_code = 0x61, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 34 */  { .next_char_code = 0x2e, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 35 */  { .next_char_code = 0x2c, .u = { .kern_idx = 6 }, .stop = 1, .tag = 1 },
/* 36 */  { .next_char_code = 0x6f, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 37 */  { .next_char_code = 0x65, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 38 */  { .next_char_code = 0x75, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 39 */  { .next_char_code = 0x72, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 40 */  { .next_char_code = 0x61, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 41 */  { .next_char_code = 0x41, .u = { .kern_idx = 7 }, .stop = 0, .tag = 1 },
/* 42 */  { .next_char_code = 0x4f, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 43 */  { .next_char_code = 0x43, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 44 */  { .next_char_code = 0x47, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 45 */  { .next_char_code = 0x51, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 46 */  { .next_char_code = 0x79, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 47 */  { .next_char_code = 0x65, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 48 */  { .next_char_code = 0x6f, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 49 */  { .next_char_code = 0x72, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 50 */  { .next_char_code = 0x61, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 51 */  { .next_char_code = 0x41, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 52 */  { .next_char_code = 0x75, .u = { .kern_idx = 6 }, .stop = 1, .tag = 1 },
/* 53 */  { .next_char_code = 0x58, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 54 */  { .next_char_code = 0x57, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 55 */  { .next_char_code = 0x41, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 56 */  { .next_char_code = 0x56, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 57 */  { .next_char_code = 0x59, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 58 */  { .next_char_code = 0x74, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 59 */  { .next_char_code = 0x75, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 60 */  { .next_char_code = 0x62, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 61 */  { .next_char_code = 0x79, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 62 */  { .next_char_code = 0x76, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 63 */  { .next_char_code = 0x77, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 64 */  { .next_char_code = 0x68, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 65 */  { .next_char_code = 0x6b, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 66 */  { .next_char_code = 0x65, .u = { .kern_idx = 8 }, .stop = 0, .tag = 1 },
/* 67 */  { .next_char_code = 0x6f, .u = { .kern_idx = 8 }, .stop = 0, .tag = 1 },
/* 68 */  { .next_char_code = 0x78, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 69 */  { .next_char_code = 0x64, .u = { .kern_idx = 8 }, .stop = 0, .tag = 1 },
/* 70 */  { .next_char_code = 0x63, .u = { .kern_idx = 8 }, .stop = 0, .tag = 1 },
/* 71 */  { .next_char_code = 0x71, .u = { .kern_idx = 8 }, .stop = 0, .tag = 1 },
/* 72 */  { .next_char_code = 0x76, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 73 */  { .next_char_code = 0x6a, .u = { .kern_idx = 9 }, .stop = 0, .tag = 1 },
/* 74 */  { .next_char_code = 0x79, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 75 */  { .next_char_code = 0x77, .u = { .kern_idx = 5 }, .stop = 1, .tag = 1 },
/* 76 */  { .next_char_code = 0x74, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 77 */  { .next_char_code = 0x43, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 78 */  { .next_char_code = 0x4f, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 79 */  { .next_char_code = 0x47, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 80 */  { .next_char_code = 0x55, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 81 */  { .next_char_code = 0x51, .u = { .kern_idx = 5 }, .stop = 0, .tag = 1 },
/* 82 */  { .next_char_code = 0x54, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 83 */  { .next_char_code = 0x59, .u = { .kern_idx = 6 }, .stop = 0, .tag = 1 },
/* 84 */  { .next_char_code = 0x56, .u = { .kern_idx = 7 }, .stop = 0, .tag = 1 },
/* 85 */  { .next_char_code = 0x57, .u = { .kern_idx = 7 }, .stop = 1, .tag = 1 },
/* 86 */  { .next_char_code = 0x6a, .u = { .kern_idx = 8 }, .stop = 1, .tag = 1 },
/* 87 */  { .next_char_code = 0x49, .u = { .kern_idx = 8 }, .stop = 1, .tag = 1 },
};

static constexpr FIX16 kerns[] = {
  /* 0 */  -479, -551, 123, 191, -95, -47, -143, -191, 47, 95, 
};

static constexpr int16_t lig_kern_indexes[] = {
  /* 0 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 20 */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 23, -1, -1, -1, -1, -1, 18, 
  /* 40 */  -1, -1, -1, -1, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  /* 60 */  -1, -1, -1, 24, -1, 76, -1, -1, 53, -1, 36, -1, -1, 87, -1, 42, 82, -1, -1, 53, 
  /* 80 */  30, -1, 76, -1, 46, -1, 36, 36, 42, 47, -1, -1, -1, -1, -1, -1, 17, 72, 66, 64, 
  /* 100 */  -1, -1, 2, 86, 58, -1, -1, 25, -1, 58, 58, 66, 66, -1, -1, -1, 74, 75, 25, 26, 
  /* 120 */  -1, 31, -1, 22, -1, -1, -1, -1, 
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

int32_t translate(int32_t charcode)
{
  int32_t glyph_code = charcode;

  if ((charcode >= ' ') && (charcode < '{')) {
    glyph_code = charcode;
  }
  else {
    switch (charcode) {
      case 0x2018:
      case 0x02BB: glyph_code = 0x60; break;
      case 0x2019:
      case 0x02BC: glyph_code = 0x27; break;
      case 0x201C: glyph_code = 0x5C; break;
      case 0x201D: glyph_code = 0x22; break;
      default:
        glyph_code = ' ';
    }
  }

  return glyph_code;
}

Font::Glyph *
IBMF::get_glyph(int32_t charcode, int32_t next_charcode, int16_t glyph_size, int16_t & kern, bool & ignore_next)
{
  std::scoped_lock guard(mutex);

  int32_t glyph_code = translate(charcode);

  ignore_next = false;
  Glyph * glyph = get_glyph_internal(glyph_code, glyph_size);

  if ((glyph != nullptr) && (glyph->ligature_and_kern_pgm_index >= 0)) {
    int16_t k; // THis is a FIX16...
    glyph = adjust_ligature_and_kern(glyph, glyph_size, next_charcode, k, ignore_next);
    if (k != 0) {
      kern = (advances[glyph_code] + k) >> 6;
    }
    else kern = glyph->advance;
  }
  else {
    kern = advances[glyph_code] >> 6;
  }
  return (glyph == nullptr) ? nullptr : glyph;
}

Font::Glyph *
IBMF::get_glyph(int32_t charcode, int16_t glyph_size)
{
  std::scoped_lock guard(mutex);
  
  int32_t glyph_code = translate(charcode);

  return get_glyph_internal(glyph_code, glyph_size);
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

  last_charcode = charcode;

  uint8_t glyph_code = charcode;

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

    if ((glyph_code == ' ') || (glyph_code == 160)) {
      glyph->dim.width   =  0;
      glyph->dim.height  =  0;
      glyph->line_height =  face->get_line_height();
      glyph->pitch       =  0;
      glyph->xoff        =  0;
      glyph->yoff        =  0;
      glyph->advance     =  8;
      glyph->ligature_and_kern_pgm_index = -1;
    }
    else if (face->get_glyph(glyph_code, &pk_glyph, true)) {
      glyph->dim.width   = pk_glyph->bitmap_width;
      glyph->dim.height  = pk_glyph->bitmap_height;
      glyph->line_height = face->get_line_height();
      glyph->pitch       = (pk_glyph->bitmap_width + 7) >> 3;
      glyph->xoff        = -pk_glyph->horizontal_offset;
      glyph->yoff        = -pk_glyph->vertical_offset;
      glyph->advance     =  advances[glyph_code] >> 6;
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
IBMF::adjust_ligature_and_kern(Glyph   * glyph, 
                               uint16_t  glyph_size, 
                               uint32_t  next_charcode, 
                               int16_t & kern,
                               bool    & ignore_next)
{
  ignore_next = false;
  kern = 0;
  if (glyph->ligature_and_kern_pgm_index != -1) {
    const LigKernStep * step = &lig_kerns[glyph->ligature_and_kern_pgm_index];
    while (true) {
      if (step->tag == 1) {
        if (step->next_char_code == next_charcode) {
          kern = kerns[step->u.kern_idx];
          LOG_D("Kerning between %c and %c: %d", (char) last_charcode, (char) next_charcode, kern);
          break;
        }
      }
      else {
        if (step->next_char_code == next_charcode) {
          LOG_D("Ligature between %c and %c", (char) last_charcode, (char) next_charcode);
          glyph = get_glyph_internal(step->u.char_code, glyph_size);
          ignore_next = true;
          break;
        }
      }
      if (step->stop == 1) break;
      step++;
    }
  }
  return glyph;
}