// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "picture.hpp"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT

#include "stb_image_resize2.h"

auto Picture::resize(Dim newDim) -> void {
  LOG_D("Resize to [%d, %d] %d bytes.", newDim.width, newDim.height, newDim.width * newDim.height);

  if (bitmap != nullptr) {
    auto resizedBitmap = makeUniqueHimem<uint8_t[]>(newDim.width * newDim.height);

    stbir_resize_uint8_linear(bitmap.get(), dim.width, dim.height, 0, resizedBitmap.get(),
                              newDim.width, newDim.height, 0, (stbir_pixel_layout)1);

    bitmap = std::move(resizedBitmap);
    dim    = newDim;
  }
}
