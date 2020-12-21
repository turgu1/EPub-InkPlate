// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __IMAGE_INFO_HPP__
#define __IMAGE_INFO_HPP__

#include <cinttypes>

enum class ImageType { UNKNOWN, JPEG, PNG };

struct ImageInfo {
  uint32_t  size;
  uint16_t  width;
  uint16_t  height;
  uint8_t   pixel_size;
  ImageType image_type;
};

#if __IMAGE_INFO__
  ImageInfo * get_image_info(uint8_t * mem_image, uint32_t size);
#else
  extern ImageInfo * get_image_info(uint8_t * mem_image, uint32_t size);
#endif

#endif