// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cstring>

#include "strlcpy.hpp"

//#pragma GCC diagnostic error "-Wframe-larger-than=10"

#define APP_VERSION "1.3.2"

#if !(defined(EPUB_LINUX_BUILD) || defined(EPUB_INKPLATE_BUILD)) 
  #error "BUILD_TYPE Not Set."
#else
  #if !(EPUB_LINUX_BUILD || EPUB_INKPLATE_BUILD)
    #error "One of EPUB_LINUX_BUILD or EPUB_INKPLATE_BUILD must be set to 1"
  #endif
#endif

#define USE_EPUB_FONTS 1  ///< 1: Embeded fonts in EPub books are loaded and used 0: Only preset fonts are used

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

#define DEBUGGING_AID 0  ///< 1: Allow for specific page debugging output

#if DEBUGGING_AID
  #define PAGE_FROM  6
  #define PAGE_TO    6 //PAGE_FROM
#endif

// The following data definitions are here for laziness... 
// ToDo: Move them to the appropriate location

struct Dim { 
  uint16_t width; 
  uint16_t height; 
  Dim(uint16_t w, uint16_t h) { 
    width  = w; 
    height = h;
  }
  Dim() {}
};

struct Pos { 
  int16_t x; 
  int16_t y; 
  Pos(int16_t xpos, int16_t ypos) { 
    x = xpos; 
    y = ypos; 
  }
  Pos() {}
};

struct Dim8 { 
  uint8_t width; 
  uint8_t height; 
  Dim8(uint8_t w, uint8_t h) { 
    width  = w; 
    height = h;
  }
  Dim8() {}
};

struct Pos8 { 
  int8_t x; 
  int8_t y; 
  Pos8(int8_t xpos, int8_t ypos) { 
    x = xpos; 
    y = ypos; 
  }
  Pos8() {}
};
