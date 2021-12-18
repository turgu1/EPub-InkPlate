// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/png_image.hpp"
#include "helpers/unzip.hpp"
#include "viewers/msg_viewer.hpp"
#include "alloc.hpp"
#include "mypngle.hpp"

#include <cmath>

static uint32_t  load_start_time;
static bool      waiting_msg_shown;
static uint16_t  pix_count;

static int32_t 
get_int_big_endian(uint8_t * a) {
  return
    a[0] << 24 |
    a[1] << 16 |
    a[2] <<  8 |
    a[3];
}

static void 
on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint8_t pix, uint8_t alpha)
{
  static constexpr char const * TAG = "PngImageOnDraw";
  
  // if (!waiting_msg_shown && (--pix_count == 0)) {
  //   pix_count = 2048;

  //   if ((ESP::millis() - load_start_time) > 2000) {
  //     waiting_msg_shown = true;

  //     msg_viewer.show(
  //       MsgViewer::INFO, 
  //       false, false, 
  //       "Retrieving Image", 
  //       "The application is retrieving image(s) from the e-book. Please wait."
  //     );
  //   }
  // }

  const Image::ImageData * data = ((PngImage *) mypngle_get_user_data(pngle))->get_image_data();
  int8_t   scale = ((PngImage *) mypngle_get_user_data(pngle))->get_scale_factor();
  uint16_t trans = alpha; // 0: fully transparent, 255: fully opaque

  if (scale) {
    x >>= scale;
    y >>= scale;
  }

  if ((x >= data->dim.width) || (y >= data->dim.height)) {
    LOG_D("Rect outside of image dimensions!");
    return;
  }
  else {
    data->bitmap[(y * data->dim.width) + x] = (trans * pix / 255) + (255 - trans);
  }
}

PngImage::PngImage(std::string filename, Dim max, bool load_bitmap) : Image(filename)
{
  LOG_I("Loading PNG image file %s", filename.c_str());

  if (unzip.open_stream_file(filename.c_str(), file_size)) {

    pngle_t * pngle   = mypngle_new();
    size_t    sz_work = WORK_SIZE;
    uint8_t * work    = (uint8_t *) allocate(sz_work);
    bool      first   = true;
    int32_t   total   = 0;

    mypngle_set_user_data(pngle, this);

    mypngle_set_draw_callback(pngle, on_draw);

    load_start_time = ESP::millis();
    waiting_msg_shown = false;
    pix_count = 2048;

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
        uint32_t w;
        
        scale      = 0;
        float s    = 1.0;

        if (width > max.width) {
          s = ((float) max.width) / width;
          h = floor(s * height) + 1;
        }
        if (h > max.height) {
          s = ((float) max.height) / height;
        }

        if (s < 1.0) {
          // if (s <= 0.125) scale = 3;
          // else if (s <=  0.25) scale = 2;
          // else if (s <=   0.5) scale = 1;
          if      (s < 0.25) scale = 3;
          else if (s <  0.5) scale = 2;
          else scale = 1;
        
          h = height >> scale;
          w = width  >> scale;
        }
        else {
          h = height;
          w = width;
        }

        LOG_D("Image size: [%d, %d] %d bytes.", w, h, w * h);

        if (load_bitmap) {
          if ((image_data.bitmap = (uint8_t *) allocate(w * h)) == nullptr) break;
          image_data.dim = Dim(w, h);
        }
        else {
          image_data.dim = Dim(w, h);
          break;
        }
      }

      int32_t res = mypngle_feed(pngle, work, size);

      if (res < 0) {
        LOG_E("Unable to load image. Error msg: %s", mypngle_error(pngle));
        break;
      }

      total += size;
      if (total >= file_size) break;

      size = WORK_SIZE;
    }

    free(work);
    mypngle_destroy(pngle);
    unzip.close_stream_file();

    LOG_I("PNG Image load complete");
  }
}

