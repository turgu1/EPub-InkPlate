// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MATRIX_BOOKS_DIR_VIEWER__ 1
#include "viewers/matrix_books_dir_viewer.hpp"

#include "models/fonts.hpp"
#include "models/config.hpp"
#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
  #include "models/nvs_mgr.hpp"
#endif

#include "screen.hpp"

#include <iomanip>

#if (INKPLATE_6PLUS || TOUCH_TRIAL)
  static const std::string TOUCH_AND_HOLD_STR = "Touch cover and hold for more info. Tap for action.";
#endif

void
MatrixBooksDirViewer::setup()
{
  TTF * font = fonts.get(TITLE_FONT);
  title_font_height = font->get_line_height(TITLE_FONT_SIZE);

  font = fonts.get(AUTHOR_FONT);
  author_font_height = font->get_line_height(AUTHOR_FONT_SIZE);

  font = fonts.get(PAGENBR_FONT);
  pagenbr_font_height = font->get_line_height(PAGENBR_FONT_SIZE);

  first_entry_ypos = (title_font_height << 1) + author_font_height + SPACE_BELOW_INFO + 10;

  line_count = (Screen::HEIGHT - first_entry_ypos - pagenbr_font_height - SPACE_ABOVE_PAGENBR + MIN_SPACE_BETWEEN_ENTRIES) / 
                (BooksDir::max_cover_height + MIN_SPACE_BETWEEN_ENTRIES);

  column_count = (Screen::WIDTH - 10 + MIN_SPACE_BETWEEN_ENTRIES) / (BooksDir::max_cover_width + MIN_SPACE_BETWEEN_ENTRIES);

  horiz_space_between_entries = (Screen::WIDTH - 10 - (BooksDir::max_cover_width * column_count)) / (column_count - 1);
  vert_space_between_entries  = (Screen::HEIGHT - first_entry_ypos - pagenbr_font_height - SPACE_ABOVE_PAGENBR - (BooksDir::max_cover_height * line_count)) / (line_count - 1);
  books_per_page              = line_count * column_count;
  page_count                  = (books_dir.get_book_count() + books_per_page - 1) / books_per_page;

  current_page_nbr = -1;
  current_book_idx = -1;
  current_item_idx = -1;
}

void 
MatrixBooksDirViewer::show_page(int16_t page_nbr, int16_t hightlight_item_idx)
{
  current_page_nbr = page_nbr;    // First page == 0
  current_item_idx = hightlight_item_idx;

  int16_t book_idx = page_nbr * books_per_page;
  int16_t last     = book_idx + books_per_page;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  if (last > books_dir.get_book_count()) last = books_dir.get_book_count();

  int16_t xpos = 5;
  int16_t ypos = first_entry_ypos;
  int16_t line_pos = 0;

  Page::Format fmt = {
      .line_height_factor =   0.8,
      .font_index         = TITLE_FONT,
      .font_size          = TITLE_FONT_SIZE,
      .indent             =     0,
      .margin_left        =     0,
      .margin_right       =     0,
      .margin_top         =     0,
      .margin_bottom      =     0,
      .screen_left        =    10,
      .screen_right       =    10,
      .screen_top         =    10,
      .screen_bottom      =   100,
      .width              =     0,
      .height             =     0,
      .vertical_align     =     0,
      .trim               =  true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::LEFT,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

  page.start(fmt);

  for (int item_idx = 0; book_idx < last; item_idx++, book_idx++) {

    const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

    if (book == nullptr) break;
     
    Image::ImageData image(Dim(book->cover_width, book->cover_height), (uint8_t *) book->cover_bitmap);
    page.put_image(image, Pos(xpos + ((BooksDir::MAX_COVER_WIDTH - book->cover_width) >> 1), ypos + ((BooksDir::MAX_COVER_HEIGHT - book->cover_height) >> 1)));

    #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
      if (item_idx == current_item_idx) {
        page.put_highlight(Dim(BooksDir::max_cover_width + 4, BooksDir::max_cover_height + 4), 
                           Pos(xpos - 2, ypos - 2));
        page.put_highlight(Dim(BooksDir::max_cover_width + 6, BooksDir::max_cover_height + 6), 
                           Pos(xpos - 3, ypos - 3));

        fmt.font_index    = TITLE_FONT;
        fmt.font_size     = TITLE_FONT_SIZE;
        fmt.font_style    = Fonts::FaceStyle::NORMAL;

        char title[MAX_TITLE_SIZE];
        title[MAX_TITLE_SIZE - 1] = 0;
        strncpy(title, book->title, MAX_TITLE_SIZE - 1);
        if (strlen(book->title) > (MAX_TITLE_SIZE - 1)) {
          strcpy(&title[MAX_TITLE_SIZE - 5], " ...");
        }

        page.set_limits(fmt);
        page.new_paragraph(fmt);
        #if EPUB_INKPLATE_BUILD
          if (nvs_mgr.id_exists(book->id)) page.add_text("[Reading] ", fmt);
        #endif
        page.add_text(title, fmt);
        page.end_paragraph(fmt);

        fmt.font_index = AUTHOR_FONT;
        fmt.font_size  = AUTHOR_FONT_SIZE;
        fmt.font_style = Fonts::FaceStyle::ITALIC;

        page.new_paragraph(fmt);
        page.add_text(book->author, fmt);
        page.end_paragraph(fmt);
      }
    #endif

    line_pos++;

    if (line_pos >= line_count) {
      xpos    += BooksDir::MAX_COVER_WIDTH + horiz_space_between_entries;
      ypos     = first_entry_ypos;
      line_pos = 0;
    }
    else {
      ypos += BooksDir::MAX_COVER_HEIGHT + vert_space_between_entries;
    }
  }

  #if (INKPLATE_6PLUS || TOUCH_TRIAL)
    fmt.screen_top = 10 + title_font_height;
    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(TOUCH_AND_HOLD_STR, fmt);
    page.end_paragraph(fmt);
  #endif

  page.put_highlight(Dim(Screen::WIDTH - 20, 3), Pos(10, first_entry_ypos - 8));

  TTF * font = fonts.get(0);

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
MatrixBooksDirViewer::highlight(int16_t item_idx)
{
  int16_t book_idx,
          column_idx, line_idx,
          xpos, ypos;

  const BooksDir::EBookRecord * book;

  Page::Format fmt = {
    .line_height_factor = 0.8,
    .font_index         = TITLE_FONT,
    .font_size          = TITLE_FONT_SIZE,
    .indent             = 0,
    .margin_left        = 0,
    .margin_right       = 0,
    .margin_top         = 0,
    .margin_bottom      = 0,
    .screen_left        = 10,
    .screen_right       = 10,
    .screen_top         = 10,
    .screen_bottom      = 100,
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

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  page.start(fmt);

  if ((current_item_idx != -1) && (current_item_idx != item_idx)) {

    // Clear the highlighting of the current item

    book_idx = current_page_nbr * books_per_page + current_item_idx;

    column_idx = current_item_idx / line_count;
    line_idx   = current_item_idx % line_count;

    xpos = 5 + ((BooksDir::max_cover_width + horiz_space_between_entries) * column_idx);
    ypos = first_entry_ypos + ((BooksDir::max_cover_height + vert_space_between_entries) * line_idx);

    book = books_dir.get_book_data(book_idx);

    if (book == nullptr) return;

    // TTF * font = fonts.get(5, 9);

    page.clear_highlight(Dim(BooksDir::max_cover_width + 4, BooksDir::max_cover_height + 4), 
                         Pos(xpos - 2, ypos - 2));
    page.clear_highlight(Dim(BooksDir::max_cover_width + 6, BooksDir::max_cover_height + 6), 
                         Pos(xpos - 3, ypos - 3));

    page.clear_region(Dim(Screen::WIDTH - 10, (title_font_height << 1) + author_font_height),
                      Pos(10, 10));
  }
    // Highlight the new current item

  current_item_idx = -1;

  book_idx = current_page_nbr * books_per_page + item_idx;

  book = books_dir.get_book_data(book_idx);

  if (book == nullptr) return;
  
  current_item_idx = item_idx;

  column_idx = current_item_idx / line_count;
  line_idx   = current_item_idx % line_count;

  xpos = 5 + ((BooksDir::max_cover_width + horiz_space_between_entries) * column_idx);
  ypos = first_entry_ypos + ((BooksDir::max_cover_height + vert_space_between_entries) * line_idx);

  page.put_highlight(Dim(BooksDir::max_cover_width + 4, BooksDir::max_cover_height + 4), 
                      Pos(xpos - 2, ypos - 2));
  page.put_highlight(Dim(BooksDir::max_cover_width + 6, BooksDir::max_cover_height + 6), 
                      Pos(xpos - 3, ypos - 3));

  page.clear_region(Dim(Screen::WIDTH - 10, (title_font_height << 1) + author_font_height),
                    Pos(10, 10));

  fmt.font_index    = TITLE_FONT;
  fmt.font_size     = TITLE_FONT_SIZE;
  fmt.font_style    = Fonts::FaceStyle::NORMAL;

  char title[MAX_TITLE_SIZE];
  title[MAX_TITLE_SIZE - 1] = 0;
  strncpy(title, book->title, MAX_TITLE_SIZE - 1);
  if (strlen(book->title) > (MAX_TITLE_SIZE - 1)) {
    strcpy(&title[MAX_TITLE_SIZE - 5], " ...");
  }

  page.set_limits(fmt);
  page.new_paragraph(fmt);
  #if EPUB_INKPLATE_BUILD
    if (nvs_mgr.id_exists(book->id)) page.add_text("[Reading] ", fmt);
  #endif
  page.add_text(title, fmt);
  page.end_paragraph(fmt);

  fmt.font_index = AUTHOR_FONT;
  fmt.font_size  = AUTHOR_FONT_SIZE;
  fmt.font_style = Fonts::FaceStyle::ITALIC;

  page.new_paragraph(fmt);
  page.add_text(book->author, fmt);
  page.end_paragraph(fmt);

  #if EPUB_INKPLATE_BUILD
    BatteryViewer::show();
  #endif

  page.paint(false);
}

void 
MatrixBooksDirViewer::clear_highlight()
{
  if (current_item_idx == -1) return;

  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  // Clear the highlighting of the current item

  int16_t book_idx = current_page_nbr * books_per_page + current_item_idx;

  int16_t column_idx = current_item_idx / line_count;
  int16_t line_idx   = current_item_idx % line_count;

  int16_t xpos = 5 + ((BooksDir::max_cover_width + horiz_space_between_entries) * column_idx);
  int16_t ypos = first_entry_ypos + ((BooksDir::max_cover_height + vert_space_between_entries) * line_idx);

  const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

  if (book == nullptr) return;

  // TTF * font = fonts.get(5, 9);

  Page::Format fmt = {
    .line_height_factor = 0.8,
    .font_index         = TITLE_FONT,
    .font_size          = TITLE_FONT_SIZE,
    .indent             = 0,
    .margin_left        = 0,
    .margin_right       = 0,
    .margin_top         = 0,
    .margin_bottom      = 0,
    .screen_left        = 10,
    .screen_right       = 10,
    .screen_top         = 10,
    .screen_bottom      = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),
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

  page.clear_highlight(Dim(BooksDir::max_cover_width + 4, BooksDir::max_cover_height + 4), 
                        Pos(xpos - 2, ypos - 2));
  page.clear_highlight(Dim(BooksDir::max_cover_width + 6, BooksDir::max_cover_height + 6), 
                        Pos(xpos - 3, ypos - 3));

  page.clear_region(Dim(Screen::WIDTH - 10, (title_font_height << 1) + author_font_height),
                    Pos(10, 10));

  #if (INKPLATE_6PLUS || TOUCH_TRIAL)
    fmt.screen_top = 10 + title_font_height;
    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.add_text(TOUCH_AND_HOLD_STR, fmt);
    page.end_paragraph(fmt);
  #endif

  #if EPUB_INKPLATE_BUILD
    BatteryViewer::show();
  #endif

  page.paint(false);

  current_item_idx = -1;
}

int16_t
MatrixBooksDirViewer::show_page_and_highlight(int16_t book_idx)
{
  int16_t page_nbr = book_idx / books_per_page;
  int16_t item_idx = book_idx % books_per_page;

  if (current_page_nbr != page_nbr) {
    show_page(page_nbr, item_idx);
    current_page_nbr = page_nbr;
  }
  else {
    if (item_idx != current_item_idx) highlight(item_idx);
  }
  current_book_idx = book_idx;
  return current_book_idx;
}

void
MatrixBooksDirViewer::highlight_book(int16_t book_idx)
{
  highlight(book_idx % books_per_page);  
}

int16_t
MatrixBooksDirViewer::next_page()
{
  int16_t page_nbr = current_page_nbr + 1;
  if (page_nbr >= page_count) {
    page_nbr = page_count - 1;
  }
  if (current_page_nbr != page_nbr) {
    show_page(page_nbr, 0);
    current_book_idx = page_nbr * books_per_page;
  }
  else if ((page_nbr + 1) == page_count) {
    #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
      highlight(books_dir.get_book_count() % books_per_page - 1);
    #endif
    current_book_idx = books_dir.get_book_count() - 1;
  }
  return current_book_idx;
}

int16_t
MatrixBooksDirViewer::prev_page()
{
  int16_t page_nbr = current_page_nbr - 1;
  if (page_nbr < 0) {
    page_nbr = 0;
  }
  if (current_page_nbr != page_nbr) {
    show_page(page_nbr, 0);
  }
  else {
    #if !(INKPLATE_6PLUS || TOUCH_TRIAL)
      highlight(0);
    #endif
  }
  current_book_idx = page_nbr * books_per_page;
  return current_book_idx;
}

int16_t
MatrixBooksDirViewer::next_item()
{
  int16_t book_idx = current_book_idx + 1;
  if (book_idx >= books_dir.get_book_count()) {
    book_idx = books_dir.get_book_count() - 1;
  }
  return show_page_and_highlight(book_idx);
}

int16_t
MatrixBooksDirViewer::prev_item()
{
  int16_t book_idx = current_book_idx - 1;
  if (book_idx < 0) book_idx = 0;
  return show_page_and_highlight(book_idx);
}

int16_t
MatrixBooksDirViewer::next_column()
{
  int16_t book_idx = current_book_idx + line_count;
  if (book_idx >= books_dir.get_book_count()) {
    book_idx = books_dir.get_book_count() - 1;
  }
  return show_page_and_highlight(book_idx);
}

int16_t
MatrixBooksDirViewer::prev_column()
{
  int16_t book_idx = current_book_idx - line_count;
  if (book_idx < 0) book_idx = 0;
  return show_page_and_highlight(book_idx);
}
