// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "bitmap_picture.hpp"
#include "jpeg_picture.hpp"
#include "picture.hpp"
#include "png_picture.hpp"

class PictureFactory {

public:
  static auto create(std::string filename, Dim max, bool loadBitmap, bool fromFile = false)
      -> PicturePtr {

    std::string ext = filename.substr(filename.find_last_of(".") + 1);

    if ((ext == "png") && !fromFile) {
      return PngPicture::Make(filename, max, loadBitmap);
    } else if ((ext == "jpg") || (ext == "jpeg")) {
      return JPegPicture::Make(filename, max, loadBitmap, fromFile);
    }

    return nullptr;
  }

  static auto create(Dim d, const uint8_t *b, uint32_t size) -> PicturePtr {
    return BitmapPicture::Make(d, b, size);
  }
};