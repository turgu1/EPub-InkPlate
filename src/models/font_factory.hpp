// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "models/fonts_db.hpp"
#include "models/ibmf.hpp"
#include "models/ttf2.hpp"

class FontFactory {

public:
  static FontPtr create(const FontFaceDescriptorPtr &descr, FT_Library &library) {
    HimemString ext = descr->filename.substr(descr->filename.find_last_of(".") + 1);

    if (ext == "ibmf")
      return IBMF::Make(descr);
    else if ((ext == "ttf") || (ext == "otf"))
      return TTF::Make(descr, library);

    return nullptr;
  }
};