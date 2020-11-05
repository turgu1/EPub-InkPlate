#ifndef __GLOBAL_HPP__
#define __GLOBAL_HPP__

#include <cstdint>
#include <iostream>

#define APP_NAME "EPUB-INKPLATE" // Used to mark the database (file books_dir.cpp)

#if !(defined(EPUB_LINUX_BUILD) || defined(EPUB_INKPLATE6_BUILD)) 
  #error "BUILD_TYPE Not Set."
#else
  #if !(EPUB_LINUX_BUILD || EPUB_INKPLATE6_BUILD)
    #error "One of EPUB_LINUX_BUILD or EPUB_INKPLATE6_BUILD must be set to 1"
  #endif
#endif

#define DEBUGGING_AID  0   ///< 1: Allow for specific page debugging output

#define COMPUTE_SIZE   0  ///< ToDo: To implement on the ESP32
#define USE_EPUB_FONTS 1  ///< 1: Embeded fonts in EPub books are loaded and used 0: Only preset fonts are used

#define BASE_FONT_SIZE 12

#define MAX_COVER_WIDTH  70
#define MAX_COVER_HEIGHT 90

#if EPUB_LINUX_BUILD
  #define MAIN_FOLDER "/home/turgu1/Dev/EPub-Linux/bin"
#else
  #define MAIN_FOLDER "/sdcard"
#endif

#define FONTS_FOLDER MAIN_FOLDER "/fonts"
#define BOOKS_FOLDER MAIN_FOLDER "/books"


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
// page locations refresh time (done by the book_dir class at start time), 
// a lot of data will be output on the terminal. Then, opening the book,
// and going to the selected page, some similar information will be sent to 
// the terminal. You can then compare the results and find the differences.
//
// To be used, only one book must be in the books folder and the books_dir.db 
// file must be deleted before any trial.

#if DEBUGGING_AID
  #define PAGE_TO_SHOW_LOCATION 706
  #define SET_PAGE_TO_SHOW(val) { show_location = val == PAGE_TO_SHOW_LOCATION; }
  #define SHOW_LOCATION(msg) { if (show_location) { \
    std::cout << msg << " Offset:" << current_offset << " "; \
    page.show_controls("  "); \
    std::cout << "     "; \
    page.show_fmt(fmt, "  ");  }}

  #if _GLOBAL_
    bool show_location = false;
  #else
    extern bool show_location;
  #endif
#else
  #define SET_PAGE_TO_SHOW(val)
  #define SHOW_LOCATION(msg)
#endif

#if COMPUTE_SIZE
  #if _GLOBAL_
    int64_t memory_used;
    int64_t start_heap_size;
  #else
    extern int32_t memory_used;
    extern int64_t start_heap_size;
  #endif
#endif

#if 0
/**
 * @brief Book Compute Mode
 * 
 * This is used to indicate if we are computing the location of pages 
 * to include in the database, moving to the start of a page to be
 * shown, or preparing a page to display on screen. This is mainly 
 * used by the book_view and page classes to optimize the speed of 
 * computations.
 */
enum ComputeMode { LOCATION, MOVE, DISPLAY };
#if __GLOBAL__
  ComputeMode compute_mode;
#else
  extern ComputeMode compute_mode;
#endif
#endif


#endif