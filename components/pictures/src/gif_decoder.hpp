#pragma once

#include <bitset>
#include <cstdint>

// Decoder options
enum class GifOption : uint8_t {
  SCALE_HALF, SCALE_QUARTER, SCALE_EIGHTH, Count
};

using GifOptions = std::bitset<static_cast<uint8_t>(GifOption::Count)>;

#define GIF_OPTION(opt) static_cast<uint8_t>(GifOption::opt)

enum class GifError : uint8_t {
  SUCCESS, INVALID_PARAM, INVALID_FILE, UNSUPPORTED_FEATURE, DECODE_ERROR
};

struct GifDraw {
  int32_t x{0};
  int32_t y{0};
  int32_t iWidth{0};
  int32_t iHeight{0};
  int32_t iBpp{8};
  uint8_t *pPixels{nullptr};
  void *pUser{nullptr};
};

using GifDrawCallback = int32_t (*)(GifDraw *pDraw);

class GifDecoder {
public:
  auto openRAM(const uint8_t *pData, int32_t iDataSize, GifDrawCallback pfnDraw) -> int32_t;
  auto close() -> void;

  auto decode(int32_t x, int32_t y, GifOptions iOptions = {}) -> int32_t;

  auto setUserPointer(void *p) -> void;

  [[nodiscard]] auto getLastError() const -> GifError;
  [[nodiscard]] auto getWidth() const -> int32_t;
  [[nodiscard]] auto getHeight() const -> int32_t;
  [[nodiscard]] auto getOutputWidth() const -> int32_t;
  [[nodiscard]] auto getOutputHeight() const -> int32_t;

private:
  static constexpr int32_t LZW_MAX_CODE = 4096;

  struct SubBlockReader {
    const uint8_t *data{nullptr};
    int32_t size{0};
    int32_t pos{0};
    int32_t remaining{0};
    bool finished{false};

    auto init(const uint8_t *pData, int32_t iSize, int32_t iPos) -> bool;
    auto nextByte(uint8_t &value) -> bool;
  };

  auto reset() -> void;
  auto parse() -> bool;
  auto skipSubBlocks(int32_t startPos) -> int32_t;
  auto readU16(int32_t offset) const -> uint16_t;
  auto decodeLzwRows(int32_t x, int32_t y, int32_t scale) -> bool;

  auto flushRowIfNeeded(uint8_t *rowIndexBuf, int32_t srcY, int32_t x, int32_t y, int32_t scale,
                        uint8_t *outGrayRow, GifDraw &draw) -> bool;

private:
  const uint8_t *data_{nullptr};
  int32_t size_{0};
  GifDrawCallback drawCb_{nullptr};
  void *user_{nullptr};

  int32_t width_{0};
  int32_t height_{0};
  int32_t outputWidth_{0};
  int32_t outputHeight_{0};

  int32_t imageDataPos_{0};
  uint8_t lzwMinCodeSize_{0};
  bool interlaced_{false};
  uint8_t transparentIndex_{0};
  bool hasTransparentIndex_{false};

  uint8_t paletteGray_[256]{};

  GifError lastError_{GifError::SUCCESS};
};
