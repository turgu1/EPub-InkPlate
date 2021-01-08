#ifndef __BOOK_PARAMS_HPP__
#define __BOOK_PARAMS_HPP__

#include <string>
#include <iostream>

class BookParams
{
  public:
    bool read(const std::string fname);
    bool save();

  private:
    static constexpr char const * TAG                 = "BooksDir";
    static const uint16_t         BOOK_PARAMS_VERSION = 1;

    bool        modified;
    std::string filename;

    int8_t   show_images;         ///< -1: uses default, 0/1 bool value
    int8_t   font_size;           ///< -1: uses default, >0 font size in points
    int8_t   use_fonts_in_books;  ///< -1: uses default, 0/1 bool value
    int8_t   default_font;        ///< -1: uses default, >= 0 font index
};

#endif