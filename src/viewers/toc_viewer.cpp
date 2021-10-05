// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __TOC_VIEWER__ 1
#include "viewers/toc_viewer.hpp"

#include "models/fonts.hpp"
#include "models/config.hpp"
#include "models/toc.hpp"
#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

void
TocViewer::setup()
{
  entries_per_page = (Screen::HEIGHT - FIRST_ENTRY_YPOS - 20) / ENTRY_HEIGHT;
  page_count       = (toc.get_entry_count() + entries_per_page - 1) / entries_per_page;

  current_page_nbr    = -1;
  current_screen_idx  = -1;
  current_entry_idx   = -1;

  LOG_D("TOC entry count: %d", toc.get_entry_count());
}

void 
TocViewer::show_page(int16_t page_nbr, int16_t hightlight_screen_idx)
{
  current_page_nbr   = page_nbr;
  current_screen_idx = hightlight_screen_idx;

  int16_t entry_idx = page_nbr  * entries_per_page; // entry idx in the current page
  int16_t last_idx  = entry_idx + entries_per_page; // last entry idx in the current page

  if (last_idx > toc.get_entry_count()) last_idx = toc.get_entry_count();

  int16_t xpos = 20;
  int16_t ypos = TITLE_YPOS;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  Page::Format fmt = {
      .line_height_factor =   0.8,
      .font_index         = TITLE_FONT,
      .font_size          = TITLE_FONT_SIZE,
      .indent             =     0,
      .margin_left        =     0,
      .margin_right       =     0,
      .margin_top         =     0,
      .margin_bottom      =     0,
      .screen_left        =  xpos,
      .screen_right       =    10,
      .screen_top         =  ypos,
      .screen_bottom      = (int16_t)(Screen::HEIGHT - (ypos + MAX_TITLE_SIZE + 20)),
      .width              =     0,
      .height             =     0,
      .vertical_align     =     0,
      .trim               =  true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::BOLD,
      .align              = CSS::Align::CENTER,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

  page.start(fmt);

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  page.add_text(epub.get_title(), fmt);
  page.end_paragraph(fmt);

  ypos = FIRST_ENTRY_YPOS;

  fmt.font_index    = ENTRY_FONT;
  fmt.font_size     = ENTRY_FONT_SIZE;
  fmt.font_style    = Fonts::FaceStyle::NORMAL;
  fmt.align         = CSS::Align::LEFT;

  for (int16_t screen_idx = 0; entry_idx < last_idx; screen_idx++, entry_idx++) {

    const TOC::EntryRecord & entry = toc.get_entry(entry_idx);

    #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
      if (screen_idx == current_screen_idx) {
        page.put_highlight(Dim(Screen::WIDTH - 30, ENTRY_HEIGHT + 5), 
                           Pos(15, ypos));
      }
    #endif

    fmt.screen_left   = 20 + (entry.level * 20);
    fmt.screen_top    = ypos,
    fmt.screen_bottom = (int16_t)(Screen::HEIGHT - (ypos + ENTRY_HEIGHT)),

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(entry.label, fmt);
    page.end_paragraph(fmt);

    ypos += ENTRY_HEIGHT;
  }

  Font * font = fonts.get(0);

  fmt.line_height_factor = 1.0;
  fmt.font_index         = PAGENBR_FONT;
  fmt.font_size          = PAGENBR_FONT_SIZE;
  fmt.font_style         = Fonts::FaceStyle::NORMAL;
  fmt.align              = CSS::Align::CENTER;
  fmt.screen_left        = 10;
  fmt.screen_right       = 10; 
  fmt.screen_top         = 10;
  fmt.screen_bottom      = 10;

  page.set_limits(fmt);

  std::ostringstream ostr;
  ostr << page_nbr + 1 << " / " << page_count;

  page.put_str_at(ostr.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(PAGENBR_FONT_SIZE) - 2), fmt);

  #if EPUB_INKPLATE_BUILD
    int8_t show_heap;
    config.get(Config::Ident::SHOW_HEAP, &show_heap);

    if (show_heap != 0) {
      ostr.str(std::string());
      ostr << uxTaskGetStackHighWaterMark(nullptr)
           << " / "
           << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) 
           << " / " 
           << heap_caps_get_free_size(MALLOC_CAP_8BIT);
      fmt.align = CSS::Align::RIGHT;
      page.put_str_at(ostr.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(PAGENBR_FONT_SIZE) - 2), fmt);
    }

    BatteryViewer::show();
  #endif

  page.paint();
}

void 
TocViewer::highlight(int16_t screen_idx)
{
  #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (current_screen_idx != screen_idx) {

    // Clear the highlighting of the current item

    int16_t entry_idx = current_page_nbr * entries_per_page + current_screen_idx;

    const TOC::EntryRecord & entry = toc.get_entry(entry_idx);

    int16_t xpos = 20 + (entry.level * 20);
    int16_t ypos = FIRST_ENTRY_YPOS + (current_screen_idx * ENTRY_HEIGHT);

    Page::Format fmt = {
      .line_height_factor = 0.8,
      .font_index         = ENTRY_FONT,
      .font_size          = ENTRY_FONT_SIZE,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = xpos,
      .screen_right       = 10,
      .screen_top         = ypos,
      .screen_bottom      = (int16_t)(Screen::HEIGHT - (ypos + ENTRY_HEIGHT + 20)),
      .width              = 0,
      .height             = 0,
      .vertical_align     = 0,
      .trim               = true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    page.start(fmt);

    page.clear_highlight(
      Dim(Screen::WIDTH - 30, ENTRY_HEIGHT + 5),
      Pos(15, ypos));

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(entry.label, fmt);
    page.end_paragraph(fmt);
    
    // Highlight the new current entry

    current_screen_idx = screen_idx;

    entry_idx = current_page_nbr * entries_per_page + current_screen_idx;
    ypos      = FIRST_ENTRY_YPOS + (current_screen_idx * ENTRY_HEIGHT);

    const TOC::EntryRecord & entry2 = toc.get_entry(entry_idx);

    page.put_highlight(
      Dim(Screen::WIDTH - 30, ENTRY_HEIGHT + 5),
      Pos(15, ypos));

    fmt.screen_left = 20 + (entry2.level * 20);
    fmt.screen_top  = ypos;

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(entry2.label, fmt);
    page.end_paragraph(fmt);

    #if EPUB_INKPLATE_BUILD
      BatteryViewer::show();
    #endif

    page.paint(false);
  }
  #endif
}

int16_t
TocViewer::show_page_and_highlight(int16_t entry_idx)
{
  int16_t page_nbr   = entry_idx / entries_per_page;
  int16_t screen_idx = entry_idx % entries_per_page;

  if (current_page_nbr != page_nbr) {
    show_page(page_nbr, screen_idx);
  }
  else {
    if (screen_idx != current_screen_idx) highlight(screen_idx);
  }

  current_entry_idx = entry_idx;
  return current_entry_idx;
}

void
TocViewer::highlight_entry(int16_t entry_idx)
{
  highlight(entry_idx % entries_per_page);  
  current_entry_idx = entry_idx;
}

int16_t
TocViewer::next_page()
{
  return next_column();
}

int16_t
TocViewer::prev_page()
{
  return prev_column();
}

int16_t
TocViewer::next_item()
{
  int16_t entry_idx = current_entry_idx + 1;
  if (entry_idx >= toc.get_entry_count()) {
    entry_idx = toc.get_entry_count() - 1;
  }
  return show_page_and_highlight(entry_idx);
}

int16_t
TocViewer::prev_item()
{
  int16_t entry_idx = current_entry_idx - 1;
  if (entry_idx < 0) entry_idx = 0;
  return show_page_and_highlight(entry_idx);
}

int16_t
TocViewer::next_column()
{
  int16_t entry_idx = current_entry_idx + entries_per_page;
  if (entry_idx >= toc.get_entry_count()) {
    entry_idx = toc.get_entry_count() - 1;
  }
  else {
    entry_idx = (entry_idx / entries_per_page) * entries_per_page;
  }
  return show_page_and_highlight(entry_idx);
}

int16_t
TocViewer::prev_column()
{
  int16_t entry_idx = current_entry_idx - entries_per_page;
  if (entry_idx < 0) entry_idx = 0;
  else entry_idx = (entry_idx / entries_per_page) * entries_per_page;
  return show_page_and_highlight(entry_idx);
}
