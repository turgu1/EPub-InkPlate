// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "image.hpp"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT

#include "stb_image_resize2.h"

void Image::resize(Dim new_dim) {
  LOG_D("Resize to [%d, %d] %d bytes.", new_dim.width, new_dim.height,
        new_dim.width * new_dim.height);

  if (bitmap != nullptr) {
    auto resized_bitmap = make_unique_himem<uint8_t[]>(new_dim.width * new_dim.height);

    stbir_resize_uint8_linear(bitmap.get(), dim.width, dim.height, 0, resized_bitmap.get(),
                              new_dim.width, new_dim.height, 0, (stbir_pixel_layout)1);

    bitmap = std::move(resized_bitmap);
    dim    = new_dim;
  }
}
