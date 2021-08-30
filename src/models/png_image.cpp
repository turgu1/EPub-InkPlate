// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/png_image.hpp"
#include "helpers/unzip.hpp"
#include "alloc.hpp"
#include "logging.hpp"

#include "pngle.h"

#include <math.h>

static int32_t 
get_int_big_endian(uint8_t * a) {
  return
    a[0] << 24 |
    a[1] << 16 |
    a[2] <<  8 |
    a[3];
}

static void 
on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
  static constexpr char const * TAG = "JPegImageOnDraw";
  
  const Image::ImageData * data = ((PngImage *) pngle_get_user_data(pngle))->get_image_data();
  float scale = ((PngImage *) pngle_get_user_data(pngle))->get_scale_factor();
  float r = 0.30 * rgba[0];
  float g = 0.59 * rgba[1];
  float b = 0.11 * rgba[2];
  float a = ((float) rgba[3]) / 255.0; // 0: fully transparent, 255: fully opaque

  if (scale != 1.0) {
    x = floor(scale * x);
    y = floor(scale * y);
  }

  if ((x >= data->dim.width) || (y >= data->dim.height)) {
    LOG_E("Rect outside of image dimensions!");
    return;
  }
  else {
    data->bitmap[(y * data->dim.width) + x] = ((r + b + g) * a) + ((1 - a) * 255);
  }
}

PngImage::PngImage(std::string filename, Dim max) : Image(filename)
{
  LOG_D("Loading image file %s", filename.c_str());

  if (unzip.open_stream_file(filename.c_str(), file_size)) {

    pngle_t * pngle   = pngle_new();
    size_t    sz_work = WORK_SIZE;
    uint8_t * work    = (uint8_t *) allocate(sz_work);
    bool      first   = true;
    int32_t   total   = 0;

    pngle_set_user_data(pngle, this);

    pngle_set_draw_callback(pngle, on_draw);

    /* Prepare to decompress */

    uint32_t size = WORK_SIZE;
    while (unzip.get_stream_data((char *) work, size)) {
      if (size == 0) break;

      if (first) {
        first = false;

        if (size < 24) break;
        uint32_t width  = get_int_big_endian(&work[16]);
        uint32_t height = get_int_big_endian(&work[20]);

        uint32_t h = height;
        uint32_t w = width;
        scale      = 1.0;

        if (width > max.width) {
          scale  = (float) max.width / width;
          w      = floor(scale * width ) + 1;
          h      = floor(scale * height) + 1;
        }
        if (h > max.height) {
          scale = (float) max.height / height;
          w      = floor(scale * width ) + 1;
          h      = floor(scale * height) + 1;
        }

        if ((image_data.bitmap = (uint8_t *) allocate(w * h)) == nullptr) break;
        image_data.dim = Dim(w, h);

        LOG_D("Image size: [%d, %d] %d bytes.", w, h, w * h);
      }

      int32_t res = pngle_feed(pngle, work, size);

      if (res < 0) {
        LOG_E("Unable to load image. Error msg: %s", pngle_error(pngle));
        break;
      }

      total += size;
      if (total >= file_size) break;

      size = WORK_SIZE;
    }

    free(work);
    pngle_destroy(pngle);
    unzip.close_stream_file();
  }
}

