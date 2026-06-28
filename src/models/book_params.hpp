// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "config_template.hpp"


enum class Param { VERSION, SHOW_PICTURES, FONT_SIZE, USE_FONTS_IN_BOOK, FONT, LINE_HEIGHT, COLUMN_COUNT };


#define BOOK_PARAMS_VERSION 2

using BookParams = ConfigBase<Param, 7>;

#if __BOOK_PARAMS__
  static int8_t version;
  static int8_t showPictures;   ///< -1: uses default, 0/1 bool value
  static int8_t fontSize;       ///< -1: uses default, >0 font size in points
  static int8_t lineHeight;     ///< -1: uses default, 0/1/2 line height factor
  static int8_t useFontsInBook; ///< -1: uses default, 0/1 bool value
  static int8_t font;           ///< -1: uses default, >= 0 font index
  static int8_t columnCount;    ///< -1: uses default, 1..4

  static int8_t theVersion   = BOOK_PARAMS_VERSION;
  static int8_t defaultValue = -1;

  template <>
  BookParams::CfgType BookParams::cfg = { {
    { Param::VERSION,           BookParams::EntryType::BYTE, "version",        &version,        &theVersion,   0 },
    { Param::SHOW_PICTURES,     BookParams::EntryType::BYTE, "showPictures",   &showPictures,   &defaultValue, 0 },
    { Param::FONT_SIZE,         BookParams::EntryType::BYTE, "fontSize",       &fontSize,       &defaultValue, 0 },
    { Param::LINE_HEIGHT,       BookParams::EntryType::BYTE, "lineHeight",     &lineHeight,     &defaultValue, 0 },
    { Param::USE_FONTS_IN_BOOK, BookParams::EntryType::BYTE, "useFontsInBook", &useFontsInBook, &defaultValue, 0 },
    { Param::FONT,              BookParams::EntryType::BYTE, "font",           &font,           &defaultValue, 0 },
    { Param::COLUMN_COUNT,      BookParams::EntryType::BYTE, "columnCount",    &columnCount,    &defaultValue, 0 },
  } };

#endif
