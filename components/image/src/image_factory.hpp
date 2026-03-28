// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "himem.hpp"

#include "image.hpp"
#include "jpeg_image.hpp"
#include "png_image.hpp"

class ImageFactory {

public:
  static auto create(std::string filename, Dim max, bool load_bitmap, bool from_file = false)
      -> ImagePtr {

    std::string ext = filename.substr(filename.find_last_of(".") + 1);

    if ((ext == "png") && !from_file) {
      return make_unique_himem<PngImage>(filename, max, load_bitmap);
    } else if ((ext == "jpg") || (ext == "jpeg")) {
      return make_unique_himem<JPegImage>(filename, max, load_bitmap, from_file);
    }

    return nullptr;
  }
};