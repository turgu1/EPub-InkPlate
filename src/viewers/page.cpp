// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PAGE__ 1
#include "viewers/page.hpp"
#include "viewers/msg_viewer.hpp"
#include "screen.hpp"

#include <iostream>

//--- Image load to bitmap tool ---
//
// Keep only PNG, JPEG
// No load from a file (will come from the epub file as a char array)

#define STBI_ONLY_JPEG
#define STBI_SUPPORT_ZLIB
#define STBI_ONLY_PNG

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT

#include "stb_image.h"
#include "stb_image_resize.h"

#include <string>
#include <algorithm>

static void
no_mem()
{
  msg_viewer.out_of_memory("display list allocation");
}

Page::Page() :
  compute_mode(ComputeMode::DISPLAY), 
  screen_is_full(false)
{
  clear_display_list();
  clear_line_list();
}

Page::~Page()
{
  clear_display_list();
  clear_line_list();
}

void
Page::clear_display_list()
{
  for (auto * entry: display_list) {
    if (entry->command == DisplayListCommand::IMAGE) {
      if (entry->kind.image_entry.image.bitmap) {
        delete [] entry->kind.image_entry.image.bitmap;
      }
    }
    display_list_entry_pool.deleteElement(entry);
    //entry = nullptr;
  }
  display_list.clear();
}

// 00000000 -- 0000007F: 	0xxxxxxx
// 00000080 -- 000007FF: 	110xxxxx 10xxxxxx
// 00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
// 00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

int32_t 
to_unicode(const char **str, CSS::TextTransform transform, bool first) 
{
  const uint8_t * c    = (uint8_t *) *str;
  int32_t         u    = 0;
  bool            done = false;

  if (*c == '&') {
    const uint8_t * s = ++c;
    uint8_t len = 0;
    while ((len < 6) && (*s != 0) && (*s != ';')) { s++; len++; }
    if (*s == ';') {
      if      (strncmp("nbsp;", (const char *) c, 5) == 0) u = 160;
      else if (strncmp("lt;",   (const char *) c, 3) == 0) u =  60;
      else if (strncmp("gt;",   (const char *) c, 3) == 0) u =  62;
      else if (strncmp("amp;",  (const char *) c, 4) == 0) u =  38;
      else if (strncmp("quot;", (const char *) c, 5) == 0) u =  34;
      else if (strncmp("apos;", (const char *) c, 5) == 0) u =  39;
      if (u == 0) {
        u = '&';
      }
      else {
        c = ++s;
      }
    }
    else {
      u = '&';
    }
    done = true;
  } 
  else while (true) {
    if ((*c & 0x80) == 0x00) {
      u = *c++;
      done = true;
    }
    else if ((**str & 0xF1) == 0xF0) {
      u = *c++ & 0x07;
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      done = true;
    }
    else if ((**str & 0xF0) == 0xE0) {
      u = *c++ & 0x0F;
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      done = true;
    }
    else if ((**str & 0xE0) == 0xC0) {
      u = *c++ & 0x1F;
      if (*c == 0) break; else u = (u << 6) + (*c++ & 0x3F);
      done = true;
    }
    break;
  }
  *str = (char *) c;
  if (transform != CSS::TextTransform::NONE) {
    if      (transform == CSS::TextTransform::UPPERCASE) u = toupper(u);
    else if (transform == CSS::TextTransform::LOWERCASE) u = tolower(u);
    else if (first && (transform == CSS::TextTransform::CAPITALIZE)) u = toupper(u);
  }
  return done ? u : ' ';
}

void 
Page::put_str_at(const std::string & str, Pos pos, const Format & fmt)
{
  TTF::BitmapGlyph * glyph;
  
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  const char * s = str.c_str(); 
  if (fmt.align == CSS::Align::LEFT) {
    bool first = true;
    while (*s) {
      if ((glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first)))) {
        DisplayListEntry * entry = display_list_entry_pool.newElement();
        if (entry == nullptr) no_mem();
        entry->command = DisplayListCommand::GLYPH;
        entry->kind.glyph_entry.glyph = glyph;
        entry->pos.x = pos.x + glyph->xoff;
        entry->pos.y = pos.y + glyph->yoff;

        #if DEBUGGING
          if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
            LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
          }
          else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
            LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
          }
        #endif

        display_list.push_front(entry);
      
        pos.x += glyph->advance;
      }
      first = false;
    }
  }
  else {
    int16_t size = 0;
    const char * s = str.c_str();

    while (*s) {
      bool first = true;
      if ((glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first)))) {
        size += glyph->advance;
      }
      first = false;
    }

    int16_t x;

    if (pos.x == -1) {
      if (fmt.align == CSS::Align::CENTER) {
        x = min_x + ((max_x - min_x) >> 1) - (size >> 1);
      }
      else { // RIGHT
        x = max_x - size;
      }
    }
    else {
      if (fmt.align == CSS::Align::CENTER) {
        x = pos.x - (size >> 1);
      }
      else { // RIGHT
        x = pos.x - size;
      }
    }

    s = str.c_str();
    bool first = true;
    while (*s) {
      if ((glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first)))) {
        
        DisplayListEntry * entry = display_list_entry_pool.newElement();
        if (entry == nullptr) no_mem();

        entry->command                = DisplayListCommand::GLYPH;
        entry->kind.glyph_entry.glyph = glyph;
        entry->pos.x                  = x + glyph->xoff;
        entry->pos.y                  = pos.y + glyph->yoff;

        #if DEBUGGING
          if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
            LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
          }
          else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
            LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
          }
        #endif

        display_list.push_front(entry);
      
        x += glyph->advance;
      }
      first = false;
    }
  }
}

void 
Page::put_char_at(char ch, Pos pos, const Format & fmt)
{
  TTF::BitmapGlyph * glyph;
  
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if ((glyph = font->get_glyph(ch))) {
    DisplayListEntry * entry = display_list_entry_pool.newElement();
    if (entry == nullptr) no_mem();
    entry->command                = DisplayListCommand::GLYPH;
    entry->kind.glyph_entry.glyph = glyph;
    entry->pos.x                  = pos.x + glyph->xoff;
    entry->pos.y                  = pos.y + glyph->yoff;

    #if DEBUGGING
      if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
        LOG_E("Put_char_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
      }
      else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
        LOG_E("Put_char_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
      }
    #endif

    display_list.push_front(entry);
  }  
}

void
Page::paint(bool clear_screen, bool no_full, bool do_it)
{
  if (!do_it) if ((display_list.empty()) || (compute_mode != ComputeMode::DISPLAY)) return;
  
  if (clear_screen) screen.clear();

  display_list.reverse();

  for (auto * entry : display_list) {
    if (entry->command == DisplayListCommand::GLYPH) {
      if (entry->kind.glyph_entry.glyph != nullptr) {
        screen.draw_glyph(
          entry->kind.glyph_entry.glyph->buffer,
          entry->kind.glyph_entry.glyph->dim,
          entry->pos,
          entry->kind.glyph_entry.glyph->pitch);
      }
      else {
        LOG_E("DISPLAY LIST CORRUPTED!!");
      }
    }
    else if (entry->command == DisplayListCommand::IMAGE) {
      screen.draw_bitmap(
        entry->kind.image_entry.image.bitmap, 
        entry->kind.image_entry.image.dim,  
        entry->pos);
    }
    else if (entry->command == DisplayListCommand::HIGHLIGHT) {
      screen.draw_rectangle(
        entry->kind.region_entry.dim, 
        entry->pos, 
        Screen::BLACK_COLOR);
    }
    else if (entry->command == DisplayListCommand::CLEAR_HIGHLIGHT) {
      screen.draw_rectangle(
        entry->kind.region_entry.dim, 
        entry->pos,
        Screen::WHITE_COLOR);
    }
    else if (entry->command == DisplayListCommand::CLEAR_REGION) {
      screen.colorize_region(
        entry->kind.region_entry.dim, 
        entry->pos,
        Screen::WHITE_COLOR);
    }
    else if (entry->command == DisplayListCommand::SET_REGION) {
      screen.colorize_region(
        entry->kind.region_entry.dim, 
        entry->pos,
        Screen::BLACK_COLOR);
    }
  };

  screen.update(no_full);
}

void
Page::start(Format & fmt)
{
  pos.x = fmt.screen_left;
  pos.y = fmt.screen_top;

  min_y = fmt.screen_top;
  max_x = Screen::WIDTH  - fmt.screen_right;
  max_y = Screen::HEIGHT - fmt.screen_bottom;
  min_x = fmt.screen_left;

  para_min_x = min_x;
  para_max_x = max_x;

  screen_is_full = false;

  clear_display_list();
  clear_line_list();

  para_indent = 0;
  line_width  = 0;

  // show_controls();
}

void
Page::set_limits(Format & fmt)
{
  pos.x = pos.y = 0;

  min_y = fmt.screen_top;
  max_x = Screen::WIDTH  - fmt.screen_right;
  max_y = Screen::HEIGHT - fmt.screen_bottom;
  min_x = fmt.screen_left;

  screen_is_full = false;

  clear_line_list();

  para_indent = 0;
  line_width  = 0;
  top_margin  = 0;
}

bool
Page::line_break(const Format & fmt, int8_t indent_next_line)
{
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);
  
  // LOG_D("line_break font index: %d", fmt.font_index);

  if (!line_list.empty()) {
    add_line(fmt, false);
  }
  else {
    pos.y += font->get_line_height() * fmt.line_height_factor;
    pos.x  = min_x + indent_next_line;
  }

  screen_is_full = screen_is_full || ((pos.y - font->get_descender_height()) >= max_y);
  if (screen_is_full) {
    return false;
  }
  return true;
}

bool 
Page::new_paragraph(const Format & fmt, bool recover)
{
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  // Check if there is enough room for the first line of the paragraph.
  if (!recover) {
    screen_is_full = screen_is_full || 
                    ((pos.y + fmt.margin_top + 
                             (fmt.line_height_factor * font->get_line_height()) - 
                              font->get_descender_height()) > max_y);
    if (screen_is_full) { 
      return false; 
    }
  }

  para_min_x = min_x + fmt.margin_left;
  para_max_x = max_x - fmt.margin_right;

 if (fmt.indent < 0) {
    para_min_x -= fmt.indent;
  }

  // When recover == true, that means we are recovering the end of a paragraph that appears at the
  // beginning of a page. top_margin and indent must then be forgot as the have already been used at
  // the end of the page before...
  
  if (recover) {
    para_indent = top_margin = 0;
  }
  else {
    para_indent = fmt.indent;
    top_margin  = fmt.margin_top;
  }

  return true;
}

bool 
Page::end_paragraph(const Format & fmt)
{
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (!line_list.empty()) {
    add_line(fmt, false);

    pos.y += fmt.margin_bottom - font->get_descender_height();

    if ((pos.y - font->get_descender_height()) >= max_y) {
      screen_is_full = true;
      return false;
    }
  }

  return true;
}

void
Page::add_line(const Format & fmt, bool justifyable)
{
  if (pos.y == 0) pos.y = min_y;

  //show_display_list(line_list, "LINE");
  pos.x = para_min_x + para_indent;
  if (pos.x < min_x) pos.x = min_x;
  int16_t line_height = glyphs_height * fmt.line_height_factor;
  pos.y += top_margin + line_height; // (line_height >> 1) + (glyphs_height >> 1);

  #if DEBUGGING_AID
    if (show_location) {
      std::cout << "New Line: ";
      show_controls(" ");
      show_fmt(fmt, "   ->  ");  
    }
  #endif

  // Get rid of space characters that are at the end of the line.
  // This is mainly required for the JUSTIFY alignment algo.

  while (!line_list.empty()) {
    if ((*(line_list.begin()))->pos.y > 0) {
      DisplayListEntry * entry = *(line_list.begin());
      if (entry->command == DisplayListCommand::IMAGE) {
        if (entry->kind.image_entry.image.bitmap) delete [] entry->kind.image_entry.image.bitmap;
      }
      display_list_entry_pool.deleteElement(entry);
      line_list.pop_front(); 
    }
    else break;
  }

  line_list.reverse();
  
  if (!line_list.empty() && (compute_mode == ComputeMode::DISPLAY)) {
  
    if ((fmt.align == CSS::Align::JUSTIFY) && justifyable) {
      int16_t target_width = (para_max_x - para_min_x - para_indent);
      int16_t loop_count = 0;
      while ((line_width < target_width) && (++loop_count < 50)) {
        bool at_least_once = false;
        for (auto * entry : line_list) {
          if (entry->pos.x > 0) {
            at_least_once = true;
            entry->pos.x++;
            if (++line_width >= target_width) break;
          }
        }
        if (!at_least_once) break; // No space available in line to justify the line
      }
      if (loop_count >= 50) {
        for (auto * entry : line_list) entry->pos.x = 0;
      }
    }
    else {
      if (fmt.align == CSS::Align::RIGHT) {
        pos.x = para_max_x - line_width;
      } 
      else if (fmt.align == CSS::Align::CENTER) {
        pos.x = para_min_x + ((para_max_x - para_min_x) >> 1) - (line_width >> 1);
      }
    }
  }
  
  for (auto * entry : line_list) {
    if (entry->command == DisplayListCommand::GLYPH) {
      int16_t x = entry->pos.x; // x may contains the calculated gap between words
      entry->pos.x = pos.x + entry->kind.glyph_entry.glyph->xoff;
      entry->pos.y = pos.y + entry->kind.glyph_entry.glyph->yoff;
      pos.x += (x == 0) ? entry->kind.glyph_entry.glyph->advance : x;
    }
    else if (entry->command == DisplayListCommand::IMAGE) {
      entry->pos.x = pos.x;
      entry->pos.y = pos.y - entry->kind.image_entry.image.dim.height;
      pos.x += entry->kind.image_entry.advance;
    }
    else {
      LOG_E("Wrong entry type for add_line: %d", (int)entry->command);
    }

    #if DEBUGGING
      if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
        LOG_E("add_line entry with a negative location: %d %d %d", entry->pos.x, entry->pos.y, entry->command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
      else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
        LOG_E("add_line with a too large location: %d %d %d", entry->pos.x, entry->pos.y, entry->command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
    #endif

    display_list.push_front(entry);
  };
  
  line_width = line_height = glyphs_height = 0;
  clear_line_list();

  para_indent = 0;
  top_margin  = 0;

  // std::cout << "End New Line:"; show_controls();
}

inline void 
Page::add_glyph_to_line(TTF::BitmapGlyph * glyph, TTF & font, bool is_space)
{
  if (is_space && (line_width == 0)) return;

  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command = DisplayListCommand::GLYPH;
  entry->kind.glyph_entry.glyph = glyph;
  entry->pos.x = entry->pos.y = is_space ? glyph->advance : 0;
  
  if (glyphs_height < glyph->root->get_line_height()) glyphs_height = glyph->root->get_line_height();

  line_width += (glyph->advance);

  line_list.push_front(entry);
}

void 
Page::add_image_to_line(Image & image, int16_t advance, bool copy, float line_height_factor)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command = DisplayListCommand::IMAGE;
  entry->kind.image_entry.image   = image;
  entry->kind.image_entry.advance = advance;
  
  if (compute_mode == ComputeMode::DISPLAY) {
    if (copy) {
      int32_t size = image.dim.width * image.dim.height;
      if (image.bitmap != nullptr) {
        if ((entry->kind.image_entry.image.bitmap = new unsigned char[size]) == nullptr) {
          msg_viewer.out_of_memory("image bitmap allocation");
        }
        memcpy((void *) entry->kind.image_entry.image.bitmap, image.bitmap, size);
      }
      else {
        entry->kind.image_entry.image.bitmap = nullptr;
      }
    }
    else {
      entry->kind.image_entry.image.bitmap = image.bitmap;
    }
  }
  else {
    entry->kind.image_entry.image.bitmap = nullptr;
  }
  entry->pos.x = entry->pos.y = 0;
  
  if (glyphs_height < image.dim.height) glyphs_height = image.dim.height / line_height_factor;

  line_width += advance;

  // LOG_D(
  //   "Image added to line: w:%d h:%d a:%d",
  //   image.width, image.height,
  //   entry->kind.image_entry.advance
  // );

  line_list.push_front(entry);
}

#define NEXT_LINE_REQUIRED_SPACE (pos.y + (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height())

#if 1
bool
Page::add_word(const char * word,  const Format & fmt)
{
  DisplayList the_list;

  TTF::BitmapGlyph * glyph;
  const char * str = word;
  int16_t height;

  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (font == nullptr) return false;

  height = font->get_line_height();

  if (line_list.empty()) {
    // We are about to start a new line. Check if it will fit on the page.
    if ((screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y)) return false;
  }

  int16_t width = 0;
  bool    first = true;

  while (*str) {
    if ((glyph = font->get_glyph(to_unicode(&str, fmt.text_transform, first))) == nullptr) {
      glyph = font->get_glyph(' ');
    }
    if (glyph) {
      width += glyph->advance;
      first  = false;

      DisplayListEntry * entry = display_list_entry_pool.newElement();
      if (entry == nullptr) no_mem();

      entry->command                = DisplayListCommand::GLYPH;
      entry->kind.glyph_entry.glyph = glyph;
      entry->pos.x = entry->pos.y   = 0;

      the_list.push_front(entry);
    }
  }

  uint16_t avail_width = para_max_x - para_min_x - para_indent;

  if (width >= avail_width) {
    if (strncasecmp(word, "http", 4) == 0) {
      for (auto * entry : the_list) {
        display_list_entry_pool.deleteElement(entry);
      }
      the_list.clear();
      return add_word("[URL removed]", fmt);
    }
    else {
      LOG_E("WORD TOO LARGE!! '%s'", word);
    }
  }

  if ((line_width + width) >= avail_width) {
    add_line(fmt, true);
    screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
    if (screen_is_full) {
      for (auto * entry : the_list) {
        display_list_entry_pool.deleteElement(entry);
      }
      the_list.clear();
      return false;
    }
  }

  line_list.splice_after(line_list.before_begin(), the_list);

  if (glyphs_height < height) glyphs_height = height;
  line_width += width;
  the_list.clear();

  return true;
}
#else
// ToDo: Optimize this method...
bool 
Page::add_word(const char * word,  const Format & fmt)
{
  TTF::BitmapGlyph * glyph;
  const char * str = word;
  int32_t code;

  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (font == nullptr) return false;

  if (line_list.empty()) {
    // We are about to start a new line. Check if it will fit on the page.
    screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
    if (screen_is_full) {
      return false;
    }
  }

  int16_t width = 0;
  bool    first = true;
  while (*str) {
    glyph = font->get_glyph(code = to_unicode(&str, fmt.text_transform, first));
    if (glyph) width += glyph->advance;
    first = false;
  }

  if (width >= (para_max_x - para_min_x - para_indent)) {
    if (strncasecmp(word, "http", 4) == 0) {
      return add_word("[URL removed]", fmt);
    }
    else {
      LOG_E("WORD TOO LARGE!! '%s'", word);
    }
  }

  if ((line_width + width) >= (para_max_x - para_min_x - para_indent)) {
    add_line(fmt, true);
    screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
    if (screen_is_full) {
      return false;
    }
  }

  first = true;
  while (*word) {
    if (font) {
      if ((glyph = font->get_glyph(code = to_unicode(&word, fmt.text_transform, first))) == nullptr) {
        glyph = font->get_glyph(' ');
      }
      if (glyph) {
        add_glyph_to_line(glyph, *font, false);
        //font->show_glyph(*glyph);
      }
    }
    first = false;
  }

  return true;
}
#endif

bool 
Page::add_char(const char * ch, const Format & fmt)
{
  TTF::BitmapGlyph * glyph;

  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (font == nullptr) return false;

  if (screen_is_full) return false;

  if (line_list.empty()) {
    
    // We are about to start a new line. Check if it will fit on the page.

    screen_is_full = ((pos.y + (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height())) > max_y;
    if (screen_is_full) {
      return false;
    }
  }

  int32_t code = to_unicode(&ch, fmt.text_transform, true);

  glyph = font->get_glyph(code);

  if (glyph != nullptr) {
    // Verify that there is enough space for the glyph on the line.
    if ((line_width + (glyph->advance)) >= (para_max_x - para_min_x - para_indent)) {
      add_line(fmt, true); 
      screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
      if (screen_is_full) {
        return fmt.trim;
      }
    }

    add_glyph_to_line(glyph, *font, (code == 32) || (code == 160));
  }

  return true;
}

bool 
Page::add_image(Image & image, const Format & fmt)
{
  Image resized_image;

  if (screen_is_full) {
    return false;
  }

  // Compute the baseline advance for the bitmap, using info from the current font
  TTF::BitmapGlyph * glyph;
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  const char * str = "m";
  int32_t code = to_unicode(&str, fmt.text_transform, true);

  glyph = font->get_glyph(code);

  // Compute available space to put the image.

  int32_t w = 0;
  int32_t h = 0;
  int32_t advance;
  int16_t gap = 0;

  if (glyph != nullptr) {
    gap = glyph->advance - glyph->dim.width;
  }
  
  // compute target w, h and advance for the image

  int16_t target_width  = para_max_x - para_min_x;
  int16_t target_height = max_y - min_y;

  if (fmt.width || fmt.height) {
    if (fmt.width  && (fmt.width  <  target_width)) target_width  = fmt.width;
    if (fmt.height && (fmt.height < target_height)) target_height = fmt.height;

    w = target_width;
    h = image.dim.height * w / image.dim.width;
    if (h > target_height) {
      h = target_height;
      w = image.dim.width * h / image.dim.height;
    }
  }
  else {
    if ((image.dim.width < target_width) && (image.dim.height < target_height)) {
      w = image.dim.width;
      h = image.dim.height;
    } 
    else {
      w = target_width;
      h = image.dim.height * w / image.dim.width;
      if (h > target_height) {
        h = target_height;
        w = image.dim.width * h / image.dim.height;
      }
    }
  }
  advance = w + gap;

  // Verify that there is enough room for the bitmap on the line

  if ((line_width + advance) >= (para_max_x - para_min_x - para_indent)) {
    add_line(fmt, true); 

    // int16_t the_height = (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height();
    // if (the_height < h) the_height = h;
  }
  
  if ((screen_is_full = ((pos.y + h) > max_y))) return false;

  if ((w != image.dim.width) || (h != image.dim.height)) {

    unsigned char * resized_bitmap = nullptr;

    if (compute_mode == ComputeMode::DISPLAY) {
      if ((image.dim.width > 2) || (image.dim.height > 2)) {

        int32_t size = w * h;

        if (size == 0) return false;

        if ((resized_bitmap = new unsigned char[w * h]) == nullptr) {
          msg_viewer.out_of_memory("resized image allocation");
        }

        stbir_resize_uint8(image.bitmap,   image.dim.width, image.dim.height, 0,
                           resized_bitmap, w,               h,                0,
                           1);
      }
    }

    resized_image.bitmap = resized_bitmap;
    resized_image.dim    = Dim(w, h);

    add_image_to_line(resized_image, advance, false, fmt.line_height_factor);
  }
  else {
    add_image_to_line(image, advance, true, fmt.line_height_factor);
  }

  return true;
}

void
Page::add_text(std::string str, const Format & fmt)
{
  Format myfmt = fmt;

  char * buff = new char[100];
  if (buff == nullptr) msg_viewer.out_of_memory("temp buffer allocation");
  const char *s = str.c_str();
  while (*s) {
    if (uint8_t(*s) <= ' ') {
      if (*s == ' ') {
        myfmt.trim = *s == ' ';
        if (!page.add_char(s, myfmt)) break;
      }
      s++;
    }
    else {
      int16_t count = 0;
      while (uint8_t(*s) > ' ') { buff[count++] = *s++; }
      buff[count] = 0;
      if (!page.add_word(buff, myfmt)) break;
    }
  }

  delete [] buff;
}

void 
Page::put_image(Image & image, 
                Pos     pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  if (compute_mode == ComputeMode::DISPLAY) {
    int32_t size = image.dim.width * image.dim.height;
    if ((entry->kind.image_entry.image.bitmap = new unsigned char [size]) == nullptr) {
      msg_viewer.out_of_memory("image allocation");
    }
    memcpy((void *)entry->kind.image_entry.image.bitmap, image.bitmap, size);
  }
  else {
    entry->kind.image_entry.image.bitmap = nullptr;
  }

  entry->command                     = DisplayListCommand::IMAGE;
  entry->kind.image_entry.image.dim  = image.dim;
  entry->pos                         = pos;

  #if DEBUGGING
    if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      LOG_E("draw_bitmap with a negative location: %d %d", entry->pos.x, entry->pos.y);
    }
    else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
      LOG_E("draw_bitmap with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  display_list.push_front(entry);
}

void 
Page::put_highlight(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::HIGHLIGHT;
  entry->kind.region_entry.dim = dim;
  entry->pos                   = pos;

  #if DEBUGGING
    if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      LOG_E("put_highlight with a negative location: %d %d", entry->pos.x, entry->pos.y);
    }
    else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
      LOG_E("put_highlight with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  display_list.push_front(entry);
}

void 
Page::clear_highlight(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::CLEAR_HIGHLIGHT;
  entry->kind.region_entry.dim = dim;
  entry->pos                   = pos;

  #if DEBUGGING
    if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    }
    else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  display_list.push_front(entry);
}

void 
Page::clear_region(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::CLEAR_REGION;
  entry->kind.region_entry.dim = dim;
  entry->pos                   = pos;

  #if DEBUGGING
    if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    }
    else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  display_list.push_front(entry);
}


void 
Page::set_region(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::SET_REGION;
  entry->kind.region_entry.dim = dim;
  entry->pos                   = pos;

  #if DEBUGGING
    if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
      LOG_E("Put_str_at with a negative location: %d %d", entry->pos.x, entry->pos.y);
    }
    else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
      LOG_E("Put_str_at with a too large location: %d %d", entry->pos.x, entry->pos.y);
    }
  #endif

  display_list.push_front(entry);
}

bool
Page::show_cover(unsigned char * data, int32_t size)
{
  if (compute_mode == ComputeMode::DISPLAY) {
    int32_t image_width, image_height, channel_count;

    unsigned char * bitmap_data = stbi_load_from_memory(data, size, &image_width, &image_height, &channel_count, 1);

    if (bitmap_data != nullptr) { 
      // LOG_D("Image: width: %d height: %d channel_count: %d", image_width, image_height, channel_count);

      Dim dim;
      Pos pos;

      dim.width  = Screen::WIDTH;
      dim.height = image_height * Screen::WIDTH / image_width;

      if (dim.height > Screen::HEIGHT) {
        dim.height = Screen::HEIGHT;
        dim.width  = image_width * Screen::HEIGHT / image_height;
      }

      pos.x = (Screen::WIDTH  - dim.width ) >> 1;
      pos.y = (Screen::HEIGHT - dim.height) >> 1;

      // LOG_D("Image Parameters: %d %d %d %d", dim.width, dim.height, pos.x, pos.y);

      unsigned char * resized_bitmap = new unsigned char[dim.width * dim.height];
      if (resized_bitmap == nullptr) {
        stbi_image_free(bitmap_data);
        LOG_D("Unable to load cover file");
        return false;
      }

      stbir_resize_uint8(bitmap_data,   image_width, image_height, 0,
                        resized_bitmap, dim.width,   dim.height,   0,
                        1);
      screen.clear();
      screen.draw_bitmap(resized_bitmap, dim, pos);
      screen.update();

      delete [] resized_bitmap;
    }
    else {
      LOG_D("Unable to load cover file");
      return false;
    }

    stbi_image_free(bitmap_data);
    return true;
  }

  return false;
}

void
Page::show_display_list(const DisplayList & list, const char * title) const
{
  #if DEBUGGING
    std::cout << title << std::endl;
    for (auto * entry : list) {
      if (entry->command == DisplayListCommand::GLYPH) {
        std::cout << "GLYPH" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.glyph_entry.glyph->dim.width  <<
          " h:" << entry->kind.glyph_entry.glyph->dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::IMAGE) {
        std::cout << "IMAGE" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.image_entry.image.dim.width  <<
          " h:" << entry->kind.image_entry.image.dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::HIGHLIGHT) {
        std::cout << "HIGHLIGHT" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.region_entry.dim.width  <<
          " h:" << entry->kind.region_entry.dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::CLEAR_HIGHLIGHT) {
        std::cout << "CLEAR_HIGHLIGHT" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.region_entry.dim.width  <<
          " h:" << entry->kind.region_entry.dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::CLEAR_REGION) {
        std::cout << "CLEAR_REGION" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.region_entry.dim.width  <<
          " h:" << entry->kind.region_entry.dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::SET_REGION) {
        std::cout << "SET_REGION" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.region_entry.dim.width  <<
          " h:" << entry->kind.region_entry.dim.height << std::endl;
      }
    }
  #endif
}