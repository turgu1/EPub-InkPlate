// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "controllers/book_controller.hpp"
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

bool
BookViewer::build_page_recurse(xml_node node, Page::Format fmt, DOM::Node * dom_node)
{
  if (node == nullptr) return false;
  if (page.is_full()) return true;

  const char * name;
  const char * str = nullptr;
  std::string  image_filename;
  DOM::Node *  dom_current_node;

  image_filename.clear();

  // If node->name() is not an empty string, this is the start of a named element

  dom_current_node = dom_node;

  DOM::Tags::iterator tag_it = DOM::tags.end();

  bool named_element = *(name = node.name()) != 0;

  if (current_offset >= end_of_page_offset) return true;

  bool started = current_offset >= start_of_page_offset;

  if (started) page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (named_element) { 

    // LOG_D("Node name: %s", name);

    // Do it only if we are now in the current page content
    
    fmt.display = CSS::Display::INLINE;

    if ((tag_it = DOM::tags.find(std::string(name))) != DOM::tags.end()) {

      // LOG_D("==> %10s [%5d] %5d", name, current_offset, page.get_pos_y());

      if (tag_it->second != DOM::Tag::BODY) {
        dom_current_node = dom_node->add_child(tag_it->second);
      }
      else {
        dom_current_node = dom->body;
      }
      xml_attribute attr = node.attribute("id");
      if (attr != nullptr) dom_current_node->add_id(attr.value());
      attr = node.attribute("class");
      if (attr != nullptr) dom_current_node->add_classes(attr.value());


      switch (tag_it->second) {
        case DOM::Tag::BODY:
        case DOM::Tag::SPAN:
        case DOM::Tag::A:
          break;

      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;

      #else
        case DOM::Tag::IMG:
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

        case DOM::Tag::IMAGE: 
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
        case DOM::Tag::PRE:
          fmt.pre = start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::LI:
        case DOM::Tag::DIV:
        case DOM::Tag::BLOCKQUOTE:
        case DOM::Tag::P:
          start_of_paragraph = true;
          // LOG_D("Para: %d %d", fmt.font_index, fmt.font_size);
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::BREAK:
          if (started) {
            SHOW_LOCATION("Page Break");
            if (!page.line_break(fmt)) return true;
          }
          current_offset++;
          break;

        case DOM::Tag::B:
        case DOM::Tag::STRONG: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::BOLD;
            else if (style == Fonts::FaceStyle::ITALIC) style = Fonts::FaceStyle::BOLD_ITALIC;
            page.reset_font_index(fmt, style);
          }
          break; 

        case DOM::Tag::I:
        case DOM::Tag::EM: {
            Fonts::FaceStyle style = fmt.font_style;
            if      (style == Fonts::FaceStyle::NORMAL) style = Fonts::FaceStyle::ITALIC;
            else if (style == Fonts::FaceStyle::BOLD  ) style = Fonts::FaceStyle::BOLD_ITALIC;
            page.reset_font_index(fmt, style);
          }
          break;

        case DOM::Tag::H1:
          fmt.font_size          = 1.25 * fmt.font_size;
          fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H2:
          fmt.font_size          = 1.1 * fmt.font_size;
          fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H3:
          fmt.font_size          = 1.05 * fmt.font_size;
          fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H4:
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H5:
          fmt.font_size          = 0.8 * fmt.font_size;
          fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
          
        case DOM::Tag::H6:
          fmt.font_size          = 0.7 * fmt.font_size;
          fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          start_of_paragraph = true;
          fmt.display = CSS::Display::BLOCK;
          break;
      }
      attr = node.attribute("style");
      CSS *  element_css = nullptr;
      if (attr) {
        const char * buffer = attr.value();
        element_css         = new CSS(tag_it->second, buffer, strlen(buffer), 99);
      }

      page.adjust_format(dom_current_node, fmt, element_css, epub.get_current_item_css()); // Adjust format from element attributes
      if (element_css != nullptr) delete element_css;
    }
    
    // xml_attribute attr = node.attribute("style");
    // CSS::Properties * element_properties = nullptr;
    // if (attr) {
    //   const char * buffer = attr.value();
    //   const char * end = buffer + strlen(buffer);
    //   element_properties = CSS::parse_properties(&buffer, end, buffer);
    // }

    // page.adjust_format(node, fmt, element_properties, epub.get_current_item_css()); // Adjust format from element attributes

    // if (element_properties) {
    //   CSS::clear_properties(element_properties);
    //   element_properties = nullptr;
    // }

    if (started && (fmt.display == CSS::Display::BLOCK)) {

      if (page.some_data_waiting()) {
        SHOW_LOCATION("End Paragraph 1");
        if (!page.end_paragraph(fmt)) return true;
      }

      SHOW_LOCATION("New Paragraph 1");
      if (page.new_paragraph(fmt)) start_of_paragraph = false;
      else return true;
    }
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
      if (page.get_image(image_filename, image)) {
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
            if ((*str == ' ') || (!fmt.pre && (*str == '\n'))) {
              fmt.trim = !fmt.pre; // white spaces will be trimmed at the beginning and the end of a line
              if (!page.add_char(" ", fmt)) break;
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
      if (!build_page_recurse(sub, fmt, dom_current_node)) {
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
      if ((tag_it != DOM::tags.end()) && (tag_it->second == DOM::Tag::BODY)) {
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
  TTF * font  = fonts.get(0);
  page_bottom = font->get_line_height(10) + (font->get_line_height(10) >> 1);

  page.set_compute_mode(Page::ComputeMode::MOVE);

  show_images = epub.get_book_format_params()->show_images != 0;

  if (epub.get_item_at_index(page_id.itemref_index)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 1;
    }

    int8_t font_size = epub.get_book_format_params()->font_size;

    int8_t show_title;
    config.get(Config::Ident::SHOW_TITLE, &show_title);

    int16_t top = show_title != 0 ? 30 : 10;

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
      .screen_top         = top,
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

    mutex.unlock();
    std::this_thread::yield();
    const PageLocs::PageInfo * page_info = page_locs.get_page_info(page_id);
    mutex.lock();
    
    if (page_info == nullptr) return;

    current_offset       = 0;
    start_of_page_offset = page_id.offset;
    end_of_page_offset   = page_id.offset + page_info->size;

    xml_node node        = epub.get_current_item().child("html");
    dom                  = new DOM;

    SET_PAGE_TO_SHOW(current_page_nbr);

    if (node && 
        (node = node.child("body"))) {

      page.start(fmt);

      if (build_page_recurse(node, fmt, dom->body)) {

        if (page.some_data_waiting()) page.end_paragraph(fmt);

        //TTF * font = fonts.get(0, 7);

        int16_t page_nbr   = page_locs.get_page_nbr(page_id);
        int16_t page_count = page_locs.get_page_count();

        fmt.line_height_factor = 1.0;
        fmt.font_index         =   1;
        fmt.font_size          =   9;
        fmt.font_style         = Fonts::FaceStyle::NORMAL;
        fmt.align              = CSS::Align::CENTER;
        
        std::ostringstream ostr;

        if (show_title != 0) {
          ostr << epub.get_title();
          page.put_str_at(ostr.str(), Pos(-1, 25), fmt);
        }

        if ((page_nbr != -1) && (page_count != -1)) {
          ostr.str(std::string());
          ostr << page_nbr + 1 << " / " << page_count;
          page.put_str_at(ostr.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(9) - 2), fmt);
        }

        #if EPUB_INKPLATE_BUILD
          int8_t show_heap;
          config.get(Config::Ident::SHOW_HEAP, &show_heap);

          if (show_heap != 0) {     
            ostr.str(std::string());
            ostr << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) 
                 << " / " 
                 << heap_caps_get_free_size(MALLOC_CAP_8BIT);
            fmt.align = CSS::Align::RIGHT;
            page.put_str_at(ostr.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(9) - 2), fmt);
          }

          BatteryViewer::show();
        #endif

        page.paint();
      }
    }

    delete dom;
    dom = nullptr;

    if (current_offset != end_of_page_offset) {
      LOG_E("Current page offset and end of page offset differ: %d vd %d", 
            current_offset, end_of_page_offset);
    }
  }
}

void
BookViewer::show_fake_cover()
{
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

void
BookViewer::show_page(const PageLocs::PageId & page_id)
{
  std::scoped_lock guard(mutex);

  current_page_id = page_id;
    
//if (page_locs.get_page_nbr(page_id) == 0) {
  if ((page_id.itemref_index == 0) && 
      (page_id.offset        == 0)) {

    if (epub.get_book_format_params()->show_images != 0) {
      const char * filename = epub.get_cover_filename();
      if (filename != nullptr) {
        // LOG_D("Cover filename: %s", filename);
        uint32_t size;
        unsigned char * data = nullptr;
        if (!book_controller.is_cover_too_large()) {
          data = (unsigned char *) epub.retrieve_file(filename, size);
        }

        if ((data == nullptr) || !page.show_cover(data, size)) {
          LOG_D("Unable to retrieve cover file: %s", filename);
          show_fake_cover();
        }
        
        if (data) free(data);
      }
      else {
        LOG_D("There doesn't seems to have any cover.");
        build_page_at(page_id);
      }
    }
    else {
      show_fake_cover();
    }
  }
  else {
    build_page_at(page_id);
  }
}