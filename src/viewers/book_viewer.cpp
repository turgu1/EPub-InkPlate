// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "viewers/book_viewer.hpp"

#include "models/ttf2.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"
#include "image_info.hpp"

#if EPUB_INKPLATE_BUILD
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

#if !EPUB_LINUX_BUILD
  SemaphoreHandle_t BookViewer::mutex = nullptr;
  StaticSemaphore_t BookViewer::mutex_buffer;
#endif

void 
BookViewer::page_locs_end_page(Page::Format & fmt)
{
  if (!page.is_empty()) {

    PageLocs::PageId   page_id   = PageLocs::PageId(epub.get_itemref_index(), start_of_page_offset);
    PageLocs::PageInfo page_info = PageLocs::PageInfo(current_offset - start_of_page_offset, -1);
    
    page_locs.insert(page_id, page_info);

    // LOG_D("Page %d, offset: %d, size: %d", epub.get_page_count(), loc.offset, loc.size);
 
    SET_PAGE_TO_SHOW(epub.get_page_count()) // Debugging stuff
  }

  start_of_page_offset = current_offset;

  page.start(fmt);
}

bool
BookViewer::page_locs_recurse(xml_node node, Page::Format fmt)
{
  if (node == nullptr) return false;
  
  const char * name;
  const char * str = nullptr;
  std::string image_filename;

  image_filename.clear();

  Elements::iterator element_it = elements.end();
  
  bool named_element = *(name = node.name()) != 0;

  if (named_element) { // A name is attached to the node.

    fmt.display = CSS::Display::INLINE;

    if ((element_it = elements.find(name)) != elements.end()) {

      //LOG_D("==> %10s [%5d] %4d", name, current_offset, page.get_pos_y());

      switch (element_it->second) {
        case Element::BODY:
        case Element::SPAN:
        case Element::A:
          break;
      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;
      #else
        case Element::IMG: 
          if (show_images) {
            xml_attribute attr = node.attribute("src");
            if (attr != nullptr) image_filename = attr.value();
            else current_offset++;
          }
          else {
            xml_attribute attr = node.attribute("alt");
            if (attr != nullptr) str = attr.value();
            else current_offset++;
          }
          break;
        case Element::IMAGE: 
          if (show_images) {
            xml_attribute attr = node.attribute("xlink:href");
            if (attr != nullptr) image_filename = attr.value();
            else current_offset++;
          }
          break;
      #endif
        case Element::PRE:
          fmt.pre = start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::LI:
        case Element::DIV:
        case Element::BLOCKQUOTE:
        case Element::P:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::BREAK:
          SHOW_LOCATION("Page Break");
          if (!page.line_break(fmt)) {
            page_locs_end_page(fmt);
            SHOW_LOCATION("Page Break");
            page.line_break(fmt);
          }
          current_offset++;
          break;
        case Element::B: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::BOLD;
            else if (style == Fonts::FaceStyle::ITALIC) style = Fonts::FaceStyle::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case Element::I:
        case Element::EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::ITALIC;
            else if (style == Fonts::FaceStyle::BOLD  ) style = Fonts::FaceStyle::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case Element::H1:
          fmt.font_size          = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H2:
          fmt.font_size          = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H3:
          fmt.font_size          = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H4:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H5:
          fmt.font_size          = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H6:
          fmt.font_size          = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
      }
    }

    xml_attribute attr = node.attribute("style");
    CSS::Properties * element_properties = nullptr;
    if (attr) {
      const char * buffer = attr.value();
      const char * end    = buffer + strlen(buffer);
      element_properties  = CSS::parse_properties(&buffer, end, buffer);
    }

    adjust_format(node, fmt, element_properties); // Adjust format from element attributes

    if (element_properties) {
      CSS::clear_properties(element_properties);
      element_properties = nullptr;
    }

    if (fmt.display == CSS::Display::BLOCK) {
      if (page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 3");
        if (!page.end_paragraph(fmt)) {
          page_locs_end_page(fmt);

          if (page.some_data_waiting()) {
            SHOW_LOCATION("End Paragraph 4");
            page.end_paragraph(fmt);
          }
        }
      }
      SHOW_LOCATION("New Paragraph 4");
      if (!page.new_paragraph(fmt)) {
        page_locs_end_page(fmt);
        SHOW_LOCATION("New Paragraph 5");
        page.new_paragraph(fmt);
      }
    } 

    // if ((fmt.display == CSS::Display::BLOCK) && page.some_data_waiting()) {
    //   SHOW_LOCATION("End Paragraph 1");
    //   if (!page.end_paragraph(fmt)) {
    //     page_locs_end_page(fmt);
    //     SHOW_LOCATION("End Paragraph 2");
    //     page.end_paragraph(fmt);
    //   }
    // }
    // if ((current_offset == start_of_page_offset) && start_of_paragraph) {
    //   SHOW_LOCATION("New Paragraph 1");
    //   if (!page.new_paragraph(fmt)) {
    //     page_locs_end_page(fmt);
    //     SHOW_LOCATION("New Paragraph 2");
    //     page.new_paragraph(fmt);
    //   }
    //   start_of_paragraph = false;
    // }
  }
  else {
    //This is a node inside a named node. It is contaning some text to show.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (show_images && !image_filename.empty()) {
    Page::Image image;
    if (get_image(image_filename, image)) {
      if (!page.add_image(image, fmt)) {
        page_locs_end_page(fmt);
        if (start_of_paragraph) {
          SHOW_LOCATION("New Paragraph 3");
          page.new_paragraph(fmt);
          start_of_paragraph = false;
        }
        page.add_image(image, fmt);
      }
      stbi_image_free((void *) image.bitmap);
    }
    current_offset++;
  }

  if (str) {
    SHOW_LOCATION("->");
    while (*str) {
      if (uint8_t(*str) <= ' ') {
        if (*str == ' ') {
          fmt.trim = !fmt.pre;
          if (!page.add_char(str, fmt)) {
            page_locs_end_page(fmt);
            if (start_of_paragraph) {
              SHOW_LOCATION("New Paragraph 6");
              page.new_paragraph(fmt, false);
            }
            else {
              SHOW_LOCATION("New Paragraph 7");
              page.new_paragraph(fmt, true);
            }
          }
        }
        else if (fmt.pre && (*str == '\n')) {
          page.line_break(fmt, 30);
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
            SHOW_LOCATION("New Paragraph 8");
            page.new_paragraph(fmt, false);
          }
          else {
            SHOW_LOCATION("New Paragraph 9");
            page.new_paragraph(fmt, true);
          }
          page.add_word(word.c_str(), fmt);
        }
        current_offset += count;
        start_of_paragraph = false;
      }
    } 
  }

  if (named_element) {

    xml_node sub = node.first_child();
    while (sub) {
      page_locs_recurse(sub, fmt);
      sub = sub.next_sibling();
    }

    if (fmt.display == CSS::Display::BLOCK) {
      if ((current_offset != start_of_page_offset) || page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 5");
        if (!page.end_paragraph(fmt)) {
          page_locs_end_page(fmt);
          if (page.some_data_waiting()) {
            SHOW_LOCATION("End Paragraph 6");
            page.end_paragraph(fmt);
          }
        }
      }
      start_of_paragraph = false;
    } 

    // In case that we are at the end of an html file and there remains
    // characters in the page pipeline, we call end_paragraph() to get them out on the page...
    if ((element_it != elements.end()) && (element_it->second == Element::BODY)) {
      SHOW_LOCATION("End Paragraph 7");
      if (!page.end_paragraph(fmt)) {
        page_locs_end_page(fmt);
        if (page.some_data_waiting()) {
          SHOW_LOCATION("End Paragraph 8");
          page.end_paragraph(fmt);
        }
      }
    }
  } 
  return true;
}

      // if (fmt.display == CSS::Display::BLOCK) {
      //   if (page.some_data_waiting()) {
      //     SHOW_LOCATION("End Paragraph 3");
      //     if (!page.end_paragraph(fmt)) {
      //       page_locs_end_page(fmt);

      //       if (page.some_data_waiting()) {
      //         SHOW_LOCATION("End Paragraph 4");
      //         page.end_paragraph(fmt);
      //       }
      //     }
      //   }
      //   SHOW_LOCATION("New Paragraph 4");
      //   if (!page.new_paragraph(fmt)) {
      //     page_locs_end_page(fmt);
      //     SHOW_LOCATION("New Paragraph 5");
      //     page.new_paragraph(fmt);
      //   }
      // } 

bool
BookViewer::build_page_locs()
{
  bool done   = false;
  TTF * font  = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);

  page.set_compute_mode(Page::ComputeMode::LOCATION);

  int8_t images_are_shown;
  config.get(Config::Ident::SHOW_IMAGES, &images_are_shown);
  show_images = images_are_shown == 1;
  
  page_locs.clear();

  if (epub.get_first_item()) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 1;
    }
    
    int8_t font_size;
    config.get(Config::Ident::FONT_SIZE, &font_size);

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
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    last_props        = nullptr;

    while (!done) {

      // Process for each file part of the document

      current_offset       = 0;
      start_of_page_offset = 0;
      xml_node node = epub.get_current_item().child("html");

      if (node && 
         (node = node.child("body"))) {

        page.start(fmt);

        if (!page_locs_recurse(node, fmt)) break;

        if (page.some_data_waiting()) page.end_paragraph(fmt);
      }
      else {
        break;
      }

      page_locs_end_page(fmt);

      done = !epub.get_next_item();
    }
  }

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  return done;
}

bool
BookViewer::build_page_locs(int16_t itemref_index)
{
  std::scoped_lock guard(mutex);

  TTF * font  = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);
  
  page.set_compute_mode(Page::ComputeMode::LOCATION);

  int8_t images_are_shown;
  config.get(Config::Ident::SHOW_IMAGES, &images_are_shown);
  show_images = images_are_shown == 1;

  bool done = false;

  if (epub.get_item_at_index(itemref_index)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 1;
    }
    
    int8_t font_size;
    config.get(Config::Ident::FONT_SIZE, &font_size);

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
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    last_props        = nullptr;

    while (!done) {

      current_offset       = 0;
      start_of_page_offset = 0;
      xml_node node = epub.get_current_item().child("html");

      if (node && 
         (node = node.child("body"))) {

        page.start(fmt);

        if (!page_locs_recurse(node, fmt)) {
          LOG_D("html parsing issue");
          break;
        }

        if (page.some_data_waiting()) page.end_paragraph(fmt);
      }
      else {
        LOG_D("No <body>");
        break;
      }

      page_locs_end_page(fmt);

      done = true;
    }
  }

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  return done;
}

int16_t
BookViewer::get_pixel_value(const CSS::Value & value, const Page::Format & fmt, int16_t ref)
{
  switch (value.value_type) {
    case CSS::ValueType::PX:
      return value.num;
    case CSS::ValueType::PT:
      return (value.num * Screen::RESOLUTION) / 72;
    case CSS::ValueType::EM:
      {
        TTF * f = fonts.get(fmt.font_index, fmt.font_size);
        return value.num * f->get_em_width();
      }
    case CSS::ValueType::PERCENT:
      return (value.num * ref) / 100;
    case CSS::ValueType::NOTYPE:
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
  int8_t normal_size;
  config.get(Config::Ident::FONT_SIZE, &normal_size);

  switch (value.value_type) {
    case CSS::ValueType::PX:
      //  pixels -> Screen HResolution per inch
      //    x    ->    72 pixels per inch
      return (value.num * 72) / Screen::RESOLUTION; // convert pixels in points
    case CSS::ValueType::PT:
      return value.num;  // Already in points
    case CSS::ValueType::EM: {
          // TTF * font = fonts.get(1, normal_size);
          // if (font != nullptr) {
          //   return (value.num * font->get_em_height()) * 72 / Screen::RESOLUTION;
          // }
          // else {
            return value.num * ref;
          // }
        }
    case CSS::ValueType::CM:
      return (value.num * 72) / 2.54;
    case CSS::ValueType::PERCENT:
      return (value.num * normal_size) / 100;
    case CSS::ValueType::NOTYPE:
      return normal_size * value.num;
    case CSS::ValueType::STR:
      LOG_D("get_point_value(): Str value: %s.", value.str.c_str());
      return 0;
      break;
    case CSS::ValueType::VH:
      return ((value.num * (fmt.screen_bottom - fmt.screen_top)) / 100) * 72 / Screen::RESOLUTION;
    case CSS::ValueType::VW:
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
    case CSS::ValueType::PX:
    case CSS::ValueType::PT:
    case CSS::ValueType::STR:
      return 1.0;  
    case CSS::ValueType::EM:
    case CSS::ValueType::NOTYPE:
      return value.num;
    case CSS::ValueType::PERCENT:
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

  suite = current_item_css->search(element_name, "");
  if (suite) adjust_format_from_suite(fmt, *suite);

  if (current_item_css) {
    if (attr) {
      std::stringstream ss(attr.value());
      std::string class_name;

      while (std::getline(ss, class_name, ' ')) {
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

  Fonts::FaceStyle font_weight = ((fmt.font_style == Fonts::FaceStyle::BOLD) || 
                                  (fmt.font_style == Fonts::FaceStyle::BOLD_ITALIC)) ? 
                                     Fonts::FaceStyle::BOLD : 
                                     Fonts::FaceStyle::NORMAL;
  Fonts::FaceStyle font_style = ((fmt.font_style == Fonts::FaceStyle::ITALIC) || 
                                 (fmt.font_style == Fonts::FaceStyle::BOLD_ITALIC)) ? 
                                     Fonts::FaceStyle::ITALIC : 
                                     Fonts::FaceStyle::NORMAL;
  
  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::FONT_STYLE))) {
    font_style = (Fonts::FaceStyle) vals->front()->choice.face_style;
  }
  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::FONT_WEIGHT))) {
    font_weight = (Fonts::FaceStyle) vals->front()->choice.face_style;
  }
  Fonts::FaceStyle new_style = fonts.adjust_font_style(fmt.font_style, font_style, font_weight);

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::FONT_FAMILY))) {
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

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::TEXT_ALIGN))) {
    fmt.align = (CSS::Align) vals->front()->choice.align;
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::TEXT_INDENT))) {
    fmt.indent = get_pixel_value(*(vals->front()), fmt, page.paint_width());
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::FONT_SIZE))) {
    fmt.font_size = get_point_value(*(vals->front()), fmt, fmt.font_size);
    if (fmt.font_size == 0) {
      LOG_E("adjust_format_from_suite: setting fmt.font_size to 0!!!");
    }
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::LINE_HEIGHT))) {
    fmt.line_height_factor = get_factor_value(*(vals->front()), fmt, fmt.line_height_factor);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::MARGIN))) {
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

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::DISPLAY))) {
    fmt.display = (CSS::Display) vals->front()->choice.display;
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::MARGIN_LEFT))) {
    fmt.margin_left = get_pixel_value(*(vals->front()), fmt, fmt.margin_left);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::MARGIN_RIGHT))) {
    fmt.margin_right = get_pixel_value(*(vals->front()), fmt, fmt.margin_right);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::MARGIN_TOP))) {
    fmt.margin_top = get_pixel_value(*(vals->front()), fmt, fmt.margin_top);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::MARGIN_BOTTOM))) {
    fmt.margin_bottom = get_pixel_value(*(vals->front()), fmt, fmt.margin_bottom);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::WIDTH))) {
    fmt.width = get_pixel_value(*(vals->front()), fmt, Screen::WIDTH - fmt.screen_left - fmt.screen_right);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::HEIGHT))) {
    fmt.height = get_pixel_value(*(vals->front()), fmt, Screen::HEIGHT - fmt.screen_top - fmt.screen_bottom);
  }

  if ((vals = CSS::get_values_from_suite(suite, CSS::PropertyId::TEXT_TRANSFORM))) {
    fmt.text_transform = (CSS::TextTransform) vals->front()->choice.text_transform;
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
      for (int i = 0; i < (image.dim.width * image.dim.height); i++) {
        if (image.bitmap[i] != 0) { all_zero = false; break; }
      }
      if (all_zero) {
        LOG_E("Bitmap is all zeroes...");
      }

      unsigned char * bitmap2 = (unsigned char *) allocate(image.dim.width * image.dim.height);
      if (bitmap2 == nullptr) {
        stbi_image_free((void *) image.bitmap);
        return false;
      }
      stbir_resize_uint8(image.bitmap, image.dim.width, image.dim.height, 0, 
                         bitmap2,      image.dim.width, image.dim.height, 0,
                         1);

      stbi_image_free((void *) image.bitmap);
      image.bitmap = bitmap2; 
    }
    return true;
  }

  return false;
}

bool
BookViewer::build_page_recurse(xml_node node, Page::Format fmt)
{
  if (node == nullptr) return false;
  if (page.is_full()) return true;

  const char * name;
  const char * str = nullptr;
  std::string image_filename;

  image_filename.clear();

  // If node->name() is not an empty string, this is the start of a named element

  Elements::iterator element_it = elements.end();

  bool named_element = *(name = node.name()) != 0;

  if (current_offset >= end_of_page_offset) return true;

  bool started = current_offset >= start_of_page_offset;

  if (started) page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (named_element) { 

    // LOG_D("Node name: %s", name);

    // Do it only if we are now in the current page content
    
    fmt.display = CSS::Display::INLINE;

    if ((element_it = elements.find(std::string(name))) != elements.end()) {

      LOG_D("==> %10s [%5d] %5d", name, current_offset, page.get_pos_y());

      switch (element_it->second) {
        case Element::BODY:
        case Element::SPAN:
        case Element::A:
          break;
      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;
      #else
        case Element::IMG:
          if (show_images) {
            if (started) { 
              xml_attribute attr = node.attribute("src");
              if (attr != nullptr) image_filename = attr.value();
              else current_offset++;
            }
            else current_offset++;
          }
          else {
            xml_attribute attr = node.attribute("alt");
            if (attr != nullptr) str = attr.value();
            else current_offset++;
          }
          break;
        case Element::IMAGE: 
          if (show_images) {
            if (started) {
              xml_attribute attr = node.attribute("xlink:href");
              if (attr != nullptr) image_filename = attr.value();
              else current_offset++;
            }
            else current_offset++;
          }
          break;
      #endif
        case Element::PRE:
          fmt.pre = start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::LI:
        case Element::DIV:
        case Element::BLOCKQUOTE:
        case Element::P:
          start_of_paragraph = true;
          // LOG_D("Para: %d %d", fmt.font_index, fmt.font_size);
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::BREAK:
          if (started) {
            SHOW_LOCATION("Page Break");
            if (!page.line_break(fmt)) return true;
          }
          current_offset++;
          break;
        case Element::B: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::BOLD;
            else if (style == Fonts::FaceStyle::ITALIC) style = Fonts::FaceStyle::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break; 
        case Element::I:
        case Element::EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::ITALIC;
            else if (style == Fonts::FaceStyle::BOLD  ) style = Fonts::FaceStyle::BOLD_ITALIC;
            reset_font_index(fmt, style);
          }
          break;
        case Element::H1:
          fmt.font_size          = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H2:
          fmt.font_size          = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H3:
          fmt.font_size          = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H4:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H5:
          fmt.font_size          = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
        case Element::H6:
          fmt.font_size          = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
      }
    }
    
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

    if (started && (fmt.display == CSS::Display::BLOCK)) {

      if (page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 1");
        if (!page.end_paragraph(fmt)) return true;
      }

      SHOW_LOCATION("New Paragraph 1");
      if (page.new_paragraph(fmt)) start_of_paragraph = false;
      else return true;
    }

    // }
    // else {
    //   if (started && (fmt.display == CSS::BLOCK)) {
    //     if (page.some_data_waiting()) {
    //       SHOW_LOCATION("End Paragraph 2");
    //       if (!page.end_paragraph(fmt)) return true;
    //     }
    //     SHOW_LOCATION("New Paragraph 2");
    //     if (page.new_paragraph(fmt)) {
    //       start_of_paragraph = false;
    //     }
    //     // else return true;
    //   }
    // }
  }
  else {
    // We look now at the node content and prepare the glyphs to be put on a page.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (current_offset >= end_of_page_offset) return true;

  if (show_images && !image_filename.empty()) {
    if (current_offset < start_of_page_offset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      current_offset++;
    }
    else {
      Page::Image image;
      if (get_image(image_filename, image)) {
        if (started && (current_offset < end_of_page_offset)) {
          if (!page.add_image(image, fmt)) {
            stbi_image_free((void *) image.bitmap);
            return true;
          }
        }
        stbi_image_free((void *) image.bitmap);
      }
      current_offset++;
    }
  }

  if (str) {
    int16_t size;
    
    if (current_offset + (size = strlen(str)) <= start_of_page_offset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      current_offset += size;
    }
    else {
      SHOW_LOCATION("->");

      while (*str) {
        // if (current_offset == start_of_page_offset) {
        //   SHOW_LOCATION("New Paragraph 3");
        //   page.new_paragraph(fmt, !start_of_paragraph);
        //   start_of_paragraph = false;
        // }
        if (uint8_t(*str) <= ' ') {
          if (current_offset >= start_of_page_offset) {
            page.set_compute_mode(Page::ComputeMode::DISPLAY);
            if (*str == ' ') {
              fmt.trim = !fmt.pre; // white spaces will be trimmed at the beginning and the end of a line
              if (!page.add_char(str, fmt)) break;
            }
            else if (fmt.pre && (*str == '\n')) {
              page.line_break(fmt, 30);              
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
            page.set_compute_mode(Page::ComputeMode::DISPLAY);
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

  if (named_element) {

    xml_node sub = node.first_child();
    while (sub) {
      if (page.is_full() || (current_offset >= end_of_page_offset)) return true;
      if (!build_page_recurse(sub, fmt)) {
        if (page.is_full() || (current_offset >= end_of_page_offset)) return true;
        return false;
      }
      sub = sub.next_sibling();
    }

    if (current_offset >= start_of_page_offset) {
      page.set_compute_mode(Page::ComputeMode::DISPLAY);
      if (fmt.display == CSS::Display::BLOCK) {
        if ((current_offset != start_of_page_offset) || page.some_data_waiting()) {
          SHOW_LOCATION("End Paragraph 3");
          page.end_paragraph(fmt);
        }
        start_of_paragraph = false;
      }

      // In case that we are at the end of an html file and there remains
      // characters in the page pipeline, to get them out on the page...
      if ((element_it != elements.end()) && (element_it->second == Element::BODY)) {
        SHOW_LOCATION("End Paragraph 4");
        page.end_paragraph(fmt);
      }
    }

  }
  return true;
}

void
BookViewer::build_page_at(const PageLocs::PageId & page_id)
{
  TTF * font = fonts.get(0, 10);
  page_bottom = font->get_line_height() + (font->get_line_height() >> 1);

  page.set_compute_mode(Page::ComputeMode::MOVE);

  int8_t images_are_shown;
  config.get(Config::Ident::SHOW_IMAGES, &images_are_shown);
  show_images = images_are_shown != 0;

  if (epub.get_item_at_index(page_id.itemref_index)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 1;
    }

    int8_t font_size;
    config.get(Config::Ident::FONT_SIZE, &font_size);

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
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    last_props        = nullptr;

    const PageLocs::PageInfo * page_info = page_locs.get_page_info(page_id);
    if (page_info == nullptr) return;

    current_offset       = 0;
    start_of_page_offset = page_id.offset;
    end_of_page_offset   = page_id.offset + page_info->size;

    xml_node node        = epub.get_current_item().child("html");

    SET_PAGE_TO_SHOW(current_page_nbr);

    if (node && 
        (node = node.child("body"))) {

      // if (current_page_nbr == 10) {
      //   LOG_D("Found page 10");
      // }

      page.start(fmt);

      if (build_page_recurse(node, fmt)) {

        if (page.some_data_waiting()) page.end_paragraph(fmt);

        //TTF * font = fonts.get(0, 7);

        int16_t page_nbr   = page_locs.page_nbr(page_id);
        int16_t page_count = page_locs.page_count();

        if ((page_nbr != -1) && (page_count != -1)) {

          fmt.line_height_factor = 1.0;
          fmt.font_index         =   1;
          fmt.font_size          =   9;
          fmt.font_style         = Fonts::FaceStyle::NORMAL;
          fmt.align              = CSS::Align::CENTER;

          std::ostringstream ostr;
          ostr << page_nbr + 1 << " / " << page_count;
          std::string str = ostr.str();

          page.put_str_at(str, Pos(-1, Screen::HEIGHT + font->get_descender_height() - 2), fmt);
        }

        #if EPUB_INKPLATE_BUILD
          BatteryViewer::show();
        #endif

        //if (current_page_nbr == 3) page.show_display_list(page.get_display_list(), "---- BEFORE ----");
        page.paint();
        //if (current_page_nbr == 3) page.show_display_list(page.get_display_list(), "---- AFTER ----");
      }
    }

    if (current_offset != end_of_page_offset) {
      LOG_E("Current page offset and end of page offset differ: %d vd %d", 
            current_offset, end_of_page_offset);
    }
  }
}

void
BookViewer::show_page(const PageLocs::PageId & page_id)
{
  std::scoped_lock guard(mutex);

  if ((current_page_id.itemref_index != page_id.itemref_index) ||
      (current_page_id.offset        != page_id.offset)) {

    current_page_id = page_id;
    
    if (page_locs.page_nbr(page_id) == 0) {
      const char * filename = epub.get_cover_filename();
      if (filename != nullptr) {
        // LOG_D("Cover filename: %s", filename);
        uint32_t size;
        unsigned char * data = (unsigned char *) epub.retrieve_file(filename, size);

        if ((data == NULL) || !page.show_cover(data, size)) {
          LOG_D("Unable to retrieve cover file: %s", filename);

          Page::Format fmt = {
            .line_height_factor = 1.0,
            .font_index         =   3,
            .font_size          =  14,
            .indent             =   0,
            .margin_left        =   0,
            .margin_right       =   0,
            .margin_top         =   0,
            .margin_bottom      =   0,
            .screen_left        =  10,
            .screen_right       =  10,
            .screen_top         = 100,
            .screen_bottom      =  30,
            .width              =   0,
            .height             =   0,
            .trim               = true,
            .pre                = false,
            .font_style         = Fonts::FaceStyle::ITALIC,
            .align              = CSS::Align::CENTER,
            .text_transform     = CSS::TextTransform::NONE,
            .display            = CSS::Display::INLINE
          };

          std::string title  = epub.get_title();
          std::string author = epub.get_author();

          page.start(fmt);

          page.new_paragraph(fmt, false);
          page.add_text(author, fmt);
          page.end_paragraph(fmt);

          fmt.font_index =   1;
          fmt.font_size  =  18;
          fmt.screen_top = 200;
          fmt.font_style = Fonts::FaceStyle::NORMAL;

          page.set_limits(fmt);
          page.new_paragraph(fmt, false);
          page.add_text(title, fmt);
          page.end_paragraph(fmt);

          page.paint();
        }
        
        if (data) free(data);
      }
      else {
        LOG_D("There doesn't seems to have any cover.");
      }
    }
    else {
      build_page_at(page_id);
    }
  }
}