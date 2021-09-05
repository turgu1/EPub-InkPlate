// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/image.hpp"
#include "models/png_image.hpp"
#include "models/jpeg_image.hpp"

class ImageFactory {

  public:
    static Image * create(std::string filename, Dim max) {
      std::string ext = filename.substr(filename.find_last_of(".") + 1);
      if (ext == "png") return new PngImage(filename, max);
      else if ((ext == "jpg" ) || 
              (ext == "jpeg")) return new JPegImage(filename, max);
      return nullptr;
    }
}; 