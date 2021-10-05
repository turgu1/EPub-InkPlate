// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PAGE__ 1
#include "models/epub.hpp"

#include "viewers/page.hpp"
#include "viewers/msg_viewer.hpp"
#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <sstream>

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

void 
Page::clean()
{
  clear_display_list();
  clear_line_list();
  para_indent = 0;
  top_margin  = 0;
}

void
Page::clear_display_list()
{
  for (auto * entry: display_list) {
    if (entry->command == DisplayListCommand::IMAGE) {
      if (entry->kind.image_entry.image.bitmap) {
        free(entry->kind.image_entry.image.bitmap);
      }
    }
    display_list_entry_pool.deleteElement(entry);
  }
  display_list.clear();
}

// 00000000 -- 0000007F: 	0xxxxxxx
// 00000080 -- 000007FF: 	110xxxxx 10xxxxxx
// 00000800 -- 0000FFFF: 	1110xxxx 10xxxxxx 10xxxxxx
// 00010000 -- 001FFFFF: 	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

int32_t 
Page::to_unicode(const char **str, CSS::TextTransform transform, bool first) const
{
  const uint8_t * c    = (uint8_t *) *str;
  int32_t         u    = 0;
  bool            done = false;

  if (*c == '&') {
    const uint8_t * s = ++c;
    uint8_t len = 0;
    while ((len < 7) && (*s != 0) && (*s != ';')) { s++; len++; }
    if (*s == ';') {
      if      (strncmp("nbsp;",  (const char *) c, 5) == 0) u =    160;
      else if (strncmp("lt;",    (const char *) c, 3) == 0) u =     60;
      else if (strncmp("gt;",    (const char *) c, 3) == 0) u =     62;
      else if (strncmp("amp;",   (const char *) c, 4) == 0) u =     38;
      else if (strncmp("quot;",  (const char *) c, 5) == 0) u =     34;
      else if (strncmp("apos;",  (const char *) c, 5) == 0) u =     39;
      else if (strncmp("mdash;", (const char *) c, 6) == 0) u = 0x2014;
      else if (strncmp("ndash;", (const char *) c, 6) == 0) u = 0x2013;
      else if (strncmp("lsquo;", (const char *) c, 6) == 0) u = 0x2018;
      else if (strncmp("rsquo;", (const char *) c, 6) == 0) u = 0x2019;
      else if (strncmp("ldquo;", (const char *) c, 6) == 0) u = 0x201C;
      else if (strncmp("rdquo;", (const char *) c, 6) == 0) u = 0x201D;
      else if (strncmp("euro;",  (const char *) c, 5) == 0) u = 0x20AC;
      else if (strncmp("dagger;",(const char *) c, 7) == 0) u = 0x2020;
      else if (strncmp("Dagger;",(const char *) c, 7) == 0) u = 0x2021;
      else if (strncmp("copy;",  (const char *) c, 5) == 0) u =   0xa9;
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
  Font::Glyph * glyph;
  
  Font * font = fonts.get(fmt.font_index);

  const char * s = str.c_str(); 
  if (fmt.align == CSS::Align::LEFT) {
    bool first = true;
    while (*s) {
      
      glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first), fmt.font_size);
      if (glyph != nullptr) {
        DisplayListEntry * entry = display_list_entry_pool.newElement();
        if (entry == nullptr) no_mem();
        entry->command                   = DisplayListCommand::GLYPH;
        entry->kind.glyph_entry.glyph    = glyph;
        entry->pos.x                     = pos.x + glyph->xoff;
        entry->pos.y                     = pos.y + glyph->yoff;
        entry->kind.glyph_entry.is_space = false; // for completeness but not supposed to be used...

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
      glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first), fmt.font_size);
      if (glyph != nullptr) size += glyph->advance;
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
      glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first), fmt.font_size);
      if (glyph != nullptr) {
        
        DisplayListEntry * entry = display_list_entry_pool.newElement();
        if (entry == nullptr) no_mem();

        entry->command                   = DisplayListCommand::GLYPH;
        entry->kind.glyph_entry.glyph    = glyph;
        entry->pos.x                     = x + glyph->xoff;
        entry->pos.y                     = pos.y + glyph->yoff;
        entry->kind.glyph_entry.is_space = false;

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
  Font::Glyph * glyph;
  
  Font * font = fonts.get(fmt.font_index);

  glyph = font->get_glyph(ch, fmt.font_size);
  if (glyph != nullptr) {
    DisplayListEntry * entry = display_list_entry_pool.newElement();
    if (entry == nullptr) no_mem();
    entry->command                   = DisplayListCommand::GLYPH;
    entry->kind.glyph_entry.glyph    = glyph;
    entry->pos.x                     = pos.x + glyph->xoff;
    entry->pos.y                     = pos.y + glyph->yoff;
    entry->kind.glyph_entry.is_space = false;

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
    else if (entry->command == DisplayListCommand::ROUNDED) {
      screen.draw_round_rectangle(
        entry->kind.region_entry.dim, 
        entry->pos, 
        Screen::BLACK_COLOR);
    }
    else if (entry->command == DisplayListCommand::CLEAR_ROUNDED) {
      screen.draw_round_rectangle(
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

  #if 0 && DEBUGGING
    Dim dim;
    Pos pos;

    pos.x = 0;
    pos.y = 0;
    dim.width  = 50;
    dim.height = 0;

    for (int i = 0; i < Screen::HEIGHT; i += 50) {
      pos.y += 50;
      screen.draw_rectangle(dim, pos, Screen::BLACK_COLOR);
    }
  #endif

  screen.update(no_full);
}

void
Page::start(const Format & fmt)
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
Page::set_limits(const Format & fmt)
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
  Font * font = fonts.get(fmt.font_index);
  
  // LOG_D("line_break font index: %d", fmt.font_index);

  if (!line_list.empty()) {
    add_line(fmt, false);
  }
  else {
    pos.y += font->get_line_height(fmt.font_size) * fmt.line_height_factor;
    pos.x  = min_x + indent_next_line;
  }

  screen_is_full = screen_is_full || ((pos.y - font->get_descender_height(fmt.font_size)) >= max_y);
  if (screen_is_full) {
    return false;
  }
  return true;
}

bool 
Page::new_paragraph(const Format & fmt, bool recover)
{
  Font * font = fonts.get(fmt.font_index);

  // Check if there is enough room for the first line of the paragraph.
  if (!recover) {
    screen_is_full = screen_is_full || 
                    ((pos.y + fmt.margin_top + 
                             (fmt.line_height_factor * font->get_line_height(fmt.font_size)) - 
                              font->get_descender_height(fmt.font_size)) > max_y);
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

void
Page::break_paragraph(const Format & fmt)
{
  if (!line_list.empty()) {
    add_line(fmt, true);
  }
}

bool 
Page::end_paragraph(const Format & fmt)
{
  Font * font = fonts.get(fmt.font_index);

  if (!line_list.empty()) {
    add_line(fmt, false);

    int32_t descender = font->get_descender_height(fmt.font_size);

    pos.y += fmt.margin_bottom;// - descender;

    if ((pos.y - descender) >= max_y) {
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
  int16_t line_height = glyphs_height * line_height_factor;
  pos.y += top_margin + line_height; // (line_height >> 1) + (glyphs_height >> 1);

  // #if DEBUGGING_AID
  //   if (show_location) {
  //     std::cout << "New Line: ";
  //     show_controls(" ");
  //     show_fmt(fmt, "   ->  ");  
  //   }
  // #endif

  // Get rid of space characters that are at the end of the line.
  // This is mainly required for the JUSTIFY alignment algo.

  while (!line_list.empty()) {
    DisplayListEntry * entry = *(line_list.begin());
    if ((entry->command == DisplayListCommand::GLYPH) && (entry->kind.glyph_entry.is_space)) {
      display_list_entry_pool.deleteElement(entry);
      line_list.pop_front(); 
    }
    else break;
    // if (entry->pos.y > 0) {
    //   if (entry->command == DisplayListCommand::IMAGE) {
    //     if (entry->kind.image_entry.image.bitmap != nullptr) {
    //       delete [] entry->kind.image_entry.image.bitmap;
    //     }
    //   }
    // }
    // else break;
  }

  line_list.reverse();
  
  if (!line_list.empty() && (compute_mode == ComputeMode::DISPLAY)) {
  
    if ((fmt.align == CSS::Align::JUSTIFY) && justifyable) {
      int16_t target_width = (para_max_x - para_min_x - para_indent);
      int16_t loop_count = 0;
      while ((line_width < target_width) && (++loop_count < 50)) {
        bool at_least_once = false;
        for (auto * entry : line_list) {
          if (entry->pos.x > 0) {  // This means it's a white space
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
      int16_t x     = entry->pos.x; // x may contains the calculated gap between words
      entry->pos.x  = pos.x + entry->kind.glyph_entry.glyph->xoff;
      entry->pos.y += pos.y + entry->kind.glyph_entry.glyph->yoff;
      pos.x += (x == 0) ? entry->kind.glyph_entry.glyph->advance : x;
    }
    else if (entry->command == DisplayListCommand::IMAGE) {
      if (fmt.align == CSS::Align::CENTER) {
        entry->pos.x = para_min_x + ((para_max_x - para_min_x) >> 1) - (line_width >> 1);
      }
      else {
        entry->pos.x = pos.x;
      }
      entry->pos.y = pos.y - entry->kind.image_entry.image.dim.height;
      pos.x += entry->kind.image_entry.advance;
    }
    else {
      LOG_E("Wrong entry type for add_line: %d", (int)entry->command);
    }

    #if DEBUGGING
      if ((entry->pos.x < 0) || (entry->pos.y < 0)) {
        LOG_E("add_line entry with a negative location: %d %d %d", entry->pos.x, entry->pos.y, (int)entry->command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
      else if ((entry->pos.x >= Screen::WIDTH) || (entry->pos.y >= Screen::HEIGHT)) {
        LOG_E("add_line with a too large location: %d %d %d", entry->pos.x, entry->pos.y, (int)entry->command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
    #endif

    display_list.push_front(entry);
  };
  
  line_width = line_height = glyphs_height = 0;
  line_height_factor = 0.0;
  clear_line_list();

  para_indent = 0;
  top_margin  = 0;

  // std::cout << "End New Line:"; show_controls();
}

inline void 
Page::add_glyph_to_line(Font::Glyph * glyph, const Format & fmt, Font & font, bool is_space)
{
  if (is_space && (line_width == 0)) return;

  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command                   = DisplayListCommand::GLYPH;
  entry->kind.glyph_entry.glyph    = glyph;
  entry->pos.x                     = is_space ? glyph->advance : 0;
  entry->pos.y                     = 0;
  entry->kind.glyph_entry.is_space = is_space;
  
  if (glyphs_height < glyph->line_height) glyphs_height = glyph->line_height;
  if (line_height_factor < fmt.line_height_factor) line_height_factor = fmt.line_height_factor;

  line_width += (glyph->advance);

  line_list.push_front(entry);
}

void 
Page::add_image_to_line(Image & image, int16_t advance, const Format & fmt)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command                  = DisplayListCommand::IMAGE;
  entry->kind.image_entry.advance = advance;

  image.retrieve_image_data(entry->kind.image_entry.image);
  
  // if (compute_mode == ComputeMode::DISPLAY) {
  //   if (copy) {
  //     int32_t size = image.dim.width * image.dim.height;
  //     if (image.bitmap != nullptr) {
  //       if ((entry->kind.image_entry.image.bitmap = new unsigned char[size]) == nullptr) {
  //         msg_viewer.out_of_memory("image bitmap allocation");
  //       }
  //       memcpy((void *) entry->kind.image_entry.image.bitmap, image.bitmap, size);
  //     }
  //     else {
  //       entry->kind.image_entry.image.bitmap = nullptr;
  //     }
  //   }
  //   else {
  //     entry->kind.image_entry.image.bitmap = image.bitmap;
  //   }
  // }
  // else {
  //   entry->kind.image_entry.image.bitmap = nullptr;
  // }
  
  entry->pos.x = entry->pos.y = 0;
  
  Dim dim = entry->kind.image_entry.image.dim;

  if (line_height_factor < fmt.line_height_factor) line_height_factor = fmt.line_height_factor;
  if (glyphs_height < dim.height) glyphs_height = dim.height / line_height_factor;

  line_width += advance;

  // LOG_D(
  //   "Image added to line: w:%d h:%d a:%d",
  //   image.width, image.height,
  //   entry->kind.image_entry.advance
  // );

  line_list.push_front(entry);
}

#define NEXT_LINE_REQUIRED_SPACE (pos.y + (fmt.line_height_factor * font->get_line_height(fmt.font_size)) - font->get_descender_height(fmt.font_size))

#if 1
bool
#if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
  Page::add_word(const char * word,  const Format & fmt, bool debugging)
#else
  Page::add_word(const char * word,  const Format & fmt)
#endif
{
  Font * font = fonts.get(fmt.font_index);
  if (font == nullptr) return false;

  if (line_list.empty()) {
    // We are about to start a new line. Check if it will fit on the page.
    if ((screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y)) return false;
  }

  Font::Glyph * glyph;

  DisplayList      * the_list = new DisplayList;
  const char       * str      = word;
  int16_t            height   = font->get_line_height(fmt.font_size);
  int16_t            width    = 0;
  bool               first    = true;
 
  while (*str) {
    glyph = font->get_glyph(to_unicode(&str, fmt.text_transform, first), fmt.font_size);
    if (glyph == nullptr) {
      glyph = font->get_glyph(' ', fmt.font_size);
    }

    if (glyph) {
      width += glyph->advance;
      first  = false;

      DisplayListEntry * entry = display_list_entry_pool.newElement();
      if (entry == nullptr) no_mem();

      entry->command                   = DisplayListCommand::GLYPH;
      entry->kind.glyph_entry.glyph    = glyph;
      entry->pos.x                     = 0;
      entry->kind.glyph_entry.is_space = false;
      entry->pos.y                     = fmt.vertical_align;

      the_list->push_front(entry);
    }
  }

  uint16_t avail_width = para_max_x - para_min_x - para_indent;

  if (width >= avail_width) {
    if (strncasecmp(word, "http", 4) == 0) {
      for (auto * entry : *the_list) {
        display_list_entry_pool.deleteElement(entry);
      }
      the_list->clear();
      delete the_list;
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
      for (auto * entry : *the_list) {
        display_list_entry_pool.deleteElement(entry);
      }
      the_list->clear();
      delete the_list;
      return false;
    }
  }

  line_list.splice_after(line_list.before_begin(), *the_list);

  if (glyphs_height < height) glyphs_height = height;
  if (line_height_factor < fmt.line_height_factor) line_height_factor = fmt.line_height_factor;
  line_width += width;

  for (auto * entry : *the_list) {
    display_list_entry_pool.deleteElement(entry);
  }
  the_list->clear();
  delete the_list;

  return true;
}
#else
// ToDo: Optimize this method...
bool 
Page::add_word(const char * word,  const Format & fmt)
{
  Font::Glyph * glyph;
  const char * str = word;
  int32_t code;

  Font * font = fonts.get(fmt.font_index, fmt.font_size);

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
    glyph = font->get_glyph(code = to_unicode(&str, fmt.text_transform, first), fmt.font_size);
    if (glyph != nullptr) width += glyph->advance;
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
      glyph = font->get_glyph(to_unicode(&word, fmt.text_transform, first), fmt.font_size);
      if (glyph == nullptr) {
        glyph = font->get_glyph(' ', fmt.font_size);
      }
      if (glyph != nullptr) {
        add_glyph_to_line(glyph, fmt.font_size, *font, false);
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
  Font::Glyph * glyph;

  Font * font = fonts.get(fmt.font_index);

  if (font == nullptr) return false;

  if (screen_is_full) return false;

  if (line_list.empty()) {
    
    // We are about to start a new line. Check if it will fit on the page.

    screen_is_full = ((pos.y + (fmt.line_height_factor * font->get_line_height(fmt.font_size)) - font->get_descender_height(fmt.font_size))) > max_y;
    if (screen_is_full) {
      return false;
    }
  }

  int32_t code = to_unicode(&ch, fmt.text_transform, true);

  glyph = font->get_glyph(code, fmt.font_size);

  if (glyph != nullptr) {
    // Verify that there is enough space for the glyph on the line.
    if ((line_width + (glyph->advance)) >= (para_max_x - para_min_x - para_indent)) {
      add_line(fmt, true); 
      screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
      if (screen_is_full) {
        return fmt.trim;
      }
    }

    add_glyph_to_line(glyph, fmt, *font, (code == 32) || (code == 160));
  }

  return true;
}

bool 
Page::add_image(Image & image, const Format & fmt /*, bool at_start_of_page*/)
{
  if (screen_is_full) {
    return false;
  }

  // Compute the baseline advance for the bitmap, using info from the current font
  Font::Glyph * glyph;
  Font * font = fonts.get(fmt.font_index);

  const char * str = "m";
  int32_t code = to_unicode(&str, fmt.text_transform, true);

  glyph = font->get_glyph(code, fmt.font_size);

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
    h = image.get_dim().height * w / image.get_dim().width;
    if (h > target_height) {
      h = target_height;
      w = image.get_dim().width * h / image.get_dim().height;
    }
  }
  else {
    if ((image.get_dim().width < target_width) && (image.get_dim().height < target_height)) {
      w = image.get_dim().width;
      h = image.get_dim().height;
    } 
    else {
      w = target_width;
      h = image.get_dim().height * w / image.get_dim().width;
      if (h > target_height) {
        h = target_height;
        w = image.get_dim().width * h / image.get_dim().height;
      }
    }
  }
  advance = w + gap;

  // Verify that there is enough room for the bitmap on the line

  if ((line_width + advance) >= (para_max_x - para_min_x - para_indent)) {
//    if (!(line_list.empty() && at_start_of_page)) {
      add_line(fmt, true);
    // }
    // else {
    //   para_indent = 0;
    //   top_margin = 0;
    // }

    // int16_t the_height = (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height();
    // if (the_height < h) the_height = h;
  }
  
  if ((screen_is_full = ((pos.y + h) > max_y))) return false;

  if ((w != image.get_dim().width) || (h != image.get_dim().height)) {

    // unsigned char * resized_bitmap = nullptr;

    if (compute_mode == ComputeMode::DISPLAY) {
      if ((image.get_dim().width > 2) || (image.get_dim().height > 2)) {

        if ((w == 0) || (h == 0)) return false;

        image.resize(Dim(w, h));
      }
    }

    add_image_to_line(image, advance, fmt);
  }
  else {
    add_image_to_line(image, advance, fmt);
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
      if (!add_word(buff, myfmt)) break;
    }
  }

  delete [] buff;
}

void 
Page::put_image(Image::ImageData & image, 
                Pos                  pos)
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
Page::put_rounded(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::ROUNDED;
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
Page::clear_rounded(Dim dim, Pos pos)
{
  DisplayListEntry * entry = display_list_entry_pool.newElement();
  if (entry == nullptr) no_mem();

  entry->command               = DisplayListCommand::CLEAR_ROUNDED;
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
Page::show_cover(Image & img)
{
  if (compute_mode == ComputeMode::DISPLAY) {
    int32_t image_width  = img.get_dim().width;
    int32_t image_height = img.get_dim().height;

    if (img.get_bitmap() != nullptr) { 
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

      img.resize(dim);

      screen.clear();
      screen.draw_bitmap(img.get_bitmap(), img.get_dim(), pos);
      screen.update();
    }
    else {
      LOG_D("Unable to load cover file");
      return false;
    }

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
      else if (entry->command == DisplayListCommand::ROUNDED) {
        std::cout << "ROUNDED" <<
          " x:" << entry->pos.x <<
          " y:" << entry->pos.y <<
          " w:" << entry->kind.region_entry.dim.width  <<
          " h:" << entry->kind.region_entry.dim.height << std::endl;
      }
      else if (entry->command == DisplayListCommand::CLEAR_ROUNDED) {
        std::cout << "CLEAR_ROUNDED" <<
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

int16_t
Page::get_pixel_value(const CSS::Value & value, const Format & fmt, int16_t ref, bool vertical)
{
  switch (value.value_type) {
    case CSS::ValueType::PX:
      return value.num;
    case CSS::ValueType::PT:
      return (value.num * Screen::RESOLUTION) / 72;
    case CSS::ValueType::EM:
    case CSS::ValueType::REM:
        return value.num * 
               ((fmt.font_size * Screen::RESOLUTION) / 72);
    case CSS::ValueType::PERCENT:
      return (value.num * ref) / 100;
    case CSS::ValueType::NO_TYPE:
      return ref * value.num;
    case CSS::ValueType::CM:
      return (value.num * Screen::RESOLUTION) / 2.54;
    case CSS::ValueType::VH:
      return (value.num * (fmt.screen_bottom - fmt.screen_top)) / 100;
    case CSS::ValueType::VW:
      return (value.num * (fmt.screen_right - fmt.screen_left)) / 100;;
    case CSS::ValueType::STR:
      // LOG_D("get_pixel_value(): Str value: %s", value.str.c_str());
      return 0;
    default:
      // LOG_D("get_pixel_value: Wrong data type!: %d", value.value_type);
      return value.num;
  }
  return 0;
}

int16_t
Page::get_point_value(const CSS::Value & value, const Format & fmt, int16_t ref)
{
  int8_t normal_size = epub.get_book_format_params()->font_size;

  switch (value.value_type) {
    case CSS::ValueType::PX:
      //  pixels -> Screen HResolution per inch
      //    x    ->    72 pixels per inch
      return (value.num * 72) / Screen::RESOLUTION; // convert pixels in points
    case CSS::ValueType::PT:
      return value.num;  // Already in points
    case CSS::ValueType::EM:
    case CSS::ValueType::REM:
            return value.num * ref;
    case CSS::ValueType::CM:
      return (value.num * 72) / 2.54;
    case CSS::ValueType::PERCENT:
      return (value.num * normal_size) / 100;
    case CSS::ValueType::NO_TYPE:
      return normal_size * value.num;
    case CSS::ValueType::STR:
      LOG_D("get_point_value(): Str value: %s.", value.str.c_str());
      return 0;
      break;
    case CSS::ValueType::VH:
      return ((value.num * (fmt.screen_bottom - fmt.screen_top)) / 100) * 72 / Screen::RESOLUTION;
    case CSS::ValueType::VW:
      return ((value.num * (fmt.screen_right - fmt.screen_left)) / 100) * 72 / Screen::RESOLUTION;
    case CSS::ValueType::INHERIT:
      return ref;
    default:
      LOG_E("get_point_value(): Wrong data type!");
      return value.num;
  }
  return 0;
}

float
Page::get_factor_value(const CSS::Value & value, const Format & fmt, float ref)
{
  switch (value.value_type) {
    case CSS::ValueType::PX:
    case CSS::ValueType::PT:
    case CSS::ValueType::STR:
      return 1.0;  
    case CSS::ValueType::EM:
    case CSS::ValueType::REM:
    case CSS::ValueType::NO_TYPE:
    case CSS::ValueType::DIMENSION:
      return value.num;
    case CSS::ValueType::PERCENT:
      return (value.num * ref) / 100.0;
    case CSS::ValueType::INHERIT:
      return ref;
    default:
      // LOG_E("get_factor_value: Wrong data type!");
      return 1.0;
  }
  return 0;
}

void
Page::adjust_format(DOM::Node * dom_current_node, 
                    Format &    fmt,
                    CSS *       element_css,
                    CSS *       item_css)
{
  CSS::RulesMap rules;

  // item_css->show();
  // std::cout << "======" << std::endl;
  // if (element_css != nullptr) element_css->show();
  // std::cout << "------" << std::endl;
  // dom_current_node->show(1);

  if (item_css != nullptr) {
    item_css->match(dom_current_node, rules);
    if (!rules.empty()) {
      // item_css->show(rules);
      adjust_format_from_rules(fmt, rules);
    }
    else {
      #if DEBUGGING1
        LOG_D("No match");
        dom_current_node->show(1);
      #endif
    }
  }
  if (element_css != nullptr) {
    if (!element_css->rules_map.empty()) adjust_format_from_rules(fmt, element_css->rules_map);
  }
}

void
Page::adjust_format_from_rules(Format & fmt, const CSS::RulesMap & rules)
{  
  const CSS::Values * vals;

  // LOG_D("Found!");

  Fonts::FaceStyle font_weight = ((fmt.font_style == Fonts::FaceStyle::BOLD) || 
                                  (fmt.font_style == Fonts::FaceStyle::BOLD_ITALIC)) ? 
                                     Fonts::FaceStyle::BOLD : 
                                     Fonts::FaceStyle::NORMAL;
  Fonts::FaceStyle font_style = ((fmt.font_style == Fonts::FaceStyle::ITALIC) || 
                                 (fmt.font_style == Fonts::FaceStyle::BOLD_ITALIC)) ? 
                                     Fonts::FaceStyle::ITALIC : 
                                     Fonts::FaceStyle::NORMAL;
  
  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::FONT_STYLE))) {
    font_style = (Fonts::FaceStyle) vals->front()->choice.face_style;
  }
  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::FONT_WEIGHT))) {
    font_weight = (Fonts::FaceStyle) vals->front()->choice.face_style;
  }
  Fonts::FaceStyle new_style = fonts.adjust_font_style(fmt.font_style, font_style, font_weight);

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::FONT_FAMILY))) {
    int16_t idx = -1;
    for (auto & font_name : *vals) {
      if ((idx = fonts.get_index(font_name->str, new_style)) != -1) break;
    }
    if (idx == -1) {
      LOG_D("Font not found 1: %s %d", vals->front()->str.c_str(), (int)new_style);
      idx = fonts.get_index("Default", new_style);
    }
    if (idx == -1) {
      fmt.font_style = Fonts::FaceStyle::NORMAL;
      fmt.font_index = 1;
    }
    else {
      // LOG_D("Font index: %d", idx);
      fmt.font_index = idx;
      fmt.font_style = new_style;
    }
  }
  else if (new_style != fmt.font_style) {
    reset_font_index(fmt, new_style);
  }

  fonts.check(fmt.font_index, fmt.font_style);

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::TEXT_ALIGN))) {
    fmt.align = (CSS::Align) vals->front()->choice.align;
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::TEXT_INDENT))) {
    fmt.indent = get_pixel_value(*(vals->front()), fmt, paint_width());
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::FONT_SIZE))) {
    fmt.font_size = get_point_value(*(vals->front()), fmt, fmt.font_size);
    if (fmt.font_size == 0) {
      LOG_E("adjust_format_from_suite: setting fmt.font_size to 0!!!");
    }
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::LINE_HEIGHT))) {
    fmt.line_height_factor = get_factor_value(*(vals->front()), fmt, fmt.line_height_factor);
  }

  int16_t width_ref  = Screen::WIDTH  - fmt.screen_left - fmt.screen_right;
  int16_t height_ref = Screen::HEIGHT - fmt.screen_top  - fmt.screen_bottom;

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::MARGIN))) {

    int16_t size = 0;
    for (auto val __attribute__ ((unused)) : *vals) size++;
    CSS::Values::const_iterator it = vals->begin();

    if (size == 1) {
      fmt.margin_top   = fmt.margin_bottom = get_pixel_value(*(vals->front()), fmt, height_ref, true);
      fmt.margin_right = fmt.margin_left   = get_pixel_value(*(vals->front()), fmt, width_ref       );
    }
    else if (size == 2) {
      fmt.margin_top   = fmt.margin_bottom = get_pixel_value(**it++,           fmt, height_ref, true);
      fmt.margin_right = fmt.margin_left   = get_pixel_value(**it,             fmt, width_ref       );
    }
    else if (size == 3) {
      fmt.margin_top                       = get_pixel_value(**it++,           fmt, height_ref, true);
      fmt.margin_right = fmt.margin_left   = get_pixel_value(**it++,           fmt, width_ref       );
      fmt.margin_bottom                    = get_pixel_value(**it,             fmt, height_ref, true);
    }
    else if (size == 4) {
      fmt.margin_top                       = get_pixel_value(**it++,           fmt, height_ref, true);
      fmt.margin_right                     = get_pixel_value(**it++,           fmt, width_ref       );
      fmt.margin_bottom                    = get_pixel_value(**it++,           fmt, height_ref, true);
      fmt.margin_left                      = get_pixel_value(**it,             fmt, width_ref       );
    }
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::DISPLAY))) {
    fmt.display = vals->front()->choice.display;
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::MARGIN_LEFT))) {
    fmt.margin_left = get_pixel_value(*(vals->front()), fmt, width_ref);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::MARGIN_RIGHT))) {
    fmt.margin_right = get_pixel_value(*(vals->front()), fmt, width_ref);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::MARGIN_TOP))) {
    fmt.margin_top = get_pixel_value(*(vals->front()), fmt, height_ref, true);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::MARGIN_BOTTOM))) {
    fmt.margin_bottom = get_pixel_value(*(vals->front()), fmt, height_ref, true);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::WIDTH))) {
    fmt.width = get_pixel_value(*(vals->front()), fmt, fmt.width /*Screen::WIDTH - fmt.screen_left - fmt.screen_right */);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::HEIGHT))) {
    fmt.height = get_pixel_value(*(vals->front()), fmt, Screen::HEIGHT - fmt.screen_top - fmt.screen_bottom);
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::TEXT_TRANSFORM))) {
    fmt.text_transform = vals->front()->choice.text_transform;
  }

  if ((vals = CSS::get_values_from_rules(rules, CSS::PropertyId::VERTICAL_ALIGN))) {
    if (vals->front()->choice.vertical_align == CSS::VerticalAlign::NORMAL) {
      fmt.vertical_align = 0;
    }
    else if (vals->front()->choice.vertical_align == CSS::VerticalAlign::SUB) {
      fmt.vertical_align = 5;
    }
    else if (vals->front()->choice.vertical_align == CSS::VerticalAlign::SUPER) {
      fmt.vertical_align = -5;
    }
    else if (vals->front()->choice.vertical_align == CSS::VerticalAlign::VALUE) {
      Font * font = fonts.get(fmt.font_index);

      fmt.vertical_align = - get_pixel_value(*(vals->front()), fmt, font->get_line_height(fmt.font_size));
    }
  }
}
