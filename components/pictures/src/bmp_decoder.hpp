#pragma once

#include "global.hpp"

#include <cstdint>
#include <cstring>

class BMPDecoder {
  private:
    static constexpr const char * TAG = "BMPDecoder";

  public:
    BMPDecoder() = default;
    ~BMPDecoder() = default;

    /**
     * Decodes an uncompressed or RLE4 4-bit grayscale BMP into a packed 4-bit output buffer.
     * Output format: 2 pixels per byte (MSB = Pixel N, LSB = Pixel N+1).
     * Automatically handles video inversion based on the embedded color palette.
     */
    bool decode(const uint8_t* bmpData, size_t bmpSize, uint8_t* outputBuffer) {
      if (bmpData == nullptr || bmpSize < 54) {
        LOG_E("Invalid or truncated BMP data.");
        return false;
      }

      if (bmpData[0] != 0x42 || bmpData[1] != 0x4D) {
        LOG_E("Not a valid BMP file.");
        return false;
      }

      uint32_t pixelDataOffset = *reinterpret_cast<const uint32_t*>(&bmpData[10]);
      int32_t  width           = *reinterpret_cast<const int32_t*>(&bmpData[18]);
      int32_t  height          = *reinterpret_cast<const int32_t*>(&bmpData[22]);
      uint16_t bitCount        = *reinterpret_cast<const uint16_t*>(&bmpData[28]);
      uint32_t compression     = *reinterpret_cast<const uint32_t*>(&bmpData[30]);

      if (bitCount != 4) {
        LOG_E("Unsupported bit depth. Only 4-bpp is supported.");
        return false;
      }
      if (compression != 0 && compression != 2) {   // 0 = BI_RGB, 2 = BI_RLE4
        LOG_E("Unsupported compression format. Only Uncompressed and BI_RLE4 are supported.");
        return false;
      }
      if (width <= 0 || height == 0) {
        LOG_E("Invalid image dimensions.");
        return false;
      }

      bool     isBottomUp = (height > 0);
      uint32_t absHeight = isBottomUp ? height : -height;
      uint32_t outputRowStride = (width + 1) / 2;

      // Auto-detect if the palette expects inverted video signals
      bool invertPalette = false;
      if (pixelDataOffset >= 58) {
        uint8_t b = bmpData[54];
        uint8_t g = bmpData[55];
        uint8_t r = bmpData[56];
        if (r > 200 && g > 200 && b > 200) {
          invertPalette = true;
        }
      }

      // Initialize output buffer space to zero
      std::memset(outputBuffer, 0, outputRowStride * absHeight);

      if (compression == 2) {
        // Execute RLE4 Decoding Stream
        if (!decodeRLE4(bmpData + pixelDataOffset, bmpSize - pixelDataOffset,
                        outputBuffer, width, absHeight, outputRowStride, isBottomUp, invertPalette)) {
          LOG_E("RLE4 decoding failed.");
          return false;
        }
      } else {
        // Execute Standard Uncompressed Extraction Loop
        uint32_t inputRowStride = ((width * 4 + 31) / 32) * 4;
        if (pixelDataOffset + (inputRowStride * absHeight) > bmpSize) {
          LOG_E("BMP data missing expected pixel payload.");
          return false;
        }

        const uint8_t* pixelSource = bmpData + pixelDataOffset;

        for (uint32_t y = 0; y < absHeight; ++y) {
          uint32_t       srcRowIndex = isBottomUp ? (absHeight - 1 - y) : y;
          const uint8_t* srcRow = pixelSource + (srcRowIndex * inputRowStride);
          uint8_t*       destRow = outputBuffer + (y * outputRowStride);

          for (int32_t x = 0; x < width; ++x) {
            uint8_t inputByte = srcRow[x / 2];
            uint8_t pixelValue = (x % 2 == 0) ? ((inputByte >> 4) & 0x0F) : (inputByte & 0x0F);

            if (invertPalette) {
              pixelValue = 15 - pixelValue;
            }

            if (x % 2 == 0) {
              destRow[x / 2] |= (pixelValue << 4);
            } else {
              destRow[x / 2] |= (pixelValue & 0x0F);
            }
          }
        }
      }

      return true;
    }

    Dim getDim(const uint8_t* bmpData, size_t bmpSize) {
      if (bmpData == nullptr || bmpSize < 54) {
        LOG_E("Invalid or truncated BMP data.");
        return Dim(0, 0);
      }

      if (bmpData[0] != 0x42 || bmpData[1] != 0x4D) {
        LOG_E("Not a valid BMP file.");
        return Dim(0, 0);
      }

      int32_t width  = *reinterpret_cast<const int32_t*>(&bmpData[18]);
      int32_t height = *reinterpret_cast<const int32_t*>(&bmpData[22]);

      if (width <= 0 || height == 0) {
        LOG_E("Invalid image dimensions.");
        return Dim(0, 0);
      }

      return Dim(static_cast<uint16_t>(width), static_cast<uint16_t>(std::abs(height)));
    }

  private:
    /**
     * Internal implementation of the Microsoft BI_RLE4 state machine engine.
     */
    bool decodeRLE4(const uint8_t* rleData, size_t rleSize, uint8_t* outputBuffer,
                    int32_t width, uint32_t height, uint32_t outputRowStride,
                    bool isBottomUp, bool invertPalette) {
      size_t  p = 0;        // Byte pointer in RLE stream
      int32_t x = 0;        // Current column position
      int32_t y = 0;        // Current row position (relative to rendering canvas)

      auto    writePixel = [&](int32_t px, int32_t py, uint8_t val) {
                             if (px >= 0 && px < width && py >= 0 && py < static_cast<int32_t>(height)) {
                               uint32_t targetY = isBottomUp ? (height - 1 - py) : py;
                               uint8_t* destRow = outputBuffer + (targetY * outputRowStride);

                               if (invertPalette) {
                                 val = 15 - val;
                               }

                               if (px % 2 == 0) {
                                 destRow[px / 2] = (destRow[px / 2] & 0x0F) | (val << 4);
                               } else {
                                 destRow[px / 2] = (destRow[px / 2] & 0xF0) | (val & 0x0F);
                               }
                             }
                           };

      while (p + 1 < rleSize) {
        uint8_t count = rleData[p++];
        uint8_t mode  = rleData[p++];

        if (count == 0) {
          // Escape codes / Absolute Mode
          if (mode == 0) {
            // End of Line escape
            x = 0;
            y++;
          } else if (mode == 1) {
            // End of Bitmap escape
            return true;
          } else if (mode == 2) {
            // Delta escape (skip X and Y offsets)
            if (p + 1 >= rleSize) { break; }
            x += rleData[p++];
            y += rleData[p++];
          } else {
            // Absolute Mode (mode contains the literal pixel count)
            uint32_t numPixels = mode;
            uint32_t bytesToRead = (numPixels + 1) / 2;

            if (p + bytesToRead > rleSize) {
              LOG_E("RLE stream tracking out of memory bounds during absolute mode parsing.");
              return false;
            }

            for (uint32_t i = 0; i < numPixels; ++i) {
              uint8_t rawByte = rleData[p + (i / 2)];
              uint8_t pixelValue = (i % 2 == 0) ? ((rawByte >> 4) & 0x0F) : (rawByte & 0x0F);
              writePixel(x, y, pixelValue);
              x++;
            }

            p += bytesToRead;
            // Absolute blocks are padded to a word (16-bit) boundary in the byte stream
            if (bytesToRead % 2 != 0) {
              p++;
            }
          }
        } else {
          // Encoded Mode (repeat sequence)
          uint8_t highPixel = (mode >> 4) & 0x0F;
          uint8_t lowPixel  = mode & 0x0F;

          for (uint32_t i = 0; i < count; ++i) {
            uint8_t pixelValue = (i % 2 == 0) ? highPixel : lowPixel;
            writePixel(x, y, pixelValue);
            x++;
          }
        }
      }

      return true;
    }
};

