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

  if (glyph != nullptr) {
    if (glyph->ligature_and_kern_pgm_index >= 0) {
      IBMFFont::FIX16 k;
      glyph = adjust_ligature_and_kern(glyph, glyph_size, next_charcode, k, ignore_next);
      kern = (k == 0) ? glyph->advance : ((glyph_data->advance + k) >> 6);
    }
    else {
      kern =  glyph->advance;
    }
  }

  return glyph;
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

  uint8_t glyph_code = charcode;

  bool found = (cache_it != cache.end()) &&
               ((git = cache_it->second.find(glyph_code)) != cache_it->second.end());

  if (found) {
    if (current_font_size != glyph_size) set_font_size(glyph_size);
    glyph_data = face->get_glyph_data(glyph_code);
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

    glyph_data = nullptr;

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
    else if (!face->get_glyph(glyph_code, *glyph, &glyph_data, true)) {
      bitmap_glyph_pool.deallocate(glyph);
      LOG_E("Unable to render glyph for glyph_code: %d error: %d", glyph_code, error);
      return nullptr;
    }

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
  if (face->set_font_size(size)) {  
    current_font_size = size;
    return true;
  }
  else {
    current_font_size = -1;
    return false;
  }
}

bool 
IBMF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_size)
{
  if (face != nullptr) clear_face();

  face = new IBMFFont(buffer, buffer_size, *this);

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

  if ((next_charcode != 0) && (glyph->ligature_and_kern_pgm_index != 255)) {
    const IBMFFont::LigKernStep * step = face->get_lig_kern(glyph->ligature_and_kern_pgm_index);
    while (true) {
      if (step->tag == 1) {
        if (step->next_char_code == next_charcode) {
          kern = face->get_kern(step->u.kern_idx);
          LOG_D("Kerning between %c and %c: %d", (char) glyph_data->char_code, (char) next_charcode, kern);
          break;
        }
      }
      else {
        if (step->next_char_code == next_charcode) {
          LOG_D("Ligature between %c and %c", (char) glyph_data->char_code, (char) next_charcode);
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