// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_VIEWER__ 1
#include "controllers/book_controller.hpp"
#include "viewers/book_viewer.hpp"

#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/html_interpreter.hpp"
#include "viewers/screen_bottom.hpp"
#include "models/image_factory.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"
#include "alloc.hpp"

#include <iomanip>
#include <cstring>
#include <iostream>
#include <sstream>

#include "logging.hpp"

using namespace pugi;

class BookViewerInterp : public HTMLInterpreter 
{
  public:
    BookViewerInterp(Page & the_page, DOM & the_dom, Page::ComputeMode the_comp_mode, const EPub::ItemInfo & the_item) : 
      HTMLInterpreter(the_page, the_dom, the_comp_mode, the_item) {}
   ~BookViewerInterp() {}
  protected:
    bool page_end(const Page::Format & fmt) { 
      LOG_D("---- PAGE END ----");
      return true; 
    }
};

void
BookViewer::build_page_at(const PageLocs::PageId & page_id)
{
  LOG_D("build_page_at()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  Font * font = fonts.get(0);
  page_bottom = font->get_line_height(10) + (font->get_line_height(10) >> 1);

  //page.set_compute_mode(Page::ComputeMode::MOVE);

  //show_images = epub.get_book_format_params()->show_images != 0;

  if (epub.get_item_at_index(page_id.itemref_index)) {

    int16_t idx;

    if ((idx = fonts.get_index("Fontbase", Fonts::FaceStyle::NORMAL)) == -1) {
      idx = 3;
    }

    int8_t font_size = epub.get_book_format_params()->font_size;

    int8_t show_title;
    config.get(Config::Ident::SHOW_TITLE, &show_title);

    int16_t page_top = show_title != 0 ? 40 : 10;

    Page::Format fmt = {
      .line_height_factor = 0.95,
      .font_index         = idx,
      .font_size          = font_size,
      .indent             =   0,
      .margin_left        =   0,
      .margin_right       =   0,
      .margin_top         =   0,
      .margin_bottom      =   0,
      .screen_left        =  10,
      .screen_right       =  10,
      .screen_top         = page_top,
      .screen_bottom      = page_bottom,
      .width              =   0,
      .height             =   0,
      .vertical_align     =   0,
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

    // current_offset       = 0;
    // start_of_page_offset = page_id.offset;
    // end_of_page_offset   = page_id.offset + page_info->size;

    DOM              * dom    = new DOM;
    BookViewerInterp * interp = new BookViewerInterp(page, * dom, 
                                                     Page::ComputeMode::DISPLAY, 
                                                     epub.get_current_item_info());
    interp->set_limits(page_id.offset, 
                       page_id.offset + page_info->size,
                       epub.get_book_format_params()->show_images != 0);

    #if DEBUGGING_AID
      interp->set_pages_to_show_state(PAGE_FROM, PAGE_TO);
      interp->check_page_to_show(page_locs.get_page_nbr(page_id));
    #endif

    xml_node node;

    if ((node = epub.get_current_item().child("html").child("body"))) {

      page.start(fmt);

      #if EPUB_INKPLATE_BUILD
        esp_task_wdt_reset();
      #endif
      
      Page::Format * new_fmt = interp->duplicate_fmt(fmt);

      if (interp->build_pages_recurse(node, *new_fmt, dom->body, 1)) {

        if (page.some_data_waiting()) page.end_paragraph(fmt);

        //TTF * font = fonts.get(0, 7);

        int16_t page_nbr   = page_locs.get_page_nbr(page_id);
        int16_t page_count = page_locs.get_page_count();

        fmt.line_height_factor = 1.0;
        fmt.font_index         =   2;
        fmt.font_size          =   8;
        fmt.font_style         = Fonts::FaceStyle::ITALIC;
        fmt.align              = CSS::Align::CENTER;
        
        std::ostringstream ostr;

        if (show_title != 0) {
          const char * t = epub.get_title();
          if (strlen(t) > 50) {
            // Only the first 50 characters of the title will be shown
            char str[55];
            strncpy(str, t, 50);
            str[50] = 0;
            strcat(str, "...");
            ostr << str;
          }
          else {
            ostr << t;
          } 
          page.put_str_at(ostr.str(), Pos(Page::HORIZONTAL_CENTER, 25), fmt);
        }

        ScreenBottom::show(page_nbr, page_count);

        page.paint();
      }
      interp->show_stat();
      interp->release_fmt(new_fmt);
    }

    interp->check_for_completion();

    delete dom;
    dom = nullptr;

    delete interp;
    interp = nullptr;
  }
  LOG_D("end of build_page_at()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
}

void
BookViewer::show_fake_cover()
{
  Page::Format fmt = {
    .line_height_factor = 1.0,
    .font_index         =   2,
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
    .vertical_align     =   0,
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
      std::string fname = epub.get_cover_filename();
      if (!fname.empty()) {
        // LOG_D("Cover filename: %s", fname);
        Image * img = epub.get_image(fname, true);

        if (img != nullptr) {
          page.show_cover(*img);
          delete img;
        }
        else {
          LOG_D("Unable to retrieve cover file: %s", fname.c_str());
          show_fake_cover();
        }
      }
      else {
        LOG_D("It seems there is no cover.");
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
