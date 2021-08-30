// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "models/dom.hpp"
#include "models/epub.hpp"
#include "viewers/page.hpp"
#include "pugixml.hpp"

using namespace pugi;

// The HTMLInterpreter class is used to process the content of a book file (called item),
// preparing it for display through the BookViewer class or page location computation through
// the PageLocs class.
//
// Tags that are understood are used to create a partial DOM structure that is then passed through the
// CSS match algorithm to transform the viewing parameters as gathered in a Page::Format struc.

class HTMLInterpreter
{
  protected:
    static constexpr char const * TAG = "HTMLInterpreter";

    Page &                 page;
    DOM  &                 dom;
    Page::ComputeMode      compute_mode;
    const EPub::ItemInfo & item_info;

    int32_t current_offset;   ///< Where we are in current item
    int32_t start_offset;
    int32_t end_offset;

    bool show_images;
    bool started;
    //bool beginning_of_page;

    bool show_the_state;
    int16_t from_page, to_page;

    // The page_end method is responsible of doing post-processing once
    // the end of a page has been detected (the page.is_full() method returns true or
    // the process reached the page_end offset). This is specific for each of the book_viewer
    // and the page location computation processes.
    virtual bool page_end(Page::Format & fmt) = 0;

  public:
    HTMLInterpreter(Page & the_page, DOM & the_dom, Page::ComputeMode the_comp_mode, const EPub::ItemInfo & the_item) 
      :           page(the_page), 
                   dom(the_dom),
          compute_mode(the_comp_mode), 
             item_info(the_item),
        show_the_state(false), 
             from_page(-1), 
               to_page(-1) {}

    virtual ~HTMLInterpreter() {}

    void set_limits(int32_t start, int32_t end, bool show_imgs) {
      started            = false;
      //beginning_of_page  = false;
      current_offset     = 0;
      start_offset       = start;
      end_offset         = end;
      show_images        = show_imgs;
      page.set_compute_mode(Page::ComputeMode::MOVE);
    }

    bool build_pages_recurse(xml_node node, Page::Format fmt, DOM::Node * dom_node);

    void check_for_completion() {
      if (current_offset != end_offset) {
        LOG_E("Current page offset and end of page offset differ: %d vs %d", 
              current_offset, end_offset);
      }
    }

    void show_state(const char * caption, 
                    Page::Format fmt, 
                    DOM::Node  * dom_current_node = nullptr, 
                    CSS        * element_css      = nullptr) {
      if (show_the_state) {
        std::cout << caption << " Offset:" << current_offset << " ";
        page.show_controls("  ");
        std::cout << "     ";
        page.show_fmt(fmt, "  ");
        // if (item_info.css != nullptr) item_info.css->show();
        // std::cout << "--> Element CSS:" << std::endl;
        // if (element_css != nullptr) element_css->show();
        // std::cout << "[end show]" << std::endl;
        CSS::RulesMap rules;

        // item_info.css->show();
        // std::cout << "======" << std::endl;
        // if (element_css != nullptr) element_css->show();
        // std::cout << "------" << std::endl;
        // dom_current_node->show(1);

        // if ((dom_current_node != nullptr) && (item_info.css != nullptr)) {
        //   item_info.css->match(dom_current_node, rules);
        //   if (!rules.empty()) {
            // std::cout << "--> Item CSS match:" << std::endl;
            // item_info.css->show(rules);
          // }
          // else {
            //std::cout << "--> NO Item CSS match..." << std::endl;
        //   }
        // }
        // else {
          //std::cout << "--> No Item CSS..." << std::endl;
        // }

        // if (element_css != nullptr) {
          // std::cout << "--> Element style:" << std::endl;
          // element_css->show();
        // }
        // else {
          //std::cout << "--> NO Element Style..." << std::endl;
        // }
        //std::cout << "[end show]" << std::endl;
      }
    }

    void set_pages_to_show_state(int16_t from, int16_t to) {
      from_page = from;
      to_page   = to;
    }

    void check_page_to_show(int16_t page) {
      show_the_state = (from_page <= page) && ( page <= to_page);
    }

    inline bool is_started() {
      if (!started) {
        if ((started = current_offset >= start_offset)) {
          page.set_compute_mode(compute_mode);
          page.clean();
          LOG_D("---- PAGE START ----");
        }
      }
      return started;
    }

    inline bool at_end() { return current_offset >= end_offset; }
};