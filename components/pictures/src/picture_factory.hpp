// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "bitmap_picture.hpp"
#include "fonts.hpp"
#include "gif_picture.hpp"
#include "jpeg_picture.hpp"
#include "picture.hpp"
#include "png_picture.hpp"
#include "svg_picture.hpp"
#include "bmp_picture.hpp"

class PictureFactory {

  public:
    static auto create(HimemString filename, Dim max, bool loadBitmap, FontPtr &font,
                       bool fromFile = false) -> PicturePtr {

      auto ext = filename.substr(filename.find_last_of(".") + 1);

      if ((ext == "png") && !fromFile) {
        return PngPicture::Make(filename, max, loadBitmap);
      } else if ((ext == "jpg") || (ext == "jpeg")) {
        return JPegPicture::Make(filename, max, loadBitmap, fromFile);
      } else if (ext == "bmp") {
        return BmpPicture::Make(filename, max, loadBitmap, fromFile);
      } else if (ext == "svg") {
        return SvgPicture::Make(filename, max, loadBitmap, font, fromFile);
      } else if (ext == "gif") {
        return GifPicture::Make(filename, max, loadBitmap, fromFile);
      }

      return nullptr;
    }

    static auto create(Dim d, const uint8_t *b, uint32_t size) -> PicturePtr {
      return BitmapPicture::Make(d, b, size);
    }
};