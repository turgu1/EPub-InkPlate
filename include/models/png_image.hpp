// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "image.hpp"

class PngImage : public Image
{
  public:
    PngImage(std::string filename, Dim max);

    inline float get_scale_factor() { return scale; }
  private:
    static constexpr char const * TAG = "PngImage";
    const uint16_t WORK_SIZE = 10 * 1024;
    float scale;
};