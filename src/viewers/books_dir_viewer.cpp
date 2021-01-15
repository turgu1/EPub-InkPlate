// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOKS_DIR_VIEWER__ 1
#include "viewers/books_dir_viewer.hpp"

#include "models/fonts.hpp"
#include "models/config.hpp"
#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

void
BooksDirViewer::setup()
{
  books_per_page = (Screen::HEIGHT - FIRST_ENTRY_YPOS - 20 + 6) / (BooksDir::max_cover_height + 6);
}

void 
BooksDirViewer::show_page(int16_t page_nbr, int16_t hightlight_item_idx)
{
  current_page_nbr = page_nbr;
  current_item_idx = hightlight_item_idx;

  int16_t book_idx = page_nbr * books_per_page;
  int16_t last     = book_idx + books_per_page;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (last > books_dir.get_book_count()) last = books_dir.get_book_count();

  int16_t xpos = 20 + BooksDir::max_cover_width;
  int16_t ypos = FIRST_ENTRY_YPOS;

  Page::Format fmt = {
      .line_height_factor = 0.8,
      .font_index         = 1,
      .font_size          = TITLE_FONT_SIZE,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = xpos,
      .screen_right       = 10,
      .screen_top         = ypos,
      .screen_bottom      = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

  page.start(fmt);

  for (int item_idx = 0; book_idx < last; item_idx++, book_idx++) {

    int16_t top_pos = ypos;

    const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

    if (book == nullptr) break;
    
    Page::Image image = (Page::Image) {
      .bitmap = (uint8_t *) book->cover_bitmap,
      .dim    = Dim(book->cover_width, book->cover_height)
    };
    page.put_image(image, Pos(10, ypos));

    if (item_idx == current_item_idx) {
      page.put_highlight(Dim(Screen::WIDTH - (25 + BooksDir::max_cover_width), BooksDir::max_cover_height), 
                         Pos(xpos - 5, ypos));
    }

    fmt.font_index    = 1;
    fmt.font_size     = TITLE_FONT_SIZE;
    fmt.font_style    = Fonts::FaceStyle::NORMAL;
    fmt.screen_top    = ypos,
    fmt.screen_bottom = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_height + 6)),

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(book->title, fmt);
    page.end_paragraph(fmt);

    fmt.font_index = 3;
    fmt.font_size  = AUTHOR_FONT_SIZE;
    fmt.font_style = Fonts::FaceStyle::ITALIC;

    page.new_paragraph(fmt);
    page.add_text(book->author, fmt);
    page.end_paragraph(fmt);

    ypos = top_pos + BooksDir::max_cover_height + 6;
  }

  TTF * font = fonts.get(0);

  fmt.line_height_factor = 1.0;
  fmt.font_index         = 1;
  fmt.font_size          = PAGENBR_FONT_SIZE;
  fmt.font_style         = Fonts::FaceStyle::NORMAL;
  fmt.align              = CSS::Align::CENTER;
  fmt.screen_left        = 10;
  fmt.screen_right       = 10; 
  fmt.screen_top         = 10;
  fmt.screen_bottom      = 10;

  page.set_limits(fmt);

  std::ostringstream ostr;
  ostr << page_nbr + 1 << " / " << page_count();

  page.put_str_at(ostr.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(PAGENBR_FONT_SIZE) - 2), fmt);

  #if EPUB_INKPLATE_BUILD
    int8_t show_heap;
    config.get(Config::Ident::SHOW_HEAP, &show_heap);

    if (show_heap != 0) {
      ostr.str(std::string());
      ostr << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) 
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
BooksDirViewer::highlight(int16_t item_idx)
{
  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (current_item_idx != item_idx) {

    // Clear the highlighting of the current item

    int16_t book_idx = current_page_nbr * books_per_page + current_item_idx;

    int16_t xpos = 20 + BooksDir::max_cover_width;
    int16_t ypos = FIRST_ENTRY_YPOS + (current_item_idx * (BooksDir::max_cover_height + 6));

    const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

    if (book == nullptr) return;

    // TTF * font = fonts.get(1, 9);

    Page::Format fmt = {
      .line_height_factor = 0.8,
      .font_index         = 1,
      .font_size          = TITLE_FONT_SIZE,
      .indent             = 0,
      .margin_left        = 0,
      .margin_right       = 0,
      .margin_top         = 0,
      .margin_bottom      = 0,
      .screen_left        = xpos,
      .screen_right       = 10,
      .screen_top         = ypos,
      .screen_bottom      = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),
      .width              = 0,
      .height             = 0,
      .trim               = true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

    page.start(fmt);

    page.clear_highlight(
      Dim(Screen::WIDTH - (25 + BooksDir::max_cover_width), BooksDir::max_cover_height),
      Pos(xpos - 5, ypos));

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(book->title, fmt);
    page.end_paragraph(fmt);

    fmt.font_index = 3;
    fmt.font_size  = AUTHOR_FONT_SIZE;
    fmt.font_style = Fonts::FaceStyle::ITALIC;

    page.new_paragraph(fmt);
    page.add_text(book->author, fmt);
    page.end_paragraph(fmt);
    
    // Highlight the new current item

    current_item_idx = item_idx;

    book_idx = current_page_nbr * books_per_page + current_item_idx;
    ypos = FIRST_ENTRY_YPOS + (current_item_idx * (BooksDir::max_cover_height + 6));

    book = books_dir.get_book_data(book_idx);

    if (book == nullptr) return;
    
    page.put_highlight(
      Dim(Screen::WIDTH - (25 + BooksDir::max_cover_width), BooksDir::max_cover_height),
      Pos(xpos - 5, ypos));


    fmt.font_index    = 1,
    fmt.font_size     = TITLE_FONT_SIZE,
    fmt.font_style    = Fonts::FaceStyle::NORMAL,
    fmt.screen_top    = ypos,
    fmt.screen_bottom = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(book->title, fmt);
    page.end_paragraph(fmt);

    fmt.font_index = 3,
    fmt.font_size  = AUTHOR_FONT_SIZE,
    fmt.font_style = Fonts::FaceStyle::ITALIC,

    page.new_paragraph(fmt);
    page.add_text(book->author, fmt);
    page.end_paragraph(fmt);

    #if EPUB_INKPLATE_BUILD
      BatteryViewer::show();
    #endif

    page.paint(false);
  }
}
