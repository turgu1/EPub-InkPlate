// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/html_interpreter.hpp"

#include "models/epub.hpp"
#include "models/config.hpp"
#include "models/toc.hpp"

MemoryPool<Page::Format> HTMLInterpreter::fmt_pool;

// This method process a single xml node and recurse for the associated children.
// The method calls the page_end() method when it reachs the end of the page as 
// defined by the end_offset variable, or when the page class indicated that the page
// is full (through the page_full() method or false value returned by some of its methods).
//
// This method is used both to compute the pages location, size, and offsets, but also
// to display a single page on screen.
//
// Before this method can be called, the set_limits() method must be called to set
// proper control variables that will impact the method results.
//
// For pages location computation the limits must be 0 (beginning of the html file) and
// 999999 (infinite end of the html file).
// 
// For single page output on screen, the limits must be set to the offset of the first
// location in file to show on screen, and to the last offset for that page.
//
// The offsets are not character indexes in the html file, but internal computation of the location,
// depending on the kind of tag encountered and the strings of characters present in each
// block to be displayed (paragraphs, headers, etc.)

bool
HTMLInterpreter::build_pages_recurse(xml_node       node, 
                                     Page::Format & fmt, 
                                     DOM::Node    * dom_node,
                                     int16_t        level)
{
  if (level > max_level) max_level = level;

  if (level > 50) {
    LOG_E("Too many nested tag levels (max: 50).");
    return true;
  }

  if (node == nullptr) return false;
  if (at_end()) return true;
    
  check_if_started();

  std::string  image_filename;
  const char * name;
  const char * str              = nullptr;
  DOM::Node  * dom_current_node = dom_node;
  DOM::Tags::iterator tag_it    = DOM::tags.end();

  // xml node without a tag name are internal data to be processed as string of chars
  bool named_element = *(name = node.name()) != 0;

  if (named_element) {

    xml_attribute attr;

    if ((page.get_compute_mode() == Page::ComputeMode::LOCATION) &&
        toc.there_is_some_ids() &&
        (attr = node.attribute("id"))) {
      std::string id = attr.value();
      toc.set(id, current_offset);
    }
    if (node.attribute("hidden")) return true;

    // LOG_D("Node name: %s", name);
    // Do it only if we are now in the current page content
    
    fmt.display       = CSS::Display::INLINE;
    fmt.margin_bottom = 0;
    fmt.margin_left   = 0;
    fmt.margin_right  = 0;
    fmt.margin_top    = 0;

    if ((tag_it = DOM::tags.find(name)) != DOM::tags.end()) {

      //LOG_D("==> %10s [%5d] %5d", name, current_offset, page.get_pos_y());

      if (tag_it->second != DOM::Tag::BODY) {
        dom_current_node = dom_node->add_child(tag_it->second);
      }
      else {
        dom_current_node = dom.body;
      }
      if ((attr = node.attribute("id"   ))) dom_current_node->add_id(attr.value());
      if ((attr = node.attribute("class"))) dom_current_node->add_classes(attr.value());

      switch (tag_it->second) {
        case DOM::Tag::A:
        case DOM::Tag::BODY:
        case DOM::Tag::SPAN:
          break;

      #if NO_IMAGE
        case IMG:
        case IMAGE:
          break;

      #else
        case DOM::Tag::IMG:
          if (show_images) {
            if (started) { 
              if ((attr = node.attribute("src"))) image_filename = attr.value();
              else current_offset++;
            }
            else current_offset++;
          }
          else {
            if ((attr = node.attribute("alt"))) str = attr.value();
            else current_offset++;
          }
          break;

        case DOM::Tag::IMAGE: 
          if (show_images) {
            if (started) {
              if ((attr = node.attribute("xlink:href"))) image_filename = attr.value();
              else current_offset++;
            }
            else current_offset++;
          }
          break;

      #endif
        case DOM::Tag::PRE:
          fmt.pre     = true;
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::BLOCKQUOTE:
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::LI:
        case DOM::Tag::DIV:
        case DOM::Tag::P:
          fmt.display = CSS::Display::BLOCK;
          break;

        case DOM::Tag::BREAK:
          if (started) {
            show_state("Line Break", fmt);
            if (!page.line_break(fmt)) {
              //At the end of the page
              if (!page_end(fmt)) return false;
              if (at_end()) return true;
              page.line_break(fmt);
            }
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
          //fmt.line_height_factor = 1.25 * fmt.line_height_factor;
          fmt.display            = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H2:
          fmt.font_size          = 1.1 * fmt.font_size;
          //fmt.line_height_factor = 1.1 * fmt.line_height_factor;
          fmt.display            = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H3:
          fmt.font_size          = 1.05 * fmt.font_size;
          //fmt.line_height_factor = 1.05 * fmt.line_height_factor;
          fmt.display            = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H4:
          fmt.display            = CSS::Display::BLOCK;
          break;

        case DOM::Tag::H5:
          fmt.font_size          = 0.8 * fmt.font_size;
          //fmt.line_height_factor = 0.8 * fmt.line_height_factor;
          fmt.display            = CSS::Display::BLOCK;
          break;
          
        case DOM::Tag::H6:
          fmt.font_size          = 0.7 * fmt.font_size;
          //fmt.line_height_factor = 0.7 * fmt.line_height_factor;
          fmt.display            = CSS::Display::BLOCK;
          break;

        case DOM::Tag::SUB:
          fmt.font_size          = 0.75 * fmt.font_size;
          fmt.vertical_align     = 5;
          break;

        case DOM::Tag::SUP:
          fmt.font_size          = 0.75 * fmt.font_size;
          fmt.vertical_align     = -5;
          break;

        case DOM::Tag::NONE:
        case DOM::Tag::ANY:
        case DOM::Tag::FONT_FACE:
        case DOM::Tag::PAGE:
          return true;

      }
      
      // if a 'style' attribute is present, parse it's content as it will be used
      // in the processing of the tag's format styling

      CSS *  element_css = nullptr;
      if ((attr = node.attribute("style"))) {
        const char * buffer = attr.value();
        element_css         = new CSS("ELEMENT", tag_it->second, buffer, strlen(buffer), 99);
      }

      // Adjust the tag's format styling (the fmt struct) using both the current
      // DOM, the overall item css, and the element css data.
      page.adjust_format(dom_current_node, fmt, element_css, item_info.css); // Adjust format from element attributes
      
      if (started) show_state(name, fmt, dom_current_node, element_css); // For debugging
      if (element_css != nullptr) delete element_css;  // Free the tag's specific css data
    }

    if (fmt.display == CSS::Display::NONE) return true;
    if (tag_it->second == DOM::Tag::BODY) {
      if (epub.get_book_format_params()->use_fonts_in_book == 0) {
        fmt.font_size = epub.get_book_format_params()->font_size;
        //fmt.font_index = ;
      }
    }
    
    if (started && (fmt.display == CSS::Display::BLOCK)) {

      int8_t iter = 5; // Allow for a paragraph taking at most 5 pages
      while ((iter-- > 0) && page.some_data_waiting()) {
        show_state("==> End Paragraph 1 <==", fmt);
        if (!page.end_paragraph(fmt)) {
          if (!page_end(fmt)) return false;
          if (at_end()) return true;
          page.end_paragraph(fmt);
        }
      }

      show_state("==> New Paragraph 1 <==", fmt);
      if (!page.new_paragraph(fmt)) {
        if (!page_end(fmt)) return false;
        if (at_end()) return true;
        page.new_paragraph(fmt);
      }
    }
  }
  else {
    // We look now at the node content and prepare the glyphs to be put on a page.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (at_end()) return true;

  if (show_images && !image_filename.empty()) {
    if (current_offset < start_offset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      current_offset++;
    }
    else {
      std::string fname = item_info.file_path;
      fname.append(image_filename);

      if (started && (current_offset < end_offset)) {

        Image * img = epub.get_image(fname, page.get_compute_mode() == Page::ComputeMode::DISPLAY);
        if (img != nullptr) {
          if (!page.add_image(*img, fmt /*, beginning_of_page */)) {
            if (page.is_full() && !page_end(fmt)) { delete img; return false; };
            if (at_end()) { delete img; return true; };
            page.add_image(*img, fmt /*, beginning_of_page */);
            if (page.is_full() && !page_end(fmt)) { delete img; return false; };
            if (at_end()) { delete img; return true; };
          }
          show_state("After IMG", fmt);
          delete img;
        }
        else {
          str = "[An image is not compatible or not found]";
        }
      }
      current_offset++;
    }
  }

  if (named_element) { // The element possesses a tag
    // Here we recurse on each child of the currernt tag.
    current_offset++;
    xml_node sub = node.first_child();
    while (sub != nullptr) {
      if (page.is_full() && !page_end(fmt)) return false;
      if (at_end()) break;
      Page::Format * new_fmt = duplicate_fmt(fmt);
      if (!build_pages_recurse(sub, *new_fmt, dom_current_node, level + 1)) {
        release_fmt(new_fmt);
        if (page.is_full() && !page_end(fmt)) return false;
        if (at_end()) break;
        return false;
      }
      release_fmt(new_fmt);
      sub = sub.next_sibling();
    }

    // The sub-nodes have been processed. Complete the block if not Inline and check
    // for remaining data if it's the head of the hierarchy ('body' tag)
    if (check_if_started()) {
      if (fmt.display == CSS::Display::BLOCK) {
        if ((current_offset != start_offset) || page.some_data_waiting()) {
          show_state("==> End Paragraph 2 <==", fmt);
          page.end_paragraph(fmt);
          show_state("==> After End Paragraph 2 <==", fmt);
        }
      }

      // In case that we are at the end of an html file and there remains
      // characters in the page pipeline, to get them out on the page...
      if ((tag_it != DOM::tags.end()) && (tag_it->second == DOM::Tag::BODY)) {
        int8_t iter = 5; // limit of 5 pages for a single paragraph...
        // Loop until the complete paragraph has been processed
        while ((iter-- > 0) && page.some_data_waiting()) {
          show_state("==> End Paragraph 3 <==", fmt);
          if (!page.end_paragraph(fmt)) {
            // The paragraph may not have been completly rendered. This means
            // that we found the end of a page
            if (!page_end(fmt)) return false;
            page.end_paragraph(fmt);
          }
        }
      }
    }
  }

  if (str != nullptr) {
    int16_t size;

    if (current_offset + (size = strlen(str)) <= start_offset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      current_offset += size;
    }
    else {
      show_state("==> STR <==", fmt);

      bool to_be_started = !check_if_started();

      while (*str) {

        if (uint8_t(*str) <= ' ') {
          if (check_if_started()) {
            if (to_be_started) {
              to_be_started = false;
              page.new_paragraph(fmt, true);
            }       
            if ((*str == ' ') || (!fmt.pre && (*str == '\n'))) {
              if ((fmt.trim = !fmt.pre)) { // white spaces will be trimmed at the beginning and the end of a line
                // get rid of multiple spaces in a row (if not pre)
                do { 
                  str++; 
                  current_offset++; 
                } while ((*str == ' ') || (*str == '\n'));
                str--;
                current_offset--;
              }
              if (!page.add_char(" ", fmt)) {
                // Unable to add the character... the page must be full
                if (!page_end(fmt)) return false;
                if (at_end()) {
                  page.break_paragraph(fmt);
                  return true;
                }
                // We are at the beginning of a new page. We skip white spaces
                show_state("==> New Paragraph 2 <==", fmt);
                page.new_paragraph(fmt, true);
              }
            }
            else if (fmt.pre && (*str == '\n')) {
              if (!page.line_break(fmt, 30)) {
                if (!page_end(fmt)) return false;
                if (at_end()) return true;
              }              
            }
          }
          str++;
          current_offset++; // Not an UTF-8, so it's ok...
        }
        else {
          const char * w = str;
          int16_t count = 0;
          while (uint8_t(*str) > ' ') { str++; count++; }
          if (check_if_started()) {
            if (to_be_started) {
              to_be_started = false;
              page.new_paragraph(fmt, true);
            }       
            std::string word;
            word.assign(w, count);

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              static bool first = true;
              if (first) {
                LOG_D("before page.add_word()");
                ESP::show_heaps_info();
              }
            #endif

            if (!page.add_word(word.c_str(), fmt)) {
              if (!page_end(fmt)) return false;
              if (at_end()) {
                page.break_paragraph(fmt);
                return true;
              }
              show_state("==> New Paragraph 3 <==", fmt);
              page.new_paragraph(fmt, true);
              show_state("==> After New Paragraph 3 <==", fmt);
              page.add_word(word.c_str(), fmt);
            }
          }
          current_offset += count;
        }
        // ToDo: Not sure the following test is required...
        // if (page.is_full()) {
        //   if (!page_end(fmt)) return false;
        // }
        // ToDo: This may have to be moved down after the while loop..
        if (at_end()) {
          if (*str) {
            show_state("==> Before Break Paragraph 1 <==", fmt);
            page.break_paragraph(fmt);
            show_state("==> After Break Paragraph 1 <==", fmt);
          }
          break;
        }
      }
    }
  }

  return true;
}
