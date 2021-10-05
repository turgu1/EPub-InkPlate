// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/font.hpp"
#include "models/ttf2.hpp"
#include "models/ibmf.hpp"

class FontFactory {

  public:
    static Font * 
    create(const std::string & filename) {
      std::string ext = filename.substr(filename.find_last_of(".") + 1);

      if (ext == "ibmf") return new IBMF(filename);
      else if ((ext == "ttf") || 
               (ext == "otf")) return new TTF(filename);

      return nullptr;
    }

    static Font * 
    create(const std::string & filename, 
           unsigned char *     buffer,
           int32_t             size) {
      std::string ext = filename.substr(filename.find_last_of(".") + 1);

      if (ext == "ibmf") return new IBMF(filename);
      else if ((ext == "ttf") || 
               (ext == "otf")) return new TTF(filename);

      return nullptr;
    }
}; 