// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "png_picture.hpp"
#include "himem.hpp"
#include "mypngle.hpp"
#include "unzip.hpp"

#include <cinttypes>
#include <cmath>

// static uint32_t load_start_time;
// static bool waiting_msg_shown;
// static uint16_t pix_count;

static int32_t get_int_big_endian(uint8_t *a) { return a[0] << 24 | a[1] << 16 | a[2] << 8 | a[3]; }

static void on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint8_t pix, uint8_t alpha) {
  static constexpr char const *TAG = "PngPictureOnDraw";

  auto data      = ((PngPicture *)mypngle_get_user_data(pngle))->get_picture_data();
  auto scale     = ((PngPicture *)mypngle_get_user_data(pngle))->get_scale_factor();
  uint16_t trans = alpha; // 0: fully transparent, 255: fully opaque

  if (scale) {
    x >>= scale;
    y >>= scale;
  }

  if ((x >= data->dim.width) || (y >= data->dim.height)) {
    LOG_D("Rect outside of picture dimensions!");
  } else {
    data->bitmap[(y * data->dim.width) + x] = (trans * pix / 255) + (255 - trans);
  }
}

PngPicture::PngPicture(std::string filename, Dim max, bool load_bitmap) : Picture() {
  LOG_D("Loading PNG picture file %s", filename.c_str());

  if (unzip.open_stream_file(filename.c_str(), file_size)) {

    pngle_t *pngle = mypngle_new();
    auto work      = make_unique_himem<uint8_t[]>(WORK_SIZE);
    bool first     = true;
    uint32_t total = 0;

    mypngle_set_user_data(pngle, this);

    mypngle_set_draw_callback(pngle, on_draw);

    #if EPUB_INKPLATE_BUILD
      // load_start_time   = ESP::millis();
      // waiting_msg_shown = false;
      // pix_count = 2048;
    #endif

    /* Prepare to decompress */

    auto size = WORK_SIZE;
    while ((size = unzip.get_stream_data((char *)work.get(), size))) {

      if (first) {
        first = false;

        if (size < 24) break;
        uint32_t width  = get_int_big_endian(&work[16]);
        uint32_t height = get_int_big_endian(&work[20]);

        uint32_t h = height;
        uint32_t w;

        scale   = 0;
        float s = 1.0;

        if (width > max.width) {
          s = ((float)max.width) / width;
          h = floor(s * height) + 1;
        }
        if (h > max.height) {
          s = ((float)max.height) / height;
        }

        if (s < 1.0) {
          // if (s <= 0.125) scale = 3;
          // else if (s <=  0.25) scale = 2;
          // else if (s <=   0.5) scale = 1;
          if (s < 0.25)
            scale = 3;
          else if (s < 0.5)
            scale = 2;
          else
            scale = 1;

          h = height >> scale;
          w = width >> scale;
        } else {
          h = height;
          w = width;
        }

        LOG_D("Picture size: [%" PRIu32 ", %" PRIu32 "] %" PRIu32 " bytes.", w, h, w * h);

        if (load_bitmap) {
          if ((bitmap = make_unique_himem<uint8_t[]>(w * h)) == nullptr) break;
          dim = Dim(w, h);
        } else {
          dim = Dim(w, h);
          break;
        }
      }

      int32_t res = mypngle_feed(pngle, work.get(), size);

      if (res < 0) {
        LOG_E("Unable to load picture. Error msg: %s", mypngle_error(pngle));
        break;
      }

      total += size;
      if (total >= file_size) break;

      size = WORK_SIZE;
    }

    mypngle_destroy(pngle);
    unzip.close_stream_file();

    LOG_D("PNG Picture load complete");
  }
}
