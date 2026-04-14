#pragma once

#include <cstring>
#include <fstream>
#include <iostream>

#include "global.hpp"
#include "models/font.hpp"
#include "sys/stat.h"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"

/**
 * @brief Access to a IBMF font.
 *
 * This is a class to allow acces to a IBMF font generated from METAFONT
 *
 */
class IBMFFont {
public:
  using FIX16 = int16_t;

  #pragma pack(push, 1)
  struct LigKernStep {
    unsigned int nextCharCode : 8;
    union {
      unsigned int charCode : 8;
      unsigned int kernIdx : 8; // Ligature: replacement char code, kern: displacement
    } u;
    unsigned int stop : 1;
    unsigned int tag : 1; // 0 = Ligature, 1 = Kern
    unsigned int filler : 6;
  };

  struct GlyphMetric {
    unsigned int dynF : 4;
    unsigned int firstIsBlack : 1;
    unsigned int filler : 3;
  };

  struct GlyphInfo {
    uint8_t charCode;
    uint8_t bitmapWidth;
    uint8_t bitmapHeight;
    int8_t horizontalOffset;
    int8_t verticalOffset;
    uint8_t ligKernPgmIndex; // = 255 if none
    uint16_t packetLength;
    FIX16 advance;
    GlyphMetric glyphMetric;
  };
  #pragma pack(pop)

private:
  static constexpr char const *TAG = "IBMFFont";

  static constexpr uint8_t MAX_GLYPH_COUNT = 256;
  static constexpr uint8_t IBMF_VERSION    = 1;

  Font &font;

  bool initialized;
  bool memoryOwnerIsTheInstance;

  #pragma pack(push, 1)
  struct Preamble {
    char marker[4];
    uint8_t sizeCount;
    struct {
      uint8_t version : 5;
      uint8_t charSet : 3;
    } bits;
    uint16_t fontOffsets[1];
  };

  struct Header {
    uint8_t pointSize;
    uint8_t lineHeight;
    uint16_t dpi;
    FIX16 xHeight;
    FIX16 emSize;
    FIX16 slantCorrection;
    uint8_t descenderHeight;
    uint8_t spaceSize;
    uint8_t glyphCount;
    uint8_t ligKernPgmCount;
    uint8_t kernCount;
    uint8_t version;
  };
  #pragma pack(pop)

  uint8_t *memory;
  uint32_t memoryLength;

  uint8_t *memoryPtr;
  uint8_t *memoryEnd;

  uint32_t repeatCount;

  Preamble *preamble;
  uint8_t *sizes;

  uint8_t *currentFont;

  Header *header;
  GlyphInfo *glyphInfoTable[MAX_GLYPH_COUNT];
  LigKernStep *ligKernPgm;
  FIX16 *kerns;

  static constexpr uint8_t PK_REPEAT_COUNT = 14;
  static constexpr uint8_t PK_REPEAT_ONCE  = 15;

  GlyphInfo *glyphInfo;
  Glyph glyph;

  auto getnext8(uint8_t &val) -> bool {
    if (memoryPtr >= memoryEnd) return false;
    val = *memoryPtr++;
    return true;
  }

  uint8_t nybbleFlipper = 0xf0U;
  uint8_t nybbleByte;

  auto getNybble(uint8_t &nyb) -> bool {
    if (nybbleFlipper == 0xf0U) {
      if (!getnext8(nybbleByte)) return false;
      nyb = nybbleByte >> 4;
    } else {
      nyb = (nybbleByte & 0x0f);
    }
    nybbleFlipper ^= 0xffU;
    return true;
  }

  // Pseudo-code:
  //
  // function pk_packed_num: integer;
  // var i,j,k: integer;
  // begin
  //   i := get_nyb;
  //   if i = 0 then begin
  //     repeat
  //       j := getnyb; incr(i);
  //     until j != 0;
  //     while i > 0 do begin
  //       j := j * 16 + get_nyb;
  //       decr(i);
  //     end;
  //     pk_packed_num := j - 15 + (13 - dynF) * 16 + dynF;
  //   end
  //   else if i <= dynF then
  //     pk_packed_num := i
  //   else if i < 14 then
  //     pk_packed_num := (i - dynF - 1) * 16 + get_nyb + dynF + 1
  //   else begin
  //     if repeatCount != 0 then abort('Extra repeat count!');
  //     if i = 14 then
  //        repeatCount := pk_packed_num
  //     else
  //        repeatCount := 1;
  //     send_out(true, repeatCount);
  //     pk_packed_num := pk_packed_num;
  //   end;
  // end;

  auto getPackedNumber(uint32_t &val, const GlyphInfo &glyph) -> bool {
    uint8_t nyb;
    uint32_t i, j, k;

    uint8_t dynF = glyph.glyphMetric.dynF;

    while (true) {
      if (!getNybble(nyb)) return false;
      i = nyb;
      if (i == 0) {
        do {
          if (!getNybble(nyb)) return false;
          i++;
        } while (nyb == 0);
        j = nyb;
        while (i-- > 0) {
          if (!getNybble(nyb)) return false;
          j = (j << 4) + nyb;
        }
        val = j - 15 + ((13 - dynF) << 4) + dynF;
        break;
      } else if (i <= dynF) {
        val = i;
        break;
      } else if (i < PK_REPEAT_COUNT) {
        if (!getNybble(nyb)) return false;
        val = ((i - dynF - 1) << 4) + nyb + dynF + 1;
        break;
      } else {
        // if (repeatCount != 0) {
        //   std::cerr << "Spurious repeatCount iteration!" << std::endl;
        //   return false;
        // }
        if (i == PK_REPEAT_COUNT) {
          if (!getPackedNumber(repeatCount, glyph)) return false;
        } else { // i == PK_REPEAT_ONCE
          repeatCount = 1;
        }
      }
    }
    return true;
  }

  auto retrieveBitmap(GlyphInfo *glyph, uint8_t *bitmap, Dim8 dim, Pos8 offsets) -> bool {
    // point on the glyphs' bitmap definition
    memoryPtr = ((uint8_t *)glyph) + sizeof(GlyphInfo);
    uint8_t *rowp;

    if (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT) {
      uint32_t rowSize = (dim.width + 7) >> 3;
      rowp              = bitmap + (offsets.y * rowSize);

      if (glyph->glyphMetric.dynF == 14) { // is a bitmap?
        uint32_t count = 8;
        uint8_t data;

        for (uint32_t row = 0; row < glyph->bitmapHeight; row++, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < glyph->bitmapWidth + offsets.x; col++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                LOG_E("Not enough bitmap data!");
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            rowp[col >> 3] |= (data & (0x80U >> count)) ? (0x80U >> (col & 7)) : 0;
            count++;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount   = 0;
        nybbleFlipper = 0xf0U;

        bool black = !(glyph->glyphMetric.firstIsBlack == 1);

        for (uint32_t row = 0; row < glyph->bitmapHeight; row++, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < glyph->bitmapWidth + offsets.x; col++) {
            if (count == 0) {
              if (!getPackedNumber(count, *glyph)) {
                return false;
              }
              black = !black;
              // if (black) {
              //   std::cout << count << ' ';
              // }
              // else {
              //   std::cout << '(' << count << ')' << ' ';
              // }
            }
            if (black) rowp[col >> 3] |= (0x80U >> (col & 0x07));
            count--;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount << std::endl;
          while ((row < glyph->bitmapHeight) && (repeatCount-- > 0)) {
            bcopy(rowp, rowp + rowSize, rowSize);
            row++;
            rowp += rowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    } else {
      uint32_t rowSize = dim.width;
      rowp              = bitmap + (offsets.y * rowSize);

      repeatCount   = 0;
      nybbleFlipper = 0xf0U;

      if (glyph->glyphMetric.dynF == 14) { // is a bitmap?
        uint32_t count = 8;
        uint8_t data;

        for (uint32_t row = 0; row < (glyph->bitmapHeight); row++, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < (glyph->bitmapWidth + offsets.x); col++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                LOG_E("Not enough bitmap data!");
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            rowp[col] = (data & (0x80U >> count)) ? 0xFF : 0;
            count++;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount   = 0;
        nybbleFlipper = 0xf0U;

        bool black = !(glyph->glyphMetric.firstIsBlack == 1);

        for (uint32_t row = 0; row < (glyph->bitmapHeight); row++, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < (glyph->bitmapWidth + offsets.x); col++) {
            if (count == 0) {
              if (!getPackedNumber(count, *glyph)) {
                return false;
              }
              black = !black;
              // if (black) {
              //   std::cout << count << ' ';
              // }
              // else {
              //   std::cout << '(' << count << ')' << ' ';
              // }
            }
            if (black) rowp[col] = 0xFF;
            count--;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount << std::endl;
          while ((row < dim.height) && (repeatCount-- > 0)) {
            bcopy(rowp, rowp + rowSize, rowSize);
            row++;
            rowp += rowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    }
    return true;
  }

  auto loadPreamble() -> bool {
    preamble = (Preamble *)memory;
    if (strncmp("IBMF", preamble->marker, 4) != 0) return false;
    sizes        = (uint8_t *)&memory[6 + (preamble->sizeCount * 2)];
    currentFont = nullptr;

    return true;
  }

  auto loadData() -> bool {
    // for (uint8_t i = 0; i < MAX_GLYPH_COUNT; i++) glyphInfoTable[i] = nullptr;
    memset(glyphInfoTable, 0, sizeof(glyphInfoTable));

    uint8_t byte;
    bool result    = true;
    bool completed = false;

    memoryPtr = currentFont;

    header = (Header *)currentFont;
    if (header->version != IBMF_VERSION) return false;

    memoryPtr += sizeof(Header);
    for (int i = 0; i < header->glyphCount; i++) {
      glyphInfo                              = (GlyphInfo *)memoryPtr;
      glyphInfoTable[glyphInfo->charCode] = (GlyphInfo *)memoryPtr;
      memoryPtr += sizeof(GlyphInfo) + glyphInfo->packetLength;
      if (memoryPtr > memoryEnd) return false;
    }

    ligKernPgm = (LigKernStep *)memoryPtr;
    memoryPtr += sizeof(LigKernStep) * header->ligKernPgmCount;
    if (memoryPtr > memoryEnd) return false;

    kerns = (FIX16 *)memoryPtr;

    memoryPtr += sizeof(FIX16) * header->kernCount;
    if (memoryPtr > memoryEnd) return false;

    return true;
  }

public:
  IBMFFont(uint8_t *memoryFont, uint32_t size, Font &font)
      : memory(memoryFont), memoryLength(size), font(font) {

    memoryEnd                   = memory + memoryLength;
    initialized                  = loadPreamble();
    memoryOwnerIsTheInstance = false;
  }

  IBMFFont(const std::string filename, Font &font) : font(font) {
    struct stat fileStat;

    initialized = false;

    if (stat(filename.c_str(), &fileStat) != -1) {
      FILE *file                   = fopen(filename.c_str(), "rb");
      memory                       = new uint8_t[memoryLength = fileStat.st_size];
      memoryEnd                   = (memory == nullptr) ? nullptr : memory + memoryLength;
      memoryOwnerIsTheInstance = true;

      if (memory != nullptr) {
        if (fread(memory, memoryLength, 1, file) == 1) {
          initialized = loadPreamble();
        }
      }
      fclose(file);
    } else {
      std::cerr << "Unable to stat file %s!" << filename.c_str() << std::endl;
    }
  }

  ~IBMFFont() {
    if (memoryOwnerIsTheInstance && (memory != nullptr)) {
      delete[] memory;
      memory = nullptr;
    }
  }

  inline auto isInitialized() -> bool { return initialized; }
  inline auto getFontSize() -> uint8_t { return header->pointSize; }
  inline auto getLineHeight() -> uint16_t { return header->lineHeight; }
  inline auto getDescenderHeight() -> int16_t { return -static_cast<int16_t>(header->descenderHeight); }
  inline auto getCharSet() -> uint8_t { return preamble->bits.charSet; }
  inline auto getLigKern(uint8_t idx) -> LigKernStep * { return &ligKernPgm[idx]; }
  inline auto getKern(uint8_t i) -> FIX16 { return kerns[i]; }
  inline auto getGlyphInfo(uint8_t glyphCode) -> GlyphInfo * { return glyphInfoTable[glyphCode]; }

  auto getGlyph(uint32_t &glyphCode, Glyph &appGlyph, GlyphInfo **glyphData, bool loadBitmap) -> bool {
    uint8_t accent         = (glyphCode & 0x0000FF00) >> 8;
    GlyphInfo *accentInfo = accent ? glyphInfoTable[accent] : nullptr;

    if (((glyphCode & 0xFF) > MAX_GLYPH_COUNT) ||
        (glyphInfoTable[glyphCode & 0xFF] == nullptr)) {
      std::cerr << "No entry for glyph code " << +glyphCode << " 0x" << std::hex << +glyphCode
                << std::endl;
      return false;
    }

    memset(&glyph, 0, sizeof(Glyph));

    glyphInfo = glyphInfoTable[glyphCode & 0xFF];

    Dim8 dim           = Dim8(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
    Pos8 offsets       = Pos8(0, 0);
    uint8_t addedLeft = 0;

    if (accentInfo != nullptr) {
      offsets.x = ((glyphInfo->bitmapWidth > accentInfo->bitmapWidth)
                       ? ((glyphInfo->bitmapWidth - accentInfo->bitmapWidth) >> 1)
                       : 0) +
                  ((((int32_t)glyphInfo->bitmapHeight - (header->xHeight >> 6)) *
                    header->slantCorrection) >>
                   6);

      if (accentInfo->verticalOffset >= (header->xHeight >> 6)) {
        // Accents that are on top of a main glyph
        dim.height += (accentInfo->verticalOffset - (header->xHeight >> 6));
      } else if (accentInfo->verticalOffset < 5) {
        // Accents below the main glyph (cedilla)
        int16_t addedHeight = (glyphInfo->bitmapHeight - glyphInfo->verticalOffset) -
                               ((-accentInfo->verticalOffset) + accentInfo->bitmapHeight);
        if (addedHeight < 0) dim.height += -addedHeight;
        offsets.y = glyphInfo->verticalOffset - accentInfo->verticalOffset;
      }
      if (glyphInfo->bitmapWidth < accentInfo->bitmapWidth) {
        addedLeft = (accentInfo->bitmapWidth - glyphInfo->bitmapWidth) >> 1;
        dim.width  = accentInfo->bitmapWidth;
      }
      if (dim.width < (offsets.x + accentInfo->bitmapWidth)) {
        dim.width = offsets.x + accentInfo->bitmapWidth;
      }
    }

    uint16_t size = (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT)
                        ? dim.height * ((dim.width + 7) >> 3)
                        : dim.height * dim.width;
    glyph.buffer  = font.bytePoolAlloc(size);
    memset(glyph.buffer, 0, size);

    if (accentInfo != nullptr) {
      if (loadBitmap) retrieveBitmap(accentInfo, glyph.buffer, dim, offsets);

      offsets.y = (accentInfo->verticalOffset >= (header->xHeight >> 6))
                      ? (accentInfo->verticalOffset - (header->xHeight >> 6))
                      : 0;
      offsets.x = addedLeft;
    }

    if (loadBitmap) retrieveBitmap(glyphInfo, glyph.buffer, dim, offsets);

    glyph.dim     = Dim(dim.width, dim.height);
    glyph.xoff    = -(glyphInfo->horizontalOffset + offsets.x);
    glyph.yoff    = -(glyphInfo->verticalOffset + offsets.y);
    glyph.advance = glyphInfo->advance >> 6;
    glyph.pitch   = (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT)
                        ? (dim.width + 7) >> 3
                        : dim.width;
    glyph.ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex;
    glyph.lineHeight              = header->lineHeight;

    // bcopy(&glyphInfo, &glyph, sizeof(Glyph));
    appGlyph   = glyph;
    *glyphData = glyphInfo;

    if (accentInfo != nullptr) showGlyph(glyph, glyphCode);

    return true;
  }

  auto setFontSize(uint8_t size) -> bool {
    if (!initialized) return false;
    uint8_t i = 0;
    while ((i < preamble->sizeCount) && (sizes[i] <= size)) i++;
    if (i > 0) i--;
    currentFont = memory + preamble->fontOffsets[i];
    return loadData();
  }

  auto showGlyphInfo(const GlyphInfo &glyph, uint8_t charCode) -> bool {
    std::cout << "Glyph Char Code: " << charCode << std::endl
              << "  Metrics: [" << std::dec << glyph.bitmapWidth << ", " << glyph.bitmapHeight
              << "] " << glyph.packetLength << std::endl
              << "  Position: [" << glyph.horizontalOffset << ", " << glyph.verticalOffset << ']'
              << std::endl;
    return true;
  }

  auto showGlyph(const Glyph &glyph, uint32_t charCode) -> bool {
    std::cout << "Glyph Char Code: " << std::hex << charCode << std::dec << std::endl
              << "  Metrics: [" << std::dec << glyph.dim.width << ", " << glyph.dim.height << "] "
              << std::endl
              << "  Position: [" << glyph.xoff << ", " << glyph.yoff << ']' << std::endl
              << "  Bitmap available: " << ((glyph.buffer == nullptr) ? "No" : "Yes") << std::endl;

    if (glyph.buffer == nullptr) return true;

    uint32_t row, col;
    uint32_t rowSize = glyph.pitch;
    const uint8_t *rowPtr;

    std::cout << '+';
    for (col = 0; col < glyph.dim.width; col++) std::cout << '-';
    std::cout << '+' << std::endl;

    for (row = 0, rowPtr = glyph.buffer; row < glyph.dim.height; row++, rowPtr += rowSize) {
      std::cout << '|';
      for (col = 0; col < glyph.dim.width; col++) {
        std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
      }
      std::cout << '|';
      std::cout << std::endl;
    }

    std::cout << '+';
    for (col = 0; col < glyph.dim.width; col++) std::cout << '-';
    std::cout << '+' << std::endl << std::endl;

    return true;
  }
};