// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __IMAGE_INFO__ 1
#include "image_info.hpp"

#include <cstring>

static ImageInfo image_info;

static int32_t 
get_int_big_endian(uint8_t * a) {
  return
    a[0] << 24 |
    a[1] << 16 |
    a[2] <<  8 |
    a[3];
}

static uint16_t 
get_short_big_endian(uint8_t * a) {
  return
    a[0] <<  8 |
    a[1];
}

bool
read_jpeg(uint8_t * mem_image)
{
  uint32_t offset = 2;

  while (true) {
    uint16_t marker = get_short_big_endian(&mem_image[offset]);
    uint16_t size   = get_short_big_endian(&mem_image[offset + 2]);
    offset += 4;
    if ((marker & 0xff00) != 0xff00) {
      return false; // not a valid marker
    }
    else if ((marker >= 0xffc0) && 
             (marker <= 0xffcf) && 
             (marker != 0xffc4) && 
             (marker != 0xffc8)) {

      image_info.image_type = JPEG;
      image_info.pixel_size = mem_image[offset] * mem_image[offset + 5];
      image_info.width      = get_short_big_endian(&mem_image[offset + 3]);
      image_info.height     = get_short_big_endian(&mem_image[offset + 1]);
      image_info.size       = image_info.width * 
                              image_info.size * 
                              (image_info.pixel_size >> 3);
      return true;
    } 
    else {
      offset += size - 2;
    }
  }
}

static bool
read_png(uint8_t * mem_image)
{
  constexpr uint8_t PNG_MAGIC[6] = { 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
  if (memcmp(&mem_image[2], PNG_MAGIC, 6) != 0) return false;

  image_info.image_type = PNG;
  image_info.width      = get_int_big_endian(&mem_image[16]);
  image_info.height     = get_int_big_endian(&mem_image[20]);
  image_info.pixel_size = mem_image[24];

  int colorType = mem_image[25];
  if (colorType == 2 || colorType == 6) {
      image_info.pixel_size *= 3;
  }

  image_info.size = image_info.width * 
                    image_info.size * 
                    (image_info.pixel_size >> 3);
  return true;
}

ImageInfo *
get_image_info(uint8_t * mem_image, uint32_t size)
{
  if ((mem_image[0] == ((uint8_t) 0xFF)) && 
      (mem_image[1] == ((uint8_t) 0xD8))) {
     
    return read_jpeg(mem_image) ? &image_info : nullptr; // JPeg
  }
  else if ((mem_image[0] == ((uint8_t) 0x89)) && 
           (mem_image[1] == ((uint8_t) 0x50))) {
    
    return read_png(mem_image) ? &image_info : nullptr; // PNG
  }
  else return nullptr;
}