#define __BOOKS_DIR_VIEW__ 1
#include "books_dir_view.hpp"

#include "fonts.hpp"
#include "page.hpp"
#include "screen.hpp"

#include <iomanip>

// static const char * TAG = "BooksDirView";

void 
BooksDirView::show_page(int16_t page_nbr, int16_t hightlight_item_idx)
{
  current_page_nbr = page_nbr;
  current_item_idx = hightlight_item_idx;

  int16_t book_idx = page_nbr * BOOKS_PER_PAGE;
  int16_t last     = book_idx + BOOKS_PER_PAGE;

  page.set_compute_mode(Page::DISPLAY);

  if (last > books_dir.get_book_count()) last = books_dir.get_book_count();

  int16_t xpos = 20 + BooksDir::max_cover_width;
  int16_t ypos = FIRST_ENTRY_YPOS;

  Page::Format fmt = {
      .line_height_factor = 0.8,
      .font_index         = 0,
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
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
      .text_transform     = CSS::NO_TRANSFORM
    };

  page.start(fmt);

  for (int item_idx = 0; book_idx < last; item_idx++, book_idx++) {

    int16_t top_pos = ypos;

    const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

    if (book == nullptr) break;
    
    Page::Image image = (Page::Image) {
      .bitmap = (uint8_t *) book->cover_bitmap,
      .width  = book->cover_width,
      .height = book->cover_height
    };
    page.put_image(image, 10, ypos);

    if (item_idx == current_item_idx) {
      page.put_highlight(Screen::WIDTH - (25 + BooksDir::max_cover_width), 
                         BooksDir::max_cover_height, 
                         xpos - 3, ypos);
    }

    fmt.font_index    = 0;
    fmt.font_size     = TITLE_FONT_SIZE;
    fmt.font_style    = Fonts::NORMAL;
    fmt.screen_top    = ypos,
    fmt.screen_bottom = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.put_text(book->title, fmt);
    page.end_paragraph(fmt);

    fmt.font_index = 2;
    fmt.font_size  = AUTHOR_FONT_SIZE;
    fmt.font_style = Fonts::ITALIC;

    page.new_paragraph(fmt);
    page.put_text(book->author, fmt);
    page.end_paragraph(fmt);

    ypos = top_pos + BooksDir::max_cover_height + 6;
  }

  std::ostringstream ostr;
  ostr << page_nbr + 1 << " / " << page_count();
  std::string str = ostr.str();

  TTF * font = fonts.get(0, PAGENBR_FONT_SIZE);

  fmt.line_height_factor = 1.0;
  fmt.font_index         = 0;
  fmt.font_size          = PAGENBR_FONT_SIZE;
  fmt.font_style         = Fonts::NORMAL;
  fmt.align              = CSS::CENTER_ALIGN;
  fmt.screen_left        = 10;
  fmt.screen_right       = 10; 
  fmt.screen_top         = 10;
  fmt.screen_bottom      = 10;

  page.set_limits(fmt);
  page.put_str_at(str, -1, Screen::HEIGHT + font->get_descender_height() - 2, fmt);
  page.paint();
}

void 
BooksDirView::highlight(int16_t item_idx)
{
  page.set_compute_mode(Page::DISPLAY);

  if (current_item_idx != item_idx) {

    // Clear the highlighting of the current item

    int16_t book_idx = current_page_nbr * BOOKS_PER_PAGE + current_item_idx;

    int16_t xpos = 20 + BooksDir::max_cover_width;
    int16_t ypos = FIRST_ENTRY_YPOS + (current_item_idx * (BooksDir::max_cover_height + 6));

    const BooksDir::EBookRecord * book = books_dir.get_book_data(book_idx);

    if (book == nullptr) return;

    page.clear_region(Screen::WIDTH - (25 + BooksDir::max_cover_width), 
                      BooksDir::max_cover_height, 
                      xpos - 3, ypos);

    // TTF * font = fonts.get(1, 9);

    Page::Format fmt = {
      .line_height_factor = 0.8,
      .font_index         = 0,
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
      .font_style         = Fonts::NORMAL,
      .align              = CSS::LEFT_ALIGN,
      .text_transform     = CSS::NO_TRANSFORM
    };

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.put_text(book->title, fmt);
    page.end_paragraph(fmt);

    // font = fonts.get(2, 8);

    fmt.font_index = 2;
    fmt.font_size  = AUTHOR_FONT_SIZE;
    fmt.font_style = Fonts::ITALIC;

    page.new_paragraph(fmt);
    page.put_text(book->author, fmt);
    page.end_paragraph(fmt);
    
    // Highlight the new current item

    current_item_idx = item_idx;

    book_idx = current_page_nbr * BOOKS_PER_PAGE + current_item_idx;
    ypos = FIRST_ENTRY_YPOS + (current_item_idx * (BooksDir::max_cover_height + 6));

    book = books_dir.get_book_data(book_idx);

    if (book == nullptr) return;
    
    page.put_highlight(Screen::WIDTH - (25 + BooksDir::max_cover_width), 
                       BooksDir::max_cover_height, 
                       xpos - 3, ypos);


    fmt.font_index    = 0,
    fmt.font_size     = TITLE_FONT_SIZE,
    fmt.font_style    = Fonts::NORMAL,
    fmt.screen_top    = ypos,
    fmt.screen_bottom = (int16_t)(Screen::HEIGHT - (ypos + BooksDir::max_cover_width + 20)),

    page.set_limits(fmt);
    page.new_paragraph(fmt);
    page.put_text(book->title, fmt);
    page.end_paragraph(fmt);

    fmt.font_index = 2,
    fmt.font_size  = AUTHOR_FONT_SIZE,
    fmt.font_style = Fonts::ITALIC,

    page.new_paragraph(fmt);
    page.put_text(book->author, fmt);
    page.end_paragraph(fmt);

    page.paint();
  }
}
