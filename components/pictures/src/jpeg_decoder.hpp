#pragma once

#include <bitset>
#include <cinttypes>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off

/* Defines and variables */
#define FILE_HIGHWATER      1536
#define JPEG_FILE_BUF_SIZE  2048
#define HUFF_TABLEN          273
#define HUFF11SIZE     (1 << 11)
#define DC_TABLE_SIZE       1024
#define DCTSIZE               64
#define MAX_MCU_COUNT          6
#define MAX_COMPS_IN_SCAN      4
#define MAX_BUFFERED_PIXELS 2048
#define MCU_SKIP              -8

// Decoder options
enum class JPegOption : uint8_t{
  AUTO_ROTATE,
  SCALE_HALF,
  SCALE_QUARTER,
  SCALE_EIGHTH,
  LE_PIXELS,
  EXIF_THUMBNAIL,
  LUMA_ONLY,
  USES_DMA,
  Count
};

using JPegOptions = std::bitset<static_cast<uint8_t>(JPegOption::Count)>;

// Helper macro to convert enum class values to their underlying integer type 
// for bitset indexing.
#define JPEG_OPTION(opt) static_cast<uint8_t>(JPegOption::opt)

// clang-format on

#define MCU0 (DCTSIZE * 0)
#define MCU1 (DCTSIZE * 1)
#define MCU2 (DCTSIZE * 2)
#define MCU3 (DCTSIZE * 3)
#define MCU4 (DCTSIZE * 4)
#define MCU5 (DCTSIZE * 5)

// Supported decode modes
enum class JPegImageMode {
  BASELINE, PROGRESSIVE
};

enum class JPegPixelType {
  EIGHT_BIT_GRAYSCALE, FOUR_BIT_DITHERED, TWO_BIT_DITHERED, ONE_BIT_DITHERED
};

enum class JPegMemType {
  RAM, FLASH
};

enum class JPegError {
  SUCCESS, INVALID_PARAM, DECODE_ERROR, UNSUPPORTED_FEATURE, INVALID_FILE, JPEG_ERROR_MEMORY
};

struct BufferedBits {
  uint8_t *pBuf{nullptr}; // buffer pointer
  uint32_t ulBits{0};     // buffered bits
  uint32_t ulBitOff{0};   // current bit offset
  BufferedBits()  = default;
  ~BufferedBits() = default;
};

struct JPegFile {
  int32_t iPos{0};         // current file position
  int32_t iSize{0};        // file size
  uint8_t *pData{nullptr}; // memory file pointer
  void *fHandle{nullptr};  // class pointer to File/SdFat or whatever you want
  JPegFile()  = default;
  ~JPegFile() = default;
};

struct JPegDraw {
  int32_t x{0}, y{0};            // upper left corner of current MCU
  int32_t iWidth{0}, iHeight{0}; // size of this pixel block
  int32_t iWidthUsed{0};         // clipped size for odd/edges
  int32_t iBpp{0};               // bit depth of the pixels (8 or 16)
  uint16_t *pPixels{nullptr};    // 16-bit pixels
  void *pUser{nullptr};
  JPegDraw()  = default;
  ~JPegDraw() = default;
};

// Callback function prototypes
using JPegReadCallback  = int32_t (*)(JPegFile *pFile, uint8_t *pBuf, int32_t iLen);
using JPegSeekCallback  = int32_t (*)(JPegFile *pFile, int32_t iPosition);
using JPegDrawCallback  = int32_t (*)(JPegDraw *pDraw);
using JPegOpenCallback  = void *(*)(const char *szFilename, int32_t *pFileSize);
using JPegCloseCallback = void (*)(void *pHandle);

/* JPEG color component info */
struct JPegCompInfo {
  // These values are fixed over the whole image
  // For compression, they must be supplied by the user interface
  // for decompression, they are read from the SOF marker.
  uint8_t component_needed{0}; /*  do we need the value of this component? */
  uint8_t component_id{0};     /* identifier for this component (0..255) */
  uint8_t component_index{0};  /* its index in SOF or cinfo->comp_info[] */
  // uint8_t h_samp_factor;    /* horizontal sampling factor (1..4) */
  // uint8_t v_samp_factor;    /* vertical sampling factor (1..4) */
  uint8_t quant_tbl_no{0}; /* quantization table selector (0..3) */
  // These values may vary between scans
  // For compression, they must be supplied by the user interface
  // for decompression, they are read from the SOS marker.
  uint8_t dc_tbl_no{0}; /* DC entropy table selector (0..3) */
  uint8_t ac_tbl_no{0}; /* AC entropy table selector (0..3) */
  // These values are computed during compression or decompression startup
  // int32_t true_comp_width;  /* component's image width in samples */
  // int32_t true_comp_height; /* component's image height in samples */
  // the above are the logical dimensions of the downsampled image
  // These values are computed before starting a scan of the component
  // int32_t MCU_width;        /* number of blocks per MCU, horizontally */
  // int32_t MCU_height;       /* number of blocks per MCU, vertically */
  // int32_t MCU_blocks;       /* MCU_width * MCU_height */
  // int32_t downsampled_width; /* image width in samples, after expansion */
  // int32_t downsampled_height; /* image height in samples, after expansion */
  // the above are the true_comp_xxx values rounded up to multiples of
  // the MCU dimensions; these are the working dimensions of the array
  // as it is passed through the DCT or IDCT step.  NOTE: these values
  // differ depending on whether the component is interleaved or not!!
  // This flag is used only for decompression.  In cases where some of the
  // components will be ignored (eg grayscale output from YCbCr image),
  // we can skip IDCT etc. computations for the unused components.
  JPegCompInfo()  = default;
  ~JPegCompInfo() = default;
};

class JPegDecoder;

//
// our private structure to hold a JPEG image decode state
//
class JPegImage {
  friend JPegDecoder;

public:
  JPegImage()                                      = default;
  ~JPegImage()                                     = default;
  JPegImage(const JPegImage &)                     = delete;
  auto operator=(const JPegImage &) -> JPegImage & = delete;
  JPegImage(JPegImage &&)                          = default;
  auto operator=(JPegImage &&) -> JPegImage &      = default;

  [[nodiscard]] inline auto getOrientation() const -> uint8_t { return ucOrientation; }
  [[nodiscard]] inline auto getLastError() const -> JPegError { return iError; }
  [[nodiscard]] inline auto getWidth() const -> int32_t { return iWidth; }
  [[nodiscard]] inline auto getHeight() const -> int32_t { return iHeight; }
  [[nodiscard]] inline auto hasThumb() const -> uint8_t { return ucHasThumb; }
  [[nodiscard]] inline auto getThumbWidth() const -> int32_t { return iThumbWidth; }
  [[nodiscard]] inline auto getThumbHeight() const -> int32_t { return iThumbHeight; }
  [[nodiscard]] inline auto getBpp() const -> uint8_t { return ucBpp; }
  [[nodiscard]] inline auto getSubSample() const -> uint8_t { return ucSubSample; }
  [[nodiscard]] inline auto getPixelType() const -> JPegPixelType { return ucPixelType; }
  [[nodiscard]] inline auto getJPegMode() const -> JPegImageMode {
    return (ucMode == 0xc2) ? JPegImageMode::PROGRESSIVE : JPegImageMode::BASELINE;
  }

protected:
  int32_t iWidth{0}, iHeight{0};                        // image size
  int32_t iThumbWidth{0}, iThumbHeight{0};              // thumbnail size (if present)
  int32_t iThumbData{0};                                // offset to image data
  int32_t iXOffset{0}, iYOffset{0};                     // placement on the display
  int32_t iCropX{0}, iCropY{0}, iCropCX{0}, iCropCY{0}; // crop area
  uint8_t ucBpp{0}, ucSubSample{0}, ucHuffTableUsed{0};
  uint8_t ucMode{0}, ucOrientation{0}, ucHasThumb{0}, b11Bit{0};
  uint8_t ucComponentsInScan{0}, cApproxBitsLow{0}, cApproxBitsHigh{0};
  uint8_t iScanStart{0}, iScanEnd{0}, ucFF{0}, ucNumComponents{0};
  uint8_t ucACTable{0}, ucDCTable{0};
  JPegMemType ucMemType{JPegMemType::RAM};
  JPegPixelType ucPixelType{JPegPixelType::EIGHT_BIT_GRAYSCALE};
  uint16_t u16MCUFlags{0};
  int32_t iEXIF{0}; // Offset to EXIF 'TIFF' file
  JPegError iError{JPegError::SUCCESS};
  JPegOptions iOptions{0};
  int32_t iVLCOff{0};                    // current VLC data offset
  int32_t iVLCSize{0};                   // current quantity of data in the VLC buffer
  int32_t iResInterval{0}, iResCount{0}; // restart interval
  int32_t iMaxMCUs{0};                   // max MCUs of pixels per JPEGDraw call
  JPegReadCallback pfnRead{nullptr};
  JPegSeekCallback pfnSeek{nullptr};
  JPegDrawCallback pfnDraw{nullptr};
  JPegOpenCallback pfnOpen{nullptr};
  JPegCloseCallback pfnClose{nullptr};
  JPegCompInfo compInfo[MAX_COMPS_IN_SCAN]; /* Max color components */
  JPegFile jpegFile{};
  BufferedBits bb{};
  void *pUser{nullptr};

  // provided externally to do Floyd-Steinberg dithering
  std::unique_ptr<uint8_t[]> pDitherBuffer{nullptr};

  uint16_t *usPixels{nullptr}; // needs to be 16-byte aligned for S3 SIMD
  uint16_t usUnalignedPixels[MAX_BUFFERED_PIXELS + 8]{0};
  int16_t *sMCUs{nullptr}; // needs to be 16-byte aligned for S3 SIMD
  int16_t sUnalignedMCUs[8 + (DCTSIZE * MAX_MCU_COUNT)]{0}; // 4:2:0 needs 6 DCT blocks per MCU
  void *pFramebuffer{
      nullptr}; // if null, we send data to the user callback; if not null, it's a pointer to a
                // full-width framebuffer that we write directly to (for faster drawing)
  int16_t sQuantTable[DCTSIZE * 4]{0};      // quantization tables
  uint8_t ucFileBuf[JPEG_FILE_BUF_SIZE]{0}; // holds temp data and pixel stack
  uint8_t ucHuffDC[DC_TABLE_SIZE * 2]{0};   // up to 2 'short' tables
  uint16_t usHuffAC[HUFF11SIZE * 2]{0};

private:
  static constexpr int32_t iBitMasks[33] = {
      0,         1,          3,          7,          0xf,       0x1f,      0x3f,
      0x7f,      0xff,       0x1ff,      0x3ff,      0x7ff,     0x0fff,    0x1fff,
      0x3fff,    0x7fff,     0xffff,     0x1ffff,    0x3ffff,   0x7ffff,   0xfffff,
      0x1fffff,  0x3fffff,   0x7fffff,   0xffffff,   0x1ffffff, 0x3ffffff, 0x7ffffff,
      0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 1};
  // zigzag ordering of DCT coefficients
  static constexpr uint8_t cZigZag[64] = {
      0,  1,  5,  6,  14, 15, 27, 28, 2,  4,  7,  13, 16, 26, 29, 42, 3,  8,  12, 17, 25, 30,
      41, 43, 9,  11, 18, 24, 31, 40, 44, 53, 10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38,
      46, 51, 55, 60, 21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63};

  // un-zigzag ordering
  static constexpr uint8_t cZigZag2[64] = {
      0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
      41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
      30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

  // For AA&N IDCT method, multipliers are equal to quantization
  // coefficients scaled by scalefactor[row]*scalefactor[col], where
  // scalefactor[0] = 1
  // scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
  // For integer operation, the multiplier table is to be scaled by
  // IFAST_SCALE_BITS.
  static constexpr int32_t iScaleBits[64] = {
      16384, 22725, 21407, 19266, 16384, 12873, 8867,  4520,  22725, 31521, 29692, 26722, 22725,
      17855, 12299, 6270,  21407, 29692, 27969, 25172, 21407, 16819, 11585, 5906,  19266, 26722,
      25172, 22654, 19266, 15137, 10426, 5315,  16384, 22725, 21407, 19266, 16384, 12873, 8867,
      4520,  12873, 17855, 16819, 15137, 12873, 10114, 6967,  3552,  8867,  12299, 11585, 10426,
      8867,  6967,  4799,  2446,  4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247};
  //
  // Range clip and shift for RGB565 output
  // input value is 0 to 255, then another 256 for overflow to FF, then 512 more for negative values
  // wrapping around Trims a few instructions off the final output stage
  //
  static constexpr uint8_t ucRangeTable[1024] = {
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
      0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d,
      0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac,
      0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb,
      0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
      0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
      0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
      0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
      0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
      0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
      0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
      0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
      0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
      0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
      0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b,
      0x7c, 0x7d, 0x7e, 0x7f};

  static auto readRAM(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t;
  static auto readFLASH(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t;
  static auto seekMem(JPegFile *pFile, int32_t iPosition) -> int32_t;
  static auto closeFile(void *handle) -> void;
  static auto seekFile(JPegFile *pFile, int32_t iPosition) -> int32_t;
  static auto openFile(const char *szFilename, int32_t *pFileSize) -> void *;
  static auto readFile(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t;

  auto init() -> int32_t;
  auto getHuffTables(uint8_t *pBuf, int32_t iLen) -> int32_t;
  auto makeHuffTables(int32_t bThumbnail) -> int32_t;
  auto tiffShort(uint8_t *p, int32_t bMotorola) -> uint16_t;
  auto tiffLong(uint8_t *p, int32_t bMotorola) -> uint32_t;
  auto tiffValue(uint8_t *p, int32_t bMotorola) -> int32_t;
  auto getTiffInfo(int32_t bMotorola, int32_t iOffset) -> void;
  auto getSOS(int32_t *iOff) -> int32_t;
  auto filter(uint8_t *pBuf, uint8_t *d, int32_t iLen, uint8_t *bFF) -> int32_t;
  auto getMoreData() -> void;
  auto parseInfo(int32_t bExtractThumb) -> int32_t;
  auto fixQuantD() -> void;
  auto decodeMCUProgressive(int32_t iMCU, int32_t *iDCPredictor) -> int32_t;
  auto decodeMCU(int32_t iMCU, int32_t *iDCPredictor) -> int32_t;
  auto inverseDCT(int32_t iMCUOffset, int32_t iQuantTable) -> void;
  auto putMCU8BitGray(int32_t x, int32_t iPitch) -> void;
  auto ditherImage(int32_t iWidth, int32_t iHeight) -> void;
  auto decodeImage() -> int32_t;
};

#define JPEG_STATIC static
//
// The JPegDecoder class wraps portable C code which does the actual work
//
class JPegDecoder {
public:
  auto openRAM(uint8_t *pData, int32_t iDataSize, JPegDrawCallback pfnDraw) -> int32_t;
  auto openFLASH(const uint8_t *pData, int32_t iDataSize, JPegDrawCallback pfnDraw) -> int32_t;
  auto open(const char *szFilename, JPegOpenCallback pfnOpen, JPegCloseCallback pfnClose,
            JPegReadCallback pfnRead, JPegSeekCallback pfnSeek, JPegDrawCallback pfnDraw)
      -> int32_t;
  auto open(const char *szFilename, JPegDrawCallback pfnDraw) -> int32_t;
  auto open(void *fHandle, int32_t iDataSize, JPegCloseCallback pfnClose, JPegReadCallback pfnRead,
            JPegSeekCallback pfnSeek, JPegDrawCallback pfnDraw) -> int32_t;
  auto setFramebuffer(void *pFramebuffer) -> void;
  auto setCropArea(int32_t x, int32_t y, int32_t w, int32_t h) -> void;
  auto getCropArea(int32_t *x, int32_t *y, int32_t *w, int32_t *h) const -> void;

  auto close() -> void;
  auto decode(int32_t x, int32_t y, JPegOptions iOptions) -> int32_t;
  auto decodeDither(std::unique_ptr<uint8_t[]> pDither, JPegOptions iOptions) -> int32_t;
  auto decodeDither(int32_t x, int32_t y, std::unique_ptr<uint8_t[]> pDither, JPegOptions iOptions)
      -> int32_t;
  auto setUserPointer(void *p) -> void;
  auto setPixelType(JPegPixelType iType) -> void; // defaults to little endian
  auto setMaxOutputSize(int32_t iMaxMCUs) -> void;

  [[nodiscard]] inline auto getOrientation() const -> uint8_t { return image.getOrientation(); }
  [[nodiscard]] inline auto getLastError() const -> JPegError { return image.getLastError(); }
  [[nodiscard]] inline auto getWidth() const -> int32_t { return image.getWidth(); }
  [[nodiscard]] inline auto getHeight() const -> int32_t { return image.getHeight(); }
  [[nodiscard]] inline auto hasThumb() const -> uint8_t { return image.hasThumb(); }
  [[nodiscard]] inline auto getThumbWidth() const -> int32_t { return image.getThumbWidth(); }
  [[nodiscard]] inline auto getThumbHeight() const -> int32_t { return image.getThumbHeight(); }
  [[nodiscard]] inline auto getBpp() const -> uint8_t { return image.getBpp(); }
  [[nodiscard]] inline auto getSubSample() const -> uint8_t { return image.getSubSample(); }
  [[nodiscard]] inline auto getJPegMode() const -> JPegImageMode { return image.getJPegMode(); }
  [[nodiscard]] inline auto getPixelType() const -> JPegPixelType { return image.getPixelType(); }

private:
  JPegImage image;
};
