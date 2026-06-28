#include "gif_decoder.hpp"

#include <array>
#include <cstring>
#include <memory>

namespace {

constexpr uint8_t GIF_TRAILER          = 0x3b;
constexpr uint8_t GIF_EXTENSION        = 0x21;
constexpr uint8_t GIF_IMAGE_DESCRIPTOR = 0x2c;
constexpr uint8_t GIF_GRAPHIC_CONTROL  = 0xf9;

auto chooseScale(const GifOptions &options) -> int32_t {
  if (options.test(GIF_OPTION(SCALE_EIGHTH))) return 8;
  if (options.test(GIF_OPTION(SCALE_QUARTER))) return 4;
  if (options.test(GIF_OPTION(SCALE_HALF))) return 2;
  return 1;
}

auto toGray(uint8_t r, uint8_t g, uint8_t b) -> uint8_t {
  return static_cast<uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
}

} // namespace

auto GifDecoder::SubBlockReader::init(const uint8_t *pData, int32_t iSize, int32_t iPos) -> bool {
  data      = pData;
  size      = iSize;
  pos       = iPos;
  remaining = 0;
  finished  = false;
  return data != nullptr && size > 0 && pos >= 0 && pos < size;
}

auto GifDecoder::SubBlockReader::nextByte(uint8_t &value) -> bool {
  if (finished) return false;

  while (remaining == 0) {
    if (pos >= size) return false;
    const uint8_t blockSize = data[pos++];
    if (blockSize == 0) {
      finished = true;
      return false;
    }
    if (pos + blockSize > size) return false;
    remaining = blockSize;
  }

  value = data[pos++];
  remaining--;
  return true;
}

auto GifDecoder::openRAM(const uint8_t *pData, int32_t iDataSize, GifDrawCallback pfnDraw)
    -> int32_t {
  reset();

  if (pData == nullptr || iDataSize < 32 || pfnDraw == nullptr) {
    lastError_ = GifError::INVALID_PARAM;
    return 0;
  }

  data_   = pData;
  size_   = iDataSize;
  drawCb_ = pfnDraw;

  if (!parse()) return 0;

  return 1;
}

auto GifDecoder::close() -> void { reset(); }

auto GifDecoder::decode(int32_t x, int32_t y, GifOptions iOptions) -> int32_t {
  if (data_ == nullptr || drawCb_ == nullptr || width_ <= 0 || height_ <= 0) {
    lastError_ = GifError::INVALID_PARAM;
    return 0;
  }

  const int32_t scale = chooseScale(iOptions);
  outputWidth_        = (width_ + scale - 1) / scale;
  outputHeight_       = (height_ + scale - 1) / scale;

  if (!decodeLzwRows(x, y, scale)) {
    if (lastError_ == GifError::SUCCESS) lastError_ = GifError::DECODE_ERROR;
    return 0;
  }

  lastError_ = GifError::SUCCESS;
  return 1;
}

auto GifDecoder::setUserPointer(void *p) -> void { user_ = p; }

auto GifDecoder::getLastError() const -> GifError { return lastError_; }

auto GifDecoder::getWidth() const -> int32_t { return width_; }

auto GifDecoder::getHeight() const -> int32_t { return height_; }

auto GifDecoder::getOutputWidth() const -> int32_t { return outputWidth_; }

auto GifDecoder::getOutputHeight() const -> int32_t { return outputHeight_; }

auto GifDecoder::reset() -> void {
  data_                = nullptr;
  size_                = 0;
  drawCb_              = nullptr;
  user_                = nullptr;
  width_               = 0;
  height_              = 0;
  outputWidth_         = 0;
  outputHeight_        = 0;
  imageDataPos_        = 0;
  lzwMinCodeSize_      = 0;
  interlaced_          = false;
  transparentIndex_    = 0;
  hasTransparentIndex_ = false;
  std::memset(paletteGray_, 0, sizeof(paletteGray_));
  lastError_ = GifError::SUCCESS;
}

auto GifDecoder::readU16(int32_t offset) const -> uint16_t {
  return static_cast<uint16_t>(data_[offset] | (data_[offset + 1] << 8));
}

auto GifDecoder::skipSubBlocks(int32_t startPos) -> int32_t {
  int32_t pos = startPos;
  while (pos < size_) {
    const uint8_t blockSize = data_[pos++];
    if (blockSize == 0) return pos;
    if (pos + blockSize > size_) return -1;
    pos += blockSize;
  }
  return -1;
}

auto GifDecoder::parse() -> bool {
  if (size_ < 13) {
    lastError_ = GifError::INVALID_FILE;
    return false;
  }

  if (!(std::memcmp(data_, "GIF87a", 6) == 0 || std::memcmp(data_, "GIF89a", 6) == 0)) {
    lastError_ = GifError::INVALID_FILE;
    return false;
  }

  int32_t pos                  = 6;
  const uint16_t logicalWidth  = readU16(pos);
  const uint16_t logicalHeight = readU16(pos + 2);
  const uint8_t packed         = data_[pos + 4];
  pos += 7;

  if (logicalWidth == 0 || logicalHeight == 0) {
    lastError_ = GifError::INVALID_FILE;
    return false;
  }

  const bool hasGlobalColorTable = (packed & 0x80U) != 0;
  const int32_t globalTableSize  = 1 << ((packed & 0x07U) + 1);

  if (hasGlobalColorTable) {
    if (pos + (globalTableSize * 3) > size_) {
      lastError_ = GifError::INVALID_FILE;
      return false;
    }

    for (int32_t i = 0; i < globalTableSize; ++i) {
      paletteGray_[i] = toGray(data_[pos], data_[pos + 1], data_[pos + 2]);
      pos += 3;
    }
    for (int32_t i = globalTableSize; i < 256; ++i) paletteGray_[i] = 0;
  }

  while (pos < size_) {
    const uint8_t marker = data_[pos++];

    if (marker == GIF_TRAILER) {
      break;
    }

    if (marker == GIF_EXTENSION) {
      if (pos >= size_) {
        lastError_ = GifError::INVALID_FILE;
        return false;
      }

      const uint8_t label = data_[pos++];

      if (label == GIF_GRAPHIC_CONTROL) {
        if (pos >= size_) {
          lastError_ = GifError::INVALID_FILE;
          return false;
        }

        const uint8_t blockSize = data_[pos++];
        if (blockSize != 4 || pos + 4 > size_) {
          lastError_ = GifError::INVALID_FILE;
          return false;
        }

        const uint8_t gcePacked  = data_[pos];
        const bool hasTrans      = (gcePacked & 0x01U) != 0;
        const uint8_t transIndex = data_[pos + 3];
        hasTransparentIndex_     = hasTrans;
        transparentIndex_        = transIndex;

        pos += 4;
        if (pos >= size_ || data_[pos] != 0) {
          lastError_ = GifError::INVALID_FILE;
          return false;
        }
        pos += 1;
      } else {
        pos = skipSubBlocks(pos);
        if (pos < 0) {
          lastError_ = GifError::INVALID_FILE;
          return false;
        }
      }

      continue;
    }

    if (marker == GIF_IMAGE_DESCRIPTOR) {
      if (pos + 9 > size_) {
        lastError_ = GifError::INVALID_FILE;
        return false;
      }

      const uint16_t imageWidth     = readU16(pos + 4);
      const uint16_t imageHeight    = readU16(pos + 6);
      const uint8_t imagePacked     = data_[pos + 8];
      const bool hasLocalColorTable = (imagePacked & 0x80U) != 0;
      interlaced_                   = (imagePacked & 0x40U) != 0;
      pos += 9;

      if (imageWidth == 0 || imageHeight == 0) {
        lastError_ = GifError::INVALID_FILE;
        return false;
      }

      if (hasLocalColorTable) {
        const int32_t localTableSize = 1 << ((imagePacked & 0x07U) + 1);
        if (pos + (localTableSize * 3) > size_) {
          lastError_ = GifError::INVALID_FILE;
          return false;
        }

        for (int32_t i = 0; i < localTableSize; ++i) {
          paletteGray_[i] = toGray(data_[pos], data_[pos + 1], data_[pos + 2]);
          pos += 3;
        }
        for (int32_t i = localTableSize; i < 256; ++i) paletteGray_[i] = 0;
      }

      if (pos >= size_) {
        lastError_ = GifError::INVALID_FILE;
        return false;
      }

      lzwMinCodeSize_ = data_[pos++];
      if (lzwMinCodeSize_ < 2 || lzwMinCodeSize_ > 8) {
        lastError_ = GifError::UNSUPPORTED_FEATURE;
        return false;
      }

      width_        = imageWidth;
      height_       = imageHeight;
      imageDataPos_ = pos;
      outputWidth_  = width_;
      outputHeight_ = height_;
      lastError_    = GifError::SUCCESS;
      return true;
    }

    lastError_ = GifError::INVALID_FILE;
    return false;
  }

  lastError_ = GifError::INVALID_FILE;
  return false;
}

auto GifDecoder::flushRowIfNeeded(uint8_t *rowIndexBuf, int32_t srcY, int32_t x, int32_t y,
                                  int32_t scale, uint8_t *outGrayRow, GifDraw &draw) -> bool {
  if ((srcY % scale) != 0) return true;

  int32_t outX = 0;
  for (int32_t srcX = 0; srcX < width_; srcX += scale) {
    uint8_t index = rowIndexBuf[srcX];
    if (hasTransparentIndex_ && index == transparentIndex_) {
      outGrayRow[outX++] = 0xff;
    } else {
      outGrayRow[outX++] = paletteGray_[index];
    }
  }

  draw.x       = x;
  draw.y       = y + (srcY / scale);
  draw.iWidth  = outputWidth_;
  draw.iHeight = 1;
  draw.iBpp    = 8;
  draw.pPixels = outGrayRow;
  draw.pUser   = user_;

  return drawCb_(&draw) != 0;
}

auto GifDecoder::decodeLzwRows(int32_t x, int32_t y, int32_t scale) -> bool {
  SubBlockReader reader;
  if (!reader.init(data_, size_, imageDataPos_)) {
    lastError_ = GifError::INVALID_FILE;
    return false;
  }

  auto rowIndexBuf = std::make_unique<uint8_t[]>(width_);
  auto outGrayRow  = std::make_unique<uint8_t[]>(outputWidth_);
  if (rowIndexBuf == nullptr || outGrayRow == nullptr) {
    lastError_ = GifError::DECODE_ERROR;
    return false;
  }

  std::array<uint16_t, LZW_MAX_CODE> prefix{};
  std::array<uint8_t, LZW_MAX_CODE> suffix{};
  std::array<uint8_t, LZW_MAX_CODE + 1> stack{};

  const int32_t clearCode = 1 << lzwMinCodeSize_;
  const int32_t endCode   = clearCode + 1;

  int32_t codeSize = lzwMinCodeSize_ + 1;
  int32_t nextCode = endCode + 1;

  auto resetDictionary = [&]() {
    codeSize = lzwMinCodeSize_ + 1;
    nextCode = endCode + 1;
  };

  uint32_t bitBuffer = 0;
  int32_t bitCount   = 0;

  auto readCode = [&](int32_t bits, int32_t &code) -> bool {
    while (bitCount < bits) {
      uint8_t byte = 0;
      if (!reader.nextByte(byte)) return false;
      bitBuffer |= static_cast<uint32_t>(byte) << bitCount;
      bitCount += 8;
    }

    code = static_cast<int32_t>(bitBuffer & ((1U << bits) - 1U));
    bitBuffer >>= bits;
    bitCount -= bits;
    return true;
  };

  int32_t rowX          = 0;
  int32_t rowsDecoded   = 0;
  int32_t interlaceY    = 0;
  int32_t interlacePass = 0;
  int32_t oldCode       = -1;
  uint8_t firstChar     = 0;

  GifDraw draw{};

  static constexpr int32_t passStart[4] = {0, 4, 2, 1};
  static constexpr int32_t passStep[4]  = {8, 8, 4, 2};

  resetDictionary();

  auto emitPixel = [&](uint8_t colorIndex) -> bool {
    rowIndexBuf[rowX++] = colorIndex;
    if (rowX < width_) return true;

    int32_t srcY = rowsDecoded;
    if (interlaced_) {
      srcY = interlaceY;
    }

    if (srcY >= 0 && srcY < height_) {
      if (!flushRowIfNeeded(rowIndexBuf.get(), srcY, x, y, scale, outGrayRow.get(), draw)) {
        return false;
      }
    }

    rowX = 0;
    rowsDecoded++;

    if (interlaced_) {
      interlaceY += passStep[interlacePass];
      while (interlacePass < 4 && interlaceY >= height_) {
        interlacePass++;
        if (interlacePass < 4) interlaceY = passStart[interlacePass];
      }
    }

    return rowsDecoded < height_;
  };

  while (true) {
    int32_t code = 0;
    if (!readCode(codeSize, code)) break;

    if (code == clearCode) {
      resetDictionary();
      oldCode = -1;
      continue;
    }

    if (code == endCode) {
      break;
    }

    if (code < 0 || code >= LZW_MAX_CODE) {
      lastError_ = GifError::DECODE_ERROR;
      return false;
    }

    int32_t inCode = code;
    int32_t top    = 0;

    if (oldCode == -1) {
      if (code >= clearCode) {
        lastError_ = GifError::DECODE_ERROR;
        return false;
      }

      firstChar = static_cast<uint8_t>(code);
      if (!emitPixel(firstChar)) {
        if (rowsDecoded >= height_) return true;
        lastError_ = GifError::DECODE_ERROR;
        return false;
      }
      oldCode = code;
      continue;
    }

    if (code >= nextCode) {
      if (code != nextCode || oldCode < 0) {
        lastError_ = GifError::DECODE_ERROR;
        return false;
      }
      stack[top++] = firstChar;
      code         = oldCode;
    }

    while (code >= clearCode) {
      if (code >= LZW_MAX_CODE || top >= LZW_MAX_CODE) {
        lastError_ = GifError::DECODE_ERROR;
        return false;
      }
      stack[top++] = suffix[code];
      code         = prefix[code];
    }

    firstChar    = static_cast<uint8_t>(code);
    stack[top++] = firstChar;

    while (top > 0) {
      const uint8_t idx = stack[--top];
      if (!emitPixel(idx)) {
        if (rowsDecoded >= height_) return true;
        lastError_ = GifError::DECODE_ERROR;
        return false;
      }
    }

    if (nextCode < LZW_MAX_CODE) {
      prefix[nextCode] = static_cast<uint16_t>(oldCode);
      suffix[nextCode] = firstChar;
      nextCode++;
      if (nextCode == (1 << codeSize) && codeSize < 12) codeSize++;
    }

    oldCode = inCode;

    if (rowsDecoded >= height_) break;
  }

  if (rowsDecoded < height_) {
    lastError_ = GifError::DECODE_ERROR;
    return false;
  }

  return true;
}
