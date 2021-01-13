#pragma once

#include "global.hpp"
#include "config_template.hpp"

enum class Param {
  VERSION, SHOW_IMAGES, FONT_SIZE, USE_FONTS_IN_BOOK, FONT
};

typedef ConfigBase<Param, 5> BookParams;

#if __BOOK_PARAMS__
  static int8_t version;
  static int8_t show_images;         ///< -1: uses default, 0/1 bool value
  static int8_t font_size;           ///< -1: uses default, >0 font size in points
  static int8_t use_fonts_in_book;   ///< -1: uses default, 0/1 bool value
  static int8_t font;                ///< -1: uses default, >= 0 font index

  static int8_t the_version    =  1;
  static int8_t default_value  = -1;

  template<>
  BookParams::CfgType BookParams::cfg = {{
    { Param::VERSION,           BookParams::EntryType::BYTE, "version",           &version,            &the_version,   0 },
    { Param::SHOW_IMAGES,       BookParams::EntryType::BYTE, "show_images",       &show_images,        &default_value, 0 },
    { Param::FONT_SIZE,         BookParams::EntryType::BYTE, "font_size",         &font_size,          &default_value, 0 },
    { Param::USE_FONTS_IN_BOOK, BookParams::EntryType::BYTE, "use_fonts_in_book", &use_fonts_in_book, &default_value, 0 },
    { Param::FONT,              BookParams::EntryType::BYTE, "font",              &font,               &default_value, 0 },
  }};
#endif
