// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <cstdint>

#include <int_to_str.hpp>
#include <strlcpy.hpp>

// #pragma GCC diagnostic error "-Wframe-larger-than=10"

#ifndef APP_VERSION
  #define APP_VERSION "3.0.0"
#endif

#if !(EPUB_LINUX_BUILD || EPUB_INKPLATE_BUILD)
  #error "BUILD_TYPE Not Set."
#else
  #if !(EPUB_LINUX_BUILD || EPUB_INKPLATE_BUILD)
    #error "One of EPUB_LINUX_BUILD or EPUB_INKPLATE_BUILD must be set to 1"
  #endif
#endif

#define USE_EPUB_FONTS                                                                             \
  1 ///< 1: Embeded fonts in EPub books are loaded and used 0: Only preset fonts are used

#if EPUB_LINUX_BUILD
  #define MAIN_FOLDER "/home/turgu1/Dev/EPub-InkPlate/SDCard"
#endif

#if EPUB_INKPLATE_BUILD
  #define MAIN_FOLDER "/sdcard"
  #define LOG_LOCAL_LEVEL EPUB_LOG_LEVEL
#endif

#define FONTS_FOLDER MAIN_FOLDER "/fonts"
#define BOOKS_FOLDER MAIN_FOLDER "/books"

#ifndef DEBUGGING
  #define DEBUGGING 0
#endif

#include "logging.hpp"

// Debugging aid
//
// These are simple tools to show information about formatting for a specific page
// in a book being tested. The problems are often reflected through bad offsets
// computation for the beginning or/and end of pages.
//
// To get good results for the user, pages location offsets computed and put in the books
// database must be the same as the one found when the book is being displayed.
// If not, lines at the beginning or the end of a page can be duplicated or
// part of a paragraph can be lost.
//
// To use, set DEBUGGING_AID to 1 and select the page number (0 = first page),
// setting the PAGE_TO_SHOW_LOCATION. At
// pages location refresh time (done by the book_dir class at start time),
// a lot of data will be output on the terminal. Then, opening the book,
// and going to the selected page, some similar information will be sent to
// the terminal. You can then compare the results and find the differences.
//
// To be used, only one book must be in the books folder and the books_dir.db
// file must be deleted before any trial.

#define DEBUGGING_AID 0 ///< 1: Allow for specific page debugging output

#if DEBUGGING_AID
  #define PAGE_FROM 6
  #define PAGE_TO 6 // PAGE_FROM
#endif

// The following data definitions are here for laziness...
// ToDo: Move them to the appropriate location

struct Dim {
  uint16_t width{0};
  uint16_t height{0};
  // Dim(uint16_t w, uint16_t h) {
  //   width  = w;
  //   height = h;
  // }
  Dim() = default;
  constexpr Dim(uint16_t w, uint16_t h) : width{w}, height{h} {}

  constexpr uint16_t get_width() const { return width; }
  constexpr uint16_t get_height() const { return height; }
};

struct Pos {
  uint16_t x{0};
  uint16_t y{0};
  Pos(uint16_t xpos, uint16_t ypos) {
    x = xpos;
    y = ypos;
  }
  Pos() = default;
};

struct Dim8 {
  uint8_t width{0};
  uint8_t height{0};
  Dim8(uint8_t w, uint8_t h) {
    width  = w;
    height = h;
  }
  Dim8() = default;
};

struct Pos8 {
  uint8_t x{0};
  uint8_t y{0};
  Pos8(uint8_t xpos, uint8_t ypos) {
    x = xpos;
    y = ypos;
  }
  Pos8() = default;
};

struct Glyph {
  Dim dim{0, 0};
  int16_t xoff{0}, yoff{0};
  int16_t advance{0};
  int16_t pitch{0};
  int16_t line_height{0};
  int16_t ligature_and_kern_pgm_index{255};
  uint8_t *buffer{nullptr};
  void clear() {
    dim.height = dim.width = 0;
    xoff = yoff = 0;
    advance = pitch = line_height = 0;
    ligature_and_kern_pgm_index   = 255;
    buffer                        = nullptr;
  }
  Glyph() = default;
};

struct PageId {
  int16_t itemref_index{0};
  int32_t offset{0};
  PageId(int16_t idx, int32_t off) {
    itemref_index = idx;
    offset        = off;
  }
  PageId() = default;
};