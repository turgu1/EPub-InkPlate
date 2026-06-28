// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "picture.hpp"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT

#include "stb_image_resize2.h"

void Picture::convert_to_4bpp() {

  if (bitsPerPixel == 4) { return; }

  int  dstStride = (dim.width + 1) >> 1;

  auto dstData = makeUniqueHimem<uint8_t[]>(dstStride * dim.height);

  for (int y = 0, srcRowIdx = 0, dstRowIdx = 0;
       y < dim.height;
       ++y, srcRowIdx += dim.width, dstRowIdx += dstStride) {

    for (int x = 0, srcIdx = srcRowIdx, dstIdx = dstRowIdx; x < dim.width;
         x += 2, srcIdx += 2, ++dstIdx) {

      // Pack the two 4-bit pixels into a single byte
      dstData[dstIdx] = (bitmap[srcIdx] & 0xF0) | (((x + 1) < dim.width) ? bitmap[srcIdx + 1] >> 4 : 0);
    }
  }

  bitmap = std::move(dstData);
  bitsPerPixel = 4;
}

void Picture::resize_4bpp_bilinear(uint8_t *dst, Dim newDim) {
  if (newDim.width <= 1 || newDim.height <= 1 || dim.width <= 1 || dim.height <= 1) { return; }

  // Calculate row stride (bytes per row) taking into account odd widths.
  // If your format requires 4-byte padding per row, change this to: ((dim.width + 7) / 8) * 4;
  int srcStride = (dim.width + 1) >> 1;
  int dstStride = (newDim.width + 1) >> 1;

  // Helper lambda to fetch and unpack 4bpp pixels with correct row strides
  auto getPixel = [&](int x, int y) {
                    if (x < 0) { x = 0; } else if (x >= dim.width) { x = dim.width - 1; }
                    if (y < 0) { y = 0; } else if (y >= dim.height) { y = dim.height - 1; }

                    // Find byte index using the explicit row stride
                    int byteIndex = (y * srcStride) + (x >> 1);
                    int shift = (1 - (x & 1)) << 2; // Even x = high nibble (shift 4), Odd x = low nibble (shift 0)
                    return (bitmap[byteIndex] >> shift) & 0x0F;
                  };

  // Calculate fixed-point ratios scaled by 256 (8 bits of precision)
  int xRatio = ((dim.width - 1) << 8) / newDim.width;
  int yRatio = ((dim.height - 1) << 8) / newDim.height;

  for (int y = 0; y < newDim.height; ++y) {
    int fy = y * yRatio;
    int y1 = fy >> 8;
    int y2 = std::min(y1 + 1, dim.height - 1);
    int dy = fy & 0xFF;     // Fractional vertical distance (0 to 255)
    int invDy = 256 - dy;

    // Calculate starting memory address for the current destination row
    uint8_t *dstRow = dst + (y * dstStride);
    uint8_t  packedByte = 0;

    for (int x = 0; x < newDim.width; ++x) {
      int fx = x * xRatio;
      int x1 = fx >> 8;
      int x2 = std::min(x1 + 1, dim.width - 1);
      int dx = fx & 0xFF;       // Fractional horizontal distance (0 to 255)
      int invDx = 256 - dx;

      // Fetch the 4 neighboring pixel values
      int q11 = getPixel(x1, y1);
      int q21 = getPixel(x2, y1);
      int q12 = getPixel(x1, y2);
      int q22 = getPixel(x2, y2);

      // Bilinear interpolation using integer weights
      int r1 = (q11 * invDx + q21 * dx) >> 8;
      int r2 = (q12 * invDx + q22 * dx) >> 8;
      int val = (r1 * invDy + r2 * dy) >> 8;

      // Clamp value to 4-bit bounds (0-15)
      uint8_t pixel4bpp = static_cast<uint8_t>(std::clamp(val, 0, 15));

      // Pack on-the-fly into destination row
      if ((x & 1) == 0) {
        // Even pixel: Stage into the high nibble
        packedByte = pixel4bpp << 4;

        // If this is the absolute last pixel of an odd row, save it immediately
        if (x == newDim.width - 1) {
          dstRow[x >> 1] = packedByte;
        }
      } else {
        // Odd pixel: Merge into low nibble and save the completed byte
        packedByte |= (pixel4bpp & 0x0F);
        dstRow[x >> 1] = packedByte;
      }
    }
  }
}

auto Picture::resize(Dim newDim) -> void {
  LOG_D("Resize to [{}, {}] {} bytes.", newDim.width, newDim.height, newDim.width * newDim.height);

  if (bitmap != nullptr) {
    if (bitsPerPixel == 8) {
      auto resizedBitmap = makeUniqueHimem<uint8_t[]>(newDim.width * newDim.height);

      stbir_resize_uint8_linear(bitmap.get(), dim.width, dim.height, 0, resizedBitmap.get(),
                                newDim.width, newDim.height, 0, (stbir_pixel_layout)1);
      bitmap = std::move(resizedBitmap);


    } else {
      auto resizedBitmap = makeUniqueHimem<uint8_t[]>(((newDim.width + 1) >> 1) * newDim.height);

      resize_4bpp_bilinear(resizedBitmap.get(), newDim);
      bitmap = std::move(resizedBitmap);
    }

    dim    = newDim;
  }
}
