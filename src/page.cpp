#define __PAGE__ 1
#include "page.hpp"

#include "screen.hpp"

//--- Image load to bitmap tool ---
//
// Keep only PNG, JPEG, BMP, GIF
// No load from a file (will come from the epub file as a char array)

#define STBI_ONLY_JPEG
#define STBI_SUPPORT_ZLIB
#define STBI_ONLY_PNG
// #define STBI_ONLY_BMP
// #define STBI_ONLY_GIF

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_resize.h"

#include <string>
#include <algorithm>

static const char * TAG = "Page";

Page::Page() :
  compute_mode(DISPLAY), 
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
  for (auto & entry: display_list) {
    if (entry.command == IMAGE) {
      if (entry.kind.image_entry.image.bitmap) delete [] entry.kind.image_entry.image.bitmap;
    }
  }
  display_list.clear();
}

void
Page::clear_line_list()
{
  // As the image bitmap has been moved to the display_list, nothing required 
  // specific memory management activity.
  line_list.clear();
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
    while ((len < 6) && (*s != ';')) { s++; len++; }
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
  if (transform != CSS::NO_TRANSFORM) {
    if      (transform == CSS::UPPERCASE) u = toupper(u);
    else if (transform == CSS::LOWERCASE) u = tolower(u);
    else if (first && (transform == CSS::CAPITALIZE)) u = toupper(u);
  }
  return done ? u : '?';
}

void 
Page::put_str_at(std::string & str, int16_t xpos, int16_t ypos, const Format & fmt)
{
  TTF::BitmapGlyph * glyph;
  
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  const char * s = str.c_str(); 
  if (fmt.align == CSS::LEFT_ALIGN) {
    bool first = true;
    while (*s) {
      if ((glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first)))) {
        DisplayListEntry entry;
        entry.command = GLYPH;
        entry.kind.glyph_entry.glyph  = glyph;
        entry.x = xpos + glyph->xoff;
        entry.y = ypos + glyph->yoff;

        #if DEBUGGING
          if ((entry.x < 0) || (entry.y < 0)) {
            LOG_E(TAG, "Put_str_at with a negative location: %d %d", entry.x, entry.y);
          }
          else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
            LOG_E(TAG, "Put_str_at with a too large location: %d %d", entry.x, entry.y);
          }
        #endif

        display_list.push_back(entry);
      
        xpos += glyph->advance;
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

    if (xpos == -1) {
      if (fmt.align == CSS::CENTER_ALIGN) {
        x = min_x + ((max_x - min_x) >> 1) - (size >> 1);
      }
      else { // RIGHT_ALIGN
        x = max_x - size;
      }
    }
    else {
      if (fmt.align == CSS::CENTER_ALIGN) {
        x = xpos - (size >> 1);
      }
      else { // RIGHT_ALIGN
        x = xpos - size;
      }
    }

    s = str.c_str();
    bool first = true;
    while (*s) {
      if ((glyph = font->get_glyph(to_unicode(&s, fmt.text_transform, first)))) {
        
        DisplayListEntry entry;
        entry.command = GLYPH;
        entry.kind.glyph_entry.glyph = glyph;
        entry.x = x + glyph->xoff;
        entry.y = ypos + glyph->yoff;

        #if DEBUGGING
          if ((entry.x < 0) || (entry.y < 0)) {
            LOG_E(TAG, "Put_str_at with a negative location: %d %d", entry.x, entry.y);
          }
          else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
            LOG_E(TAG, "Put_str_at with a too large location: %d %d", entry.x, entry.y);
          }
        #endif

        display_list.push_back(entry);
      
        x += glyph->advance;
      }
      first = false;
    }
  }
}

void
Page::paint()
{
  if (display_list.size() == 0) return;
  if (compute_mode != DISPLAY) return;
  
  screen.clear();

  for (auto & entry : display_list) {
    if (entry.command == GLYPH) {
      screen.put_bitmap_invert(
        entry.kind.glyph_entry.glyph->buffer, 
        entry.kind.glyph_entry.glyph->width, 
        entry.kind.glyph_entry.glyph->rows, 
        entry.x, 
        entry.y);
    }
    else if (entry.command == IMAGE) {
      screen.put_bitmap(
        entry.kind.image_entry.image.bitmap, 
        entry.kind.image_entry.image.width, 
        entry.kind.image_entry.image.height, 
        entry.x, 
        entry.y);
    }
    else if (entry.command == HIGHLIGHT_REGION) {
      screen.put_highlight(
        entry.kind.region_entry.width, 
        entry.kind.region_entry.height, 
        entry.x, 
        entry.y);
    }
    else if (entry.command == CLEAR_REGION) {
      screen.clear_region(
        entry.kind.region_entry.width, 
        entry.kind.region_entry.height, 
        entry.x, 
        entry.y);
    }
  };

  screen.update();
}

void
Page::start(Format & fmt)
{
  xpos = fmt.screen_left;
  ypos = fmt.screen_top;

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

  // std::cout << "New Page:";
  // show_controls();
}

void
Page::set_limits(Format & fmt)
{
  xpos = ypos = 0;

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
Page::line_break(const Format & fmt)
{
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);
  
  // LOG_D(TAG, "line_break font index: %d", fmt.font_index);

  if (line_list.size() > 0) {
    add_line(fmt, false);
  }
  else {
    ypos += font->get_line_height() * fmt.line_height_factor;
    xpos  = min_x;
  }

  // xpos = min_x;
  // ypos += font->line_height();
  screen_is_full = screen_is_full || ((ypos - font->get_descender_height()) >= max_y);
  if (screen_is_full) {
    // std::cout << "Screen full:"; show_controls();
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
                    ((ypos + fmt.margin_top + 
                              (fmt.line_height_factor * font->get_line_height()) - 
                              font->get_descender_height()) > max_y);
    if (screen_is_full) { 
      // std::cout << "Screen full:"; show_controls(); 
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
  // the start of the page of the page before...
  
  if (recover) {
    para_indent = top_margin = 0;
  }
  else {
    para_indent = fmt.indent;
    top_margin  = fmt.margin_top;
  }
 
  // std::cout << "New Paragraph:"; show_controls();

  return true;
}

bool 
Page::end_paragraph(const Format & fmt)
{
  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (line_list.size() > 0) add_line(fmt, false);

  ypos += fmt.margin_bottom - font->get_descender_height();

  if ((ypos - font->get_descender_height()) >= max_y) {
    screen_is_full = true;
    return false;
  }
  // std::cout << "End Paragraph:"; show_controls();

  return true;
}

void
Page::add_line(const Format & fmt, bool justifyable)
{
  if (ypos == 0) ypos = min_y;

  xpos  = para_min_x + para_indent;
  if (xpos < min_x) xpos = min_x;
  int16_t line_height = glyphs_height * fmt.line_height_factor;
  ypos += top_margin + (line_height >> 1) + (glyphs_height >> 1);

  #if DEBUGGING_AID
    if (show_location) {
      std::cout << "New Line: ";
      show_controls(" ");
      show_fmt(fmt, "   ->  ");  
    }
  #endif

  // std::cout << "New Line:"
  //   " xpos:" << xpos <<
  //   " ypos:" << ypos << 
  //   " line_height:" << line_height << " ";

  // show_fmt(fmt);

  // book_view.line_added_at(ypos);

  // Get rid of space characters that are at the end of the line.
  // This is mainly required for the JUSTIFY alignment algo.
  while (line_list.size() > 0) {
    if (line_list.rbegin()->y > 0) line_list.pop_back(); else break;
  }

  if (line_list.size() > 0) {
    if ((fmt.align == CSS::JUSTIFY) && justifyable) {
      int16_t target_width = (para_max_x - para_min_x - para_indent);
      int16_t loop_count = 0;
      while ((line_width < target_width) && (++loop_count < 50)) {
        bool at_least_once = false;
        for (auto & entry : line_list) {
          if (entry.x > 0) {
            at_least_once = true;
            entry.x++;
            if (++line_width >= target_width) break;
          }
        }
        if (!at_least_once) break; // No space available in line to justify the line
      }
      if (loop_count >= 50) {
        for (auto & entry : line_list) entry.x = 0;
      }
    }
    else {
      if (fmt.align == CSS::RIGHT_ALIGN) {
        xpos = para_max_x - line_width;
      } 
      else if (fmt.align == CSS::CENTER_ALIGN) {
        xpos = para_min_x + ((para_max_x - para_min_x) >> 1) - (line_width >> 1);
      }
    }
  }
  
  for (auto & entry : line_list) {
    if (entry.command == GLYPH) {
      int16_t x = entry.x;
      entry.x = xpos + entry.kind.glyph_entry.glyph->xoff;
      entry.y = ypos + entry.kind.glyph_entry.glyph->yoff;
      xpos += (x == 0) ? entry.kind.glyph_entry.glyph->advance : x;
    }
    else if (entry.command == IMAGE) {
      entry.x = xpos;
      entry.y = ypos - entry.kind.image_entry.image.height;
      xpos += entry.kind.image_entry.advance;
    }
    else {
      LOG_E(TAG, "Wrong entry type for add_line: %d", entry.command);
    }

    #if DEBUGGING
      if ((entry.x < 0) || (entry.y < 0)) {
        LOG_E(TAG, "add_line entry with a negative location: %d %d %d", entry.x, entry.y, entry.command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
      else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
        LOG_E(TAG, "add_line with a too large location: %d %d %d", entry.x, entry.y, entry.command);
        show_controls("  -> ");
        show_fmt(fmt, "  -> ");
      }
    #endif

    display_list.push_back(std::move(entry));
  };

  
  line_width = line_height = glyphs_height = 0;
  clear_line_list();

  para_indent = 0;
  top_margin = 0;

  // std::cout << "End New Line:"; show_controls();
}

inline void 
Page::add_glyph_to_line(TTF::BitmapGlyph * glyph, TTF & font, bool is_space)
{
  if (is_space && (line_width == 0)) return;

  DisplayListEntry entry;
  entry.command = GLYPH;
  entry.kind.glyph_entry.glyph = glyph;
  entry.x = entry.y = is_space ?  glyph->advance : 0;
  
  if (glyphs_height < glyph->root->get_line_height()) glyphs_height= glyph->root->get_line_height();

  line_width += (glyph->advance);

  line_list.push_back(std::move(entry));
}

void 
Page::add_image_to_line(Image & image, int16_t advance, bool copy)
{
  DisplayListEntry entry;
  entry.command = IMAGE;
  entry.kind.image_entry.image   = image;
  entry.kind.image_entry.advance = advance;
  
  if (compute_mode == DISPLAY) {
    if (copy) {
      int32_t size = image.width * image.height;
      if (image.bitmap != nullptr) {
        entry.kind.image_entry.image.bitmap = new unsigned char[size];
        memcpy((void *) entry.kind.image_entry.image.bitmap, image.bitmap, size);
      }
      else {
        entry.kind.image_entry.image.bitmap = nullptr;
      }
    }
    else {
      entry.kind.image_entry.image.bitmap = image.bitmap;
    }
  }
  else {
    entry.kind.image_entry.image.bitmap = nullptr;
  }
  entry.x = entry.y = 0;
  
  if (glyphs_height < image.height) glyphs_height = image.height;

  line_width += advance;

  line_list.push_back(std::move(entry));
}

#define NEXT_LINE_REQUIRED_SPACE (ypos + (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height())

bool 
Page::add_word(const char * word,  const Format & fmt)
{
  TTF::BitmapGlyph * glyph;
  const char * str = word;
  int32_t code;

  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (font == nullptr) return false;

  if (line_list.size() == 0) {
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

  if ((line_width + width) >= (para_max_x - para_min_x - para_indent)) {
    add_line(fmt, true);
    screen_is_full = NEXT_LINE_REQUIRED_SPACE > max_y;
    if (screen_is_full) {
      return false;
    }
    if (width >= (para_max_x - para_min_x - para_indent)) {
      LOG_E(TAG, "WORD TOO LARGE!! '%s'", word);
    }
  }

  first = true;
  while (*word) {
    if (font) {
      if ((glyph = font->get_glyph(code = to_unicode(&word, fmt.text_transform, first))) == nullptr) {
        glyph = font->get_glyph('?');
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

bool 
Page::add_char(const char * ch, const Format & fmt)
{
  TTF::BitmapGlyph * glyph;

  TTF * font = fonts.get(fmt.font_index, fmt.font_size);

  if (font == nullptr) return false;

  if (screen_is_full) return false;

  if (line_list.size() == 0) {
    
    // We are about to start a new line. Check if it will fit on the page.

    screen_is_full = ((ypos + (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height())) > max_y;
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

  if (fmt.width || fmt.height) {
    if (fmt.width) {
      // fmt.width -> image.width
      // h         -> image.height
      h = ((int32_t) image.height * fmt.width) / image.width;
      w = fmt.width;
    } 
    if (fmt.height && (h > fmt.height)) {
      // fmt.height -> image.height
      // w          -> image.width
      w = ((int32_t) image.width * fmt.height) / image.height;
      h = fmt.height;
    }
    advance = glyph->advance + w;
  }
  else {
    int16_t avail_width = para_max_x - para_min_x - line_width;
    int16_t avail_height = (ypos == min_y) ? 
                              max_y - min_y : 
                              max_y - (ypos + (fmt.line_height_factor * font->get_line_height()));

    if ((avail_height < image.height) && (avail_height < 50)) return false;
    if ((image.height > (font->get_line_height() << 2)) && 
        ((image.width > avail_width) || (image.height > avail_height))) {

      w = avail_width;
      h = ((int32_t) image.height * avail_width) / image.width;

      if ((h < 0) || (w < 0)) return false;
      
      if (h > avail_height) {
        h = avail_height;
        w = ((int32_t) image.width * avail_height) / image.height;
      }
      
      advance = (ypos == min_y) ? w : glyph->advance + w; 
    }
    else {
      w = image.width;
      h = image.height;
      advance = glyph->advance + w;
    }
  }

  // Verify that there is enough room for the bitmap on the line
  if ((line_width + advance) >= (para_max_x - para_min_x - para_indent)) {
    add_line(fmt, true); 

    int16_t the_height = (fmt.line_height_factor * font->get_line_height()) - font->get_descender_height();
    if (the_height < h) the_height = h;

    screen_is_full = (ypos + h) > max_y;

    if (screen_is_full) return false;
  }

  if ((w != image.width) || (h != image.height)) {

    unsigned char * resized_bitmap = nullptr;

    if (compute_mode == DISPLAY) {
      if ((image.width > 2) || (image.height > 2)) {

        resized_bitmap = new unsigned char[w * h];

        stbir_resize_uint8(image.bitmap,   image.width, image.height, 0,
                          resized_bitmap, w,           h,            0,
                          1);
      }
    }

    resized_image.bitmap = resized_bitmap;
    resized_image.width  = w;
    resized_image.height = h;

    add_image_to_line(resized_image, advance, false);
  }
  else {
    add_image_to_line(image, advance, true);
  }

  return true;
}

void
Page::put_text(std::string str, const Format & fmt)
{
  Format myfmt = fmt;

  char * buff = new char[100];
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
                int16_t x, int16_t y)
{
  DisplayListEntry entry;

  if (compute_mode == DISPLAY) {
    int32_t size = image.width * image.height;
    entry.kind.image_entry.image.bitmap = new unsigned char [size];
    memcpy((void *)entry.kind.image_entry.image.bitmap, image.bitmap, size);
  }
  else {
    entry.kind.image_entry.image.bitmap = nullptr;
  }

  entry.command = IMAGE;
  entry.kind.image_entry.image.width  = image.width;
  entry.kind.image_entry.image.height = image.height;
  entry.x       = x;
  entry.y       = y;

  #if DEBUGGING
    if ((entry.x < 0) || (entry.y < 0)) {
      LOG_E(TAG, "put_bitmap with a negative location: %d %d", entry.x, entry.y);
    }
    else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
      LOG_E(TAG, "put_bitmap with a too large location: %d %d", entry.x, entry.y);
    }
  #endif

  display_list.push_back(entry);
}

void 
Page::put_highlight( 
            int16_t width, int16_t height, 
            int16_t x, int16_t y)
{
  DisplayListEntry entry;

  entry.command = HIGHLIGHT_REGION;
  entry.kind.region_entry.width   = width;
  entry.kind.region_entry.height  = height;
  entry.x       = x;
  entry.y       = y;

  #if DEBUGGING
    if ((entry.x < 0) || (entry.y < 0)) {
      LOG_E(TAG, "put_highlight with a negative location: %d %d", entry.x, entry.y);
    }
    else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
      LOG_E(TAG, "put_highlight with a too large location: %d %d", entry.x, entry.y);
    }
  #endif

  display_list.push_back(entry);
}

void 
Page::clear_region( 
            int16_t width, int16_t height, 
            int16_t x, int16_t y)
{
  DisplayListEntry entry;

  entry.command = CLEAR_REGION;
  entry.kind.region_entry.width   = width;
  entry.kind.region_entry.height  = height;
  entry.x       = x;
  entry.y       = y;

  entry.x       = x;
  entry.y       = y;

  #if DEBUGGING
    if ((entry.x < 0) || (entry.y < 0)) {
      LOG_E(TAG, "Put_str_at with a negative location: %d %d", entry.x, entry.y);
    }
    else if ((entry.x >= Screen::WIDTH) || (entry.y >= Screen::HEIGHT)) {
      LOG_E(TAG, "Put_str_at with a too large location: %d %d", entry.x, entry.y);
    }
  #endif

  display_list.push_back(entry);
}

bool
Page::show_cover(unsigned char * data, int32_t size)
{
  if (compute_mode == DISPLAY) {
    int32_t image_width, image_height, channel_count;

    unsigned char * bitmap_data = stbi_load_from_memory(data, size, &image_width, &image_height, &channel_count, 1);

    if (bitmap_data != nullptr) { 
      // LOG_D(TAG, "Image: width: %d height: %d channel_count: %d", image_width, image_height, channel_count);

      int32_t w = Screen::WIDTH;
      int32_t h = image_height * Screen::WIDTH / image_width;

      if (h > Screen::HEIGHT) {
        h = Screen::HEIGHT;
        w = image_width * Screen::HEIGHT / image_height;
      }

      int32_t x = (Screen::WIDTH  - w) >> 1;
      int32_t y = (Screen::HEIGHT - h) >> 1;

      // LOG_D(TAG, "Image Parameters: %d %d %d %d", w, h, x, y);

      unsigned char * resized_bitmap = new unsigned char[w * h];

      stbir_resize_uint8(bitmap_data,    image_width, image_height, 0,
                        resized_bitmap, w,           h,            0,
                        1);
      screen.clear();
      screen.put_bitmap(resized_bitmap, w, h, x, y);
      screen.update();

      delete [] resized_bitmap;
    }
    else {
      LOG_E(TAG, "Unable to load cover file");
    }

    stbi_image_free(bitmap_data);
  }

  return true;
}