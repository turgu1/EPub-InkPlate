// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "viewers/book_viewer.hpp"

#include "models/ttf2.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE6_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"
#include "alloc.hpp"

#include <iomanip>
#include <cstring>
#include <iostream>
#include <sstream>

#include "stb_image.h"
#include "stb_image_resize.h"
#include "logging.hpp"

using namespace pugi;

void 
BookViewer::page_locs_end_page(Page::Format & fmt)
{
  EPub::Location loc;

  loc.itemref_index = epub.get_itemref_index();
  loc.offset = start_of_page_offset;
  loc.size = current_offset - start_of_page_offset;

  epub.add_page_loc(loc);

  // LOG_D("Page %d, offset: %d, size: %d", epub.get_page_count(), loc.offset, loc.size);
  
  start_of_page_offset = current_offset;

  page.start(fmt);

  SET_PAGE_TO_SHOW(epub.get_page_count())
}

bool
BookViewer::page_locs_recurse(xml_node node, Page::Format fmt)
{
  if (node == nullptr) return false;
  
  const char * name;
  bool para = false;
  bool image_is_present = false;
  Page::Image image;

  if (*(name = node.name())) { // A name is attached to the node.
    Elements::iterator element_it;

    xml_attribute attr = node.attribute("style");
    CSS::Properties * element_properties = nullptr;
    if (attr) {
      const char * buffer = attr.value();
      const char * end = buffer + strlen(buffer);
      element_properties = CSS::parse_properties(&buffer, end, buffer);
    }

    adjust_format(node, fmt, element_properties); // Adjust format from element attributes

    if (element_properties) {
      CSS::clear_properties(element_properties);
      element_properties = nullptr;
    }

    std::string the_name = name;
    if ((element_it = elements.find(the_name)) != elements.end()) {
      
      switch (element_it->second) {
        case BODY:
        case DIV:
        case SPAN:
        case A:
          break;
      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;
      #else
        case IMG: {
            xml_attribute attr = node.attribute("src");
            if (attr != nullptr) {
              std::string filename = attr.value();
              image_is_present = get_image(filename, image);
            }
          }
          break;
        case IMAGE: {
            xml_attribute attr = node.attribute("xlink:href");
            if (attr != nullptr) {
              std::string filename = attr.value();
              image_is_present = get_image(filename, image);
            }
          }
          break;
      #endif
        case LI:
        case P:
          para = true; start_of_paragraph = true;
          break;
        case BR:
          SHOW_LOCATION("Page Break");
          if (!page.line_break(fmt)) {
            page_locs_end_page(fmt);
            SHOW_LOCATION("Page Break");
            page.line_break(fmt);
          }
          current_offset++;
          break;
        case B: {
            Fonts::FaceStyle style = fmt.font_style;
            if (style == Fonts::NORMAL) style = Fonts::BOLD;
            else if (style == Fonts::ITALIC) style = Fonts::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case I:
        case EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if (style == Fonts::NORMAL) style = Fonts::ITALIC;
            else if (style == Fonts::BOLD) style = Fonts::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case H1:
          fmt.font_size   = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H2:
          fmt.font_size   = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H3:
          fmt.font_size   = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H4:
          para = true; start_of_paragraph = true;
          break;
        case H5:
          fmt.font_size   = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H6:
          fmt.font_size   = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
      }
    }

    if (image_is_present) {
      if (!page.add_image(image, fmt)) {
        page_locs_end_page(fmt);
        if (start_of_paragraph) {
          SHOW_LOCATION("New Paragraph 1");
          page.new_paragraph(fmt, true);
        }
        page.add_image(image, fmt);
      }
      stbi_image_free((void *) image.bitmap);

      current_offset++;
    }
    else {
      if (para) {
        SHOW_LOCATION("New Paragraph 2");
        if (!page.new_paragraph(fmt)) {
          page_locs_end_page(fmt);
          SHOW_LOCATION("New Paragraph 2");
          page.new_paragraph(fmt);
        }
      } 

      xml_node sub = node.first_child();
      while (sub) {
        page_locs_recurse(sub, fmt);
        sub = sub.next_sibling();
      }

      if (para) {
        SHOW_LOCATION("End Paragraph 1");
        if ((current_offset != start_of_page_offset) || page.some_data_waiting()) {
          if (!page.end_paragraph(fmt)) {
            page_locs_end_page(fmt);
            SHOW_LOCATION("End Paragraph 1");
            if (page.some_data_waiting()) page.end_paragraph(fmt);
          }
        }
        start_of_paragraph = false;
      } 

      // In case that we are at the end of an html file and there remains
      // characters in the page pipeline, we call end_paragraph() to get them out on the page...
      if ((element_it != elements.end()) && (element_it->second == BODY)) {
        SHOW_LOCATION("End Paragraph 2");
        if (!page.end_paragraph(fmt)) {
          page_locs_end_page(fmt);
          SHOW_LOCATION("End Paragraph 2");
          if (page.some_data_waiting()) page.end_paragraph(fmt);
        }
      }
    }
  }
  else {
    const char * str = node.value();
    SHOW_LOCATION("->");

    while (*str) {
      if (uint8_t(*str) <= ' ') {
        if (*str == ' ') {
          fmt.trim = *str == ' ';
          if (!page.add_char(str, fmt)) {
            page_locs_end_page(fmt);
            if (start_of_paragraph) {
              SHOW_LOCATION("New Paragraph 3");
              page.new_paragraph(fmt, false);
            }
            else {
              SHOW_LOCATION("New Paragraph 4");
              page.new_paragraph(fmt, true);
            }
          }
        }
        str++;
        current_offset++; // Not an UTF-8, so it's ok...
      }
      else {
        const char * w = str;
        int32_t count = 0;
        while (uint8_t(*str) > ' ') { str++; count++; }
        std::string word;
        word.assign(w, count);
        if (!page.add_word(word.c_str(), fmt)) {
          page_locs_end_page(fmt);
          if (start_of_paragraph) {
            SHOW_LOCATION("New Paragraph 5");
            page.new_paragraph(fmt, false);
          }
          else {
            SHOW_LOCATION("New Paragraph 6");
            page.new_paragraph(fmt, true);
          }
          page.add_word(word.c_str(), fmt);
        }
        current_offset += count;
        start_of_paragraph = false;
      }
    }
  }
 
  return true;
}

bool
BookViewer::build_page_locs()
{
  bool done   = false;
  TTF * font  = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);

  page.set_compute_mode(Page::LOCATION);

  epub.clear_page_locs(1500);

  if (epub.get_first_item()) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::NORMAL)) == -1) {
      idx = 1;
    }
    
    int8_t font_size;
    config.get(Config::FONT_SIZE, &font_size);

    Page::Format fmt = {
      .line_height_factor = 0.9,
      .font_index         = idx,
      .font_size          = font_size,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = 10,
      .screen_right       = 10,
      .screen_top         = 10,
      .screen_bottom      = page_bottom,
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
      .text_transform     = CSS::NO_TRANSFORM
    };

    last_props        = nullptr;

    while (!done) {

      // Process for each file part of the document

      current_offset       = 0;
      start_of_page_offset = 0;
      xml_node node = epub.current_item.child("html");

      if (node && 
         (node = node.child("body"))) {

        page.start(fmt);

        if (!page_locs_recurse(node, fmt)) break;
      }
      else {
        break;
      }

      page_locs_end_page(fmt);
      
      //msg_viewer.add_dot(); // Show progression to the user

      done = !epub.get_next_item();
    }
  }

  return done;
}

int16_t
BookViewer::get_pixel_value(const CSS::Value & value, const Page::Format & fmt, int16_t ref)
{
  switch (value.value_type) {
    case CSS::PX:
      return value.num;
    case CSS::PT:
      return (value.num * Screen::RESOLUTION) / 72;
    case CSS::EM:
      {
        TTF * f = fonts.get(fmt.font_index, fmt.font_size);
        return value.num * f->get_em_width();
      }
    case CSS::PERCENT:
      return (value.num * ref) / 100;
    case CSS::NOTYPE:
      return ref * value.num;
    case CSS::CM:
      return (value.num * Screen::RESOLUTION) / 2.54;
    case CSS::VH:
      return (value.num * (fmt.screen_bottom - fmt.screen_top)) / 100;
    case CSS::VW:
      return (value.num * (fmt.screen_right - fmt.screen_left)) / 100;;
    case CSS::STR:
      // LOG_D("get_pixel_value(): Str value: %s", value.str.c_str());
      return 0;
      break;
    default:
      // LOG_D("get_pixel_value: Wrong data type!: %d", value.value_type);
      return value.num;
  }
  return 0;
}

int16_t
BookViewer::get_point_value(const CSS::Value & value, const Page::Format & fmt, int16_t ref)
{
  switch (value.value_type) {
    case CSS::PX:
      //  pixels -> Screen HResolution per inch
      //    x    ->    72 pixels per inch
      return (value.num * 72) / Screen::RESOLUTION; // convert pixels in points
    case CSS::PT:
      return value.num;  // Already in points
    case CSS::EM:
        return value.num * ref;
    case CSS::CM:
      return (value.num * 72) / 2.54;
    case CSS::PERCENT:
      return (value.num * ref) / 100;
    case CSS::NOTYPE:
      return ref * value.num;
    case CSS::STR:
      LOG_D("get_point_value(): Str value: %s.", value.str.c_str());
      return 0;
      break;
    case CSS::VH:
      return ((value.num * (fmt.screen_bottom - fmt.screen_top)) / 100) * 72 / Screen::RESOLUTION;
    case CSS::VW:
      return ((value.num * (fmt.screen_right - fmt.screen_left)) / 100) * 72 / Screen::RESOLUTION;
     default:
      LOG_E("get_point_value(): Wrong data type!");
      return value.num;
  }
  return 0;
}

float
BookViewer::get_factor_value(const CSS::Value & value, const Page::Format & fmt, float ref)
{
  switch (value.value_type) {
    case CSS::PX:
    case CSS::PT:
    case CSS::STR:
      return 1.0;  
    case CSS::EM:
    case CSS::NOTYPE:
      return value.num;
    case CSS::PERCENT:
      return (value.num * ref) / 100.0;
    default:
      // LOG_E("get_factor_value: Wrong data type!");
      return 1.0;
  }
  return 0;
}

void
BookViewer::adjust_format(xml_node node, 
                        Page::Format & fmt,
                        CSS::Properties * element_properties)
{
  xml_attribute attr = node.attribute("class");

  const CSS::PropertySuite * suite = nullptr;
  const CSS * current_item_css = epub.get_current_item_css();

  std::string element_name = node.name();

  if (current_item_css) {
    if (attr) {
      std::stringstream ss(attr.value());
      std::string class_name;

      while (std::getline (ss, class_name, ' ')) {
        suite = current_item_css->search(element_name, class_name);
        if (suite) adjust_format_from_suite(fmt, *suite);
      }
    }
    else {
      if (!suite) suite = current_item_css->search(element_name, "");
      if (suite) adjust_format_from_suite(fmt, *suite);
    }
  }

  if (element_properties) {
    CSS::PropertySuite suite;
    suite.push_front(element_properties);
    adjust_format_from_suite(fmt, suite);
  }
}

void
BookViewer::adjust_format_from_suite(Page::Format & fmt, const CSS::PropertySuite & suite)
{  
  const CSS::Values * vals;

  // LOG_D("Found!");

  Fonts::FaceStyle font_weight = ((fmt.font_style == Fonts::BOLD) || (fmt.font_style == Fonts::BOLD_ITALIC)) ? Fonts::BOLD : Fonts::NORMAL;
  Fonts::FaceStyle font_style = ((fmt.font_style == Fonts::ITALIC) || (fmt.font_style == Fonts::BOLD_ITALIC)) ? Fonts::ITALIC : Fonts::NORMAL;
  
  if ((vals = CSS::get_values_from_suite(suite, CSS::FONT_STYLE))) {
    font_style = (Fonts::FaceStyle) vals->front()->choice;
  }
  if ((vals = CSS::get_values_from_suite(suite, CSS::FONT_WEIGHT))) {
    font_weight = (Fonts::FaceStyle) vals->front()->choice;
  }
  Fonts::FaceStyle new_style = fonts.adjust_font_style(fmt.font_style, font_style, font_weight);

  if ((vals = CSS::get_values_from_suite(suite, CSS::FONT_FAMILY))) {
    int16_t idx = -1;
    for (auto & font_name : *vals) {
      if ((idx = fonts.get_index(font_name->str, new_style)) != -1) break;
    }
    if (idx == -1) {
      // LOG_E("Font not found 1: %s %s", vals->at(0).str.c_str(), new_style);
      idx = fonts.get_index("Default", new_style);
    }
    if (idx == -1) {
      fmt.font_style = Fonts::NORMAL;
      fmt.font_index = 1;
    }
    else {
      fmt.font_index = idx;
      fmt.font_style = new_style;
    }
  }
  else if (new_style != fmt.font_style) {
    reset_font_index(fmt, new_style);
  }

  fonts.check(fmt.font_index, fmt.font_style);

  if ((vals = CSS::get_values_from_suite(suite, CSS::TEXT_ALIGN))) {
    fmt.align = (CSS::Align) vals->front()->choice;
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::TEXT_INDENT))) {
    fmt.indent = get_pixel_value(*(vals->front()), fmt, page.paint_width());
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::FONT_SIZE))) {
    fmt.font_size = get_point_value(*(vals->front()), fmt, fmt.font_size);
    if (fmt.font_size == 0) {
      LOG_E("adjust_format_from_suite: setting fmt.font_size to 0!!!");
    }
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::LINE_HEIGHT))) {
    fmt.line_height_factor = get_factor_value(*(vals->front()), fmt, fmt.line_height_factor);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::MARGIN))) {
    int16_t size = 0;
    for (auto val __attribute__ ((unused)) : *vals) size++;
    CSS::Values::const_iterator it = vals->begin();
    if (size == 1) {
      fmt.margin_top   = fmt.margin_bottom = 
      fmt.margin_right = fmt.margin_left   = get_pixel_value(*(vals->front()), fmt, fmt.margin_left  );
    }
    else if (size == 2) {
      fmt.margin_top   = fmt.margin_bottom = get_pixel_value(**it++, fmt, fmt.margin_top   );
      fmt.margin_right = fmt.margin_left   = get_pixel_value(**it,   fmt, fmt.margin_right );
    }
    else if (size == 3) {
      fmt.margin_top                       = get_pixel_value(**it++, fmt, fmt.margin_top   );
      fmt.margin_right = fmt.margin_left   = get_pixel_value(**it++, fmt, fmt.margin_left  );
      fmt.margin_bottom                    = get_pixel_value(**it,   fmt, fmt.margin_bottom);
    }
    else if (size == 4) {
      fmt.margin_top                       = get_pixel_value(**it++, fmt, fmt.margin_top   );
      fmt.margin_right                     = get_pixel_value(**it++, fmt, fmt.margin_right );
      fmt.margin_bottom                    = get_pixel_value(**it++, fmt, fmt.margin_bottom);
      fmt.margin_left                      = get_pixel_value(**it,   fmt, fmt.margin_left  );
    }
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::MARGIN_LEFT))) {
    fmt.margin_left = get_pixel_value(*(vals->front()), fmt, fmt.margin_left);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::MARGIN_RIGHT))) {
    fmt.margin_right = get_pixel_value(*(vals->front()), fmt, fmt.margin_right);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::MARGIN_TOP))) {
    fmt.margin_top = get_pixel_value(*(vals->front()), fmt, fmt.margin_top);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::MARGIN_BOTTOM))) {
    fmt.margin_bottom = get_pixel_value(*(vals->front()), fmt, fmt.margin_bottom);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::WIDTH))) {
    fmt.width = get_pixel_value(*(vals->front()), fmt, Screen::WIDTH - fmt.screen_left - fmt.screen_right);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::HEIGHT))) {
    fmt.height = get_pixel_value(*(vals->front()), fmt, Screen::HEIGHT - fmt.screen_top - fmt.screen_bottom);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::TEXT_TRANSFORM))) {
    fmt.text_transform = (CSS::TextTransform) vals->front()->choice;
  }
}

bool 
BookViewer::get_image(std::string & filename, Page::Image & image)
{
  int16_t channel_count;
  std::string fname = epub.get_current_item_file_path();
  fname.append(filename);
  if (epub.get_image(fname, image, channel_count)) {
    if (channel_count != 1) {
      bool all_zero = true;
      for (int i = 0; i < (image.width * image.height); i++) {
        if (image.bitmap[i] != 0) { all_zero = false; break; }
      }
      if (all_zero) {
        LOG_E("Bitmap is all zeroes...");
      }
      // malloc is used as bitmap2 will be freed by stbi_image.
      unsigned char * bitmap2 = (unsigned char *) allocate(image.width * image.height);
      stbir_resize_uint8(image.bitmap, image.width, image.height, 0, 
                         bitmap2,      image.width, image.height, 0,
                         1);

      stbi_image_free((void *) image.bitmap);
      image.bitmap = bitmap2; 
    }
    return true;
  }

  return false;
}

bool
BookViewer::build_page_recurse(pugi::xml_node node, Page::Format fmt)
{
  if (node == nullptr) return false;
  if (page.is_full()) return true;

  const char * name;
  bool para             = false;
  bool image_is_present = false;
  Page::Image image;

  // If node->name() is not an empty string, this is the start of a named element

  if (*(name = node.name())) { 

    // LOG_D("Node name: %s", name);

    // Do it only if we are now in the current page content

    bool started = current_offset >= start_of_page_offset;

    if (started) page.set_compute_mode(Page::DISPLAY);

    xml_attribute attr = node.attribute("style");
    CSS::Properties * element_properties = nullptr;
    if (attr) {
      const char * buffer = attr.value();
      const char * end = buffer + strlen(buffer);
      element_properties = CSS::parse_properties(&buffer, end, buffer);
    }

    adjust_format(node, fmt, element_properties); // Adjust format from element attributes

    if (element_properties) {
      CSS::clear_properties(element_properties);
      element_properties = nullptr;
    }
      
    Elements::iterator element_it;

    if ((element_it = elements.find(std::string(name))) != elements.end()) {

      switch (element_it->second) {
        case BODY:
        case DIV:
        case SPAN:
        case A:
          break;
      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;
      #else
        case IMAGE: {
            xml_attribute attr = node.attribute("xlink:href");
            if (attr != nullptr) {
              std::string filename = attr.value();
              image_is_present = get_image(filename, image);
            }
          }
          break;
        case IMG: {
            if (started) { 
              xml_attribute attr = node.attribute("src");
              if (attr != nullptr) {
                std::string filename = attr.value();
                image_is_present = get_image(filename, image);
              }
            }
            else {
              image_is_present = true;
            }
          }
          break;
      #endif
        case LI:
        case P:
          para = true; start_of_paragraph = true;
          // LOG_D("Para: %d %d", fmt.font_index, fmt.font_size);
          break;
        case BR:
          if (started) {
            SHOW_LOCATION("Page Break");
            if (!page.line_break(fmt)) return true;
          }
          current_offset++;
          break;
        case B: {
            Fonts::FaceStyle style = fmt.font_style;
            if (style == Fonts::NORMAL) style = Fonts::BOLD;
            else if (style == Fonts::ITALIC) style = Fonts::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break; 
        case I:
        case EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if (style == Fonts::NORMAL) style = Fonts::ITALIC;
            else if (style == Fonts::BOLD) style = Fonts::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case H1:
          fmt.font_size   = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H2:
          fmt.font_size   = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H3:
          fmt.font_size   = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H4:
          para = true; start_of_paragraph = true;
          break;
        case H5:
          fmt.font_size   = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
        case H6:
          fmt.font_size   = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          para = true; start_of_paragraph = true;
          break;
      }
    }
    
    if (current_offset >= end_of_page_offset) return true;
    if (image_is_present) {
      if ((current_offset + 1) <= start_of_page_offset) {
        // As we move from the beginning of a file, we bypass everything that is there before
        // the start of the page offset
        current_offset++;
      }
      else {
        if ((current_offset == start_of_page_offset) && start_of_paragraph) {
          SHOW_LOCATION("New Paragraph 1");
          page.new_paragraph(fmt);
          start_of_paragraph = false;
        }
        if (started && (current_offset < end_of_page_offset)) {
          if (!page.add_image(image, fmt)) {
            stbi_image_free((void *) image.bitmap);
            return true;
          }
        }
        stbi_image_free((void *) image.bitmap);

        current_offset++;
      }
    }
    else {
      if (started && para) {
        SHOW_LOCATION("New Paragraph 2");
        page.new_paragraph(fmt);
        start_of_paragraph = false;
      }

      xml_node sub = node.first_child();
      while (sub) {
        if (page.is_full()) return true;
        if (!build_page_recurse(sub, fmt)) {
          if (page.is_full()) return true;
          return false;
        }
        sub = sub.next_sibling();
      }

      if (current_offset >= start_of_page_offset) {
        page.set_compute_mode(Page::DISPLAY);
        if (para) {
          SHOW_LOCATION("End Paragraph 1");
          if ((current_offset != start_of_page_offset) || page.some_data_waiting()) page.end_paragraph(fmt);
          start_of_paragraph = false;
        }
        // In case that we are at the end of an html file and there remains
        // characters in the page pipeline, to get them out on the page...
        if ((element_it != elements.end()) && (element_it->second == BODY)) {
          SHOW_LOCATION("End Paragraph 2");
          page.end_paragraph(fmt);
        }
      }
    }
  }
  else {
    // We look now at the node content and prepare the glyphs to be put on a page.
    const char * str = node.value();
    int16_t size;

    if (current_offset >= end_of_page_offset) return true;

    if (current_offset + (size = strlen(str)) <= start_of_page_offset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      current_offset += size;
    }
    else {
      SHOW_LOCATION("->");

      if ((current_offset <= start_of_page_offset) && start_of_paragraph) {
         SHOW_LOCATION("New Paragraph 3");
         page.new_paragraph(fmt, true);
      }
      if (current_offset >= start_of_paragraph) page.set_compute_mode(Page::DISPLAY);
      while (*str) {
        if (uint8_t(*str) <= ' ') {
          if (current_offset >= start_of_page_offset) {
            if (*str == ' ') {
              fmt.trim = *str == ' '; // white spaces will be trimmed at the beginning and the end of a line
              if (!page.add_char(str, fmt)) break;
            }
          }
          str++;
          current_offset++; // Not an UTF-8, so it's ok...
          start_of_paragraph = false;
        }
        else {
          const char * w = str;
          int16_t count = 0;
          while (uint8_t(*str) > ' ') { str++; count++; }
          if (current_offset >= start_of_page_offset) {
            std::string word;
            word.assign(w, count);
            if (!page.add_word(word.c_str(), fmt)) break;
          }
          current_offset += count;
          start_of_paragraph = false;
        }
        if (page.is_full()) break;
      }
    }
  }

  return true;
}

void
BookViewer::build_page_at(const EPub::Location & loc)
{
  TTF * font = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);

  page.set_compute_mode(Page::MOVE);

  if (epub.get_item_at_index(loc.itemref_index)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::NORMAL)) == -1) {
      idx = 1;
    }

    int8_t font_size;
    config.get(Config::FONT_SIZE, &font_size);

    Page::Format fmt = {
      .line_height_factor = 0.9,
      .font_index         = idx,
      .font_size          = font_size,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = 10,
      .screen_right       = 10,
      .screen_top         = 10,
      .screen_bottom      = page_bottom,
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
      .text_transform     = CSS::NO_TRANSFORM
    };

    last_props        = nullptr;

    current_offset       = 0;
    start_of_page_offset = loc.offset;
    end_of_page_offset   = loc.offset + loc.size;
    xml_node node    = epub.current_item.child("html");

    SET_PAGE_TO_SHOW(current_page_nbr);

    if (node && 
        (node = node.child("body"))) {

      page.start(fmt);

      if (build_page_recurse(node, fmt)) {

        //TTF * font = fonts.get(0, 7);

        fmt.line_height_factor = 1.0;
        fmt.font_index         = 1;
        fmt.font_size          = 9;
        fmt.font_style         = Fonts::NORMAL;
        fmt.align              = CSS::CENTER_ALIGN;

        std::ostringstream ostr;
        ostr << current_page_nbr + 1 << " / " << epub.get_page_count();
        std::string str = ostr.str();

        page.put_str_at(str, -1, Screen::HEIGHT + font->get_descender_height() - 2, fmt);

        #if EPUB_INKPLATE6_BUILD
          BatteryViewer::show();
        #endif

        page.paint();
      }
    }

    if (current_offset != end_of_page_offset) {
      LOG_E("Current page offset and end of page offset differ: %d vd %d", current_offset, end_of_page_offset);
    }
  }
}

void
BookViewer::show_page(int16_t page_nbr)
{
  // LOG_D("Page: %d", page_nbr + 1);
 
  current_page_nbr = page_nbr;
  if (page_nbr == 0) {
    const char * filename = epub.get_cover_filename();
    if (filename != nullptr) {
      // LOG_D("Cover filename: %s", filename);
      uint32_t size;
      unsigned char * data = (unsigned char *) epub.retrieve_file(filename, size);

      if (data == NULL) {
        LOG_E("Unable to retrieve cover file: %s", filename);
      }
      else {
        page.show_cover(data, size);
        free(data);
      }

    }
    else {
      LOG_D("There doesn't seems to have any cover.");
    }
  }
  else if (page_nbr < epub.get_page_count()) {
    build_page_at(epub.get_page_loc(page_nbr));
  }
}