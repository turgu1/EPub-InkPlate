// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _FONT_ 1
#include "models/font.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

Font::Font()
{
  memory_font       = nullptr;
  current_font_size = -1;
  ready             = false;
 }

void
Font::add_buff_to_byte_pool()
{
  BytePool * pool = (BytePool *) allocate(BYTE_POOL_SIZE);
  if (pool == nullptr) {
    LOG_E("Unable to allocated memory for bytes pool.");
    msg_viewer.out_of_memory("ttf pool allocation");
  }
  byte_pools.push_front(pool);

  byte_pool_idx = 0;
}

uint8_t * 
Font::byte_pool_alloc(uint16_t size)
{
  if (size > BYTE_POOL_SIZE) {
    LOG_E("Byte Pool Size NOT BIG ENOUGH!!!");
    std::abort();
  }
  if (byte_pools.empty() || (byte_pool_idx + size) > BYTE_POOL_SIZE) {
    LOG_D("Adding new Byte Pool buffer.");
    add_buff_to_byte_pool();
  }

  uint8_t * buff = &(*byte_pools.front())[byte_pool_idx];
  byte_pool_idx += size;

  return buff;
}

void
Font::clear_cache()
{
  std::scoped_lock guard(mutex);
  
  LOG_D("Clear cache...");
  for (auto const & entry : cache) {
    for (auto const & glyph : entry.second) {
      bitmap_glyph_pool.deleteElement(glyph.second);      
    }
  }

  for (auto * buff : byte_pools) {
    free(buff);
  }
  byte_pools.clear();
  
  cache.clear();
  cache.reserve(50);
}

Font::Glyph *
Font::get_glyph(uint32_t charcode, int16_t glyph_size)
{
  std::scoped_lock guard(mutex);

  return get_glyph_internal(charcode, glyph_size);
}


Font::Glyph *
Font::get_glyph(uint32_t charcode, uint32_t next_charcode, int16_t glyph_size, int16_t & kern, bool & ignore_next)
{
  std::scoped_lock guard(mutex);

  ignore_next = false;
  Font::Glyph * glyph = get_glyph_internal(charcode, glyph_size);

  if (glyph != nullptr) {
    if (glyph->ligature_and_kern_pgm_index >= 0) {
      int16_t k; // This is a FIX16...
      glyph = adjust_ligature_and_kern(glyph, glyph_size, next_charcode, k, ignore_next);
      kern = glyph->advance + k;
    }
    else {
      kern = glyph->advance;
    }
  }
  return (glyph == nullptr) ? nullptr : glyph;
}

bool 
Font::set_font_face_from_file(const std::string font_filename)
{
  LOG_D("set_font_face_from_file() ...");

  FILE * font_file;
  if ((font_file = fopen(font_filename.c_str(), "r")) == nullptr) {
    LOG_E("set_font_face_from_file: Unable to open font file '%s'", font_filename.c_str());
    return false;
  }
  else {
    uint8_t * buffer;

    struct stat stat_buf;
    fstat(fileno(font_file), &stat_buf);
    int32_t length = stat_buf.st_size;
    
    LOG_D("Font File Length: %d", length);

    buffer = (uint8_t *) allocate(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %d", (int32_t) (length + 1));
      msg_viewer.out_of_memory("font buffer allocation");
    }

    if (fread(buffer, length, 1, font_file) != 1) {
      LOG_E("set_font_face_from_file: Unable to read file content");
      fclose(font_file);
      free(buffer);
      return false;
    }

    fclose(font_file);

    buffer[length] = 0;

    return set_font_face_from_memory(buffer, length);
  }
}

void
Font::get_size(const char * str, Dim * dim, int16_t glyph_size)
{
  int16_t max_up   = 0;
  int16_t max_down = 0;

  dim->width  = 0;

  { std::scoped_lock guard(mutex);
  
    while (*str) {
      Glyph * glyph = get_glyph_internal(*str++, glyph_size);
      if (glyph) {
        dim->width += glyph->advance;

        int16_t up   = -glyph->yoff;
        int16_t down =  glyph->dim.height + glyph->yoff;
      
        if (up   > max_up  ) max_up   = up;
        if (down > max_down) max_down = down;
      }
    }

    dim->height = max_up + max_down;
  }
}