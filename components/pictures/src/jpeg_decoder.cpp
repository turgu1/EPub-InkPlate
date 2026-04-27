#include "jpeg_decoder.hpp"

#include <utility>

#ifdef ALLOWS_UNALIGNED
  #define INTELSHORT(p) (*(uint16_t *)p)
  #define INTELLONG(p) (*(uint32_t *)p)
  #define MOTOSHORT(p) __builtin_bswap16(*(uint16_t *)p)
  #define MOTOLONG(p) __builtin_bswap32(*(uint32_t *)p)
#else
  // Due to unaligned memory causing an exception, we have to do these macros the slow way
  #define INTELSHORT(p) ((*p) + (*(p + 1) << 8))
  #define INTELLONG(p) ((*p) + (*(p + 1) << 8) + (*(p + 2) << 16) + (*(p + 3) << 24))
  #define MOTOSHORT(p) (((*(p)) << 8) + (*(p + 1)))
  #define MOTOLONG(p) (((*p) << 24) + ((*(p + 1)) << 16) + ((*(p + 2)) << 8) + (*(p + 3)))
#endif // ALLOWS_UNALIGNED

auto JPegDecoder::setPixelType(JPegPixelType iType) -> void { image.ucPixelType = iType; }

auto JPegDecoder::setMaxOutputSize(int32_t iMaxMCUs) -> void {
  if (iMaxMCUs < 1) iMaxMCUs = 1; // don't allow invalid value
  image.iMaxMCUs = iMaxMCUs;
}

//
// Memory initialization
//
auto JPegDecoder::openRAM(uint8_t *pData, int32_t iDataSize, JPegDrawCallback pfnDraw) -> int32_t {
  image                = JPegImage{};
  image.ucMemType      = JPegMemType::RAM;
  image.pfnRead        = &JPegImage::readRAM;
  image.pfnSeek        = &JPegImage::seekMem;
  image.pfnDraw        = pfnDraw;
  image.pfnOpen        = nullptr;
  image.pfnClose       = nullptr;
  image.jpegFile.iSize = iDataSize;
  image.jpegFile.pData = pData;
  image.iMaxMCUs       = 1000; // set to an unnaturally high value to start
  return image.init();
}

auto JPegDecoder::openFLASH(const uint8_t *pData, int32_t iDataSize, JPegDrawCallback pfnDraw)
    -> int32_t {
  image                = JPegImage{};
  image.ucMemType      = JPegMemType::FLASH;
  image.pfnRead        = &JPegImage::readFLASH;
  image.pfnSeek        = &JPegImage::seekMem;
  image.pfnDraw        = pfnDraw;
  image.pfnOpen        = nullptr;
  image.pfnClose       = nullptr;
  image.jpegFile.iSize = iDataSize;
  image.jpegFile.pData = (uint8_t *)pData;
  image.iMaxMCUs       = 1000; // set to an unnaturally high value to start
  return image.init();
}

auto JPegDecoder::open(const char *szFilename, JPegDrawCallback pfnDraw) -> int32_t {
  image                  = JPegImage{};
  image.pfnRead          = &JPegImage::readFile;
  image.pfnClose         = &JPegImage::closeFile;
  image.pfnSeek          = &JPegImage::seekFile;
  image.pfnDraw          = pfnDraw;
  image.iMaxMCUs         = 1000;
  image.jpegFile.fHandle = JPegImage::openFile(szFilename, &image.jpegFile.iSize);
  if (image.jpegFile.fHandle == nullptr) return 0;
  return image.init();
}

//
// File (SD/MMC) based initialization
//
auto JPegDecoder::open(const char *szFilename, JPegOpenCallback pfnOpen, JPegCloseCallback pfnClose,
                       JPegReadCallback pfnRead, JPegSeekCallback pfnSeek, JPegDrawCallback pfnDraw)
    -> int32_t {
  image                  = JPegImage{};
  image.pfnRead          = pfnRead;
  image.pfnSeek          = pfnSeek;
  image.pfnDraw          = pfnDraw;
  image.pfnOpen          = pfnOpen;
  image.pfnClose         = pfnClose;
  image.iMaxMCUs         = 1000; // set to an unnaturally high value to start
  image.jpegFile.fHandle = pfnOpen(szFilename, &image.jpegFile.iSize);
  if (image.jpegFile.fHandle == nullptr) return 0;
  return image.init();
}

//
// data stream initialization
//
auto JPegDecoder::open(void *fHandle, int32_t iDataSize, JPegCloseCallback pfnClose,
                       JPegReadCallback pfnRead, JPegSeekCallback pfnSeek, JPegDrawCallback pfnDraw)
    -> int32_t {
  image                  = JPegImage{};
  image.pfnRead          = pfnRead;
  image.pfnSeek          = pfnSeek;
  image.pfnDraw          = pfnDraw;
  image.pfnClose         = pfnClose;
  image.iMaxMCUs         = 1000; // set to an unnaturally high value to start
  image.jpegFile.iSize   = iDataSize;
  image.jpegFile.fHandle = fHandle;
  return image.init();
}

auto JPegDecoder::close() -> void {
  if (image.pfnClose) image.pfnClose(image.jpegFile.fHandle);
}

//
// Decode the image
// returns:
// 1 = good result
// 0 = error
//
auto JPegDecoder::decode(int32_t x, int32_t y, JPegOptions iOptions) -> int32_t {
  image.iXOffset = x;
  image.iYOffset = y;
  image.iOptions = iOptions;
  return image.decodeImage();
}

//
// set draw callback user pointer variable
//
auto JPegDecoder::setUserPointer(void *p) -> void { image.pUser = p; }

auto JPegDecoder::decodeDither(int32_t x, int32_t y, std::unique_ptr<uint8_t[]> pDither,
                               JPegOptions iOptions) -> int32_t {
  image.iXOffset      = x;
  image.iYOffset      = y;
  image.iOptions      = iOptions;
  image.pDitherBuffer = std::move(pDither);
  return image.decodeImage();
}

auto JPegDecoder::decodeDither(std::unique_ptr<uint8_t[]> pDither, JPegOptions iOptions)
    -> int32_t {
  image.iOptions      = iOptions;
  image.pDitherBuffer = std::move(pDither);
  return image.decodeImage();
}

//
// Validate/adjust the requested crop area to land on MCU boundaries
// (expand in all directions if needed)
//
auto JPegDecoder::setCropArea(int32_t x, int32_t y, int32_t w, int32_t h) -> void {
  int32_t mcuCX = 0, mcuCY = 0;

  if (x < 0) x = 0;
  if (y < 0) y = 0;
  switch (image.ucSubSample) // set up the parameters for the different subsampling options
  {
  case 0x00: // fake value to handle grayscale
  case 0x01: // fake value to handle sRGB/CMYK
  case 0x11:
    mcuCX = mcuCY = 8;
    break;
  case 0x12:
    mcuCX = 8;
    mcuCY = 16;
    break;
  case 0x21:
    mcuCX = 16;
    mcuCY = 8;
    break;
  case 0x22:
    mcuCX = mcuCY = 16;
    break;
  default: // to suppress compiler warning
    break;
  }
  if (w & (mcuCX - 1)) {
    w &= ~(mcuCX - 1);
    w += mcuCX;
  }
  if (h & (mcuCY - 1)) {
    h &= ~(mcuCY - 1);
    h += mcuCY;
  }
  if (x > image.iWidth - mcuCX) x = image.iWidth - mcuCX;
  if (y > image.iHeight - mcuCY) y = image.iHeight - mcuCY;
  if (x + w > image.iWidth) w = image.iWidth - mcuCX;
  if (y + h > image.iHeight) h = image.iHeight - mcuCY;
  x &= ~(mcuCX - 1);
  y &= ~(mcuCY - 1);
  image.iCropX  = x;
  image.iCropY  = y;
  image.iCropCX = w;
  image.iCropCY = h;
}

auto JPegDecoder::getCropArea(int32_t *x, int32_t *y, int32_t *w, int32_t *h) const -> void {
  *x = image.iCropX;
  *y = image.iCropY;
  *w = image.iCropCX;
  *h = image.iCropCY;
}

auto JPegDecoder::setFramebuffer(void *pFramebuffer) -> void { image.pFramebuffer = pFramebuffer; }

// ----- JPegImage METHODS -----

//
// Helper functions for memory based images
//
auto JPegImage::readRAM(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t {
  int32_t iBytesRead;

  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0) return 0;
  memcpy(pBuf, &pFile->pData[pFile->iPos], iBytesRead);
  pFile->iPos += iBytesRead;
  return iBytesRead;
}

auto JPegImage::readFLASH(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t {
  int32_t iBytesRead;

  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0) return 0;
  memcpy(pBuf, &pFile->pData[pFile->iPos], iBytesRead);
  pFile->iPos += iBytesRead;
  return iBytesRead;
}

auto JPegImage::seekMem(JPegFile *pFile, int32_t iPosition) -> int32_t {
  if (pFile->iSize <= 0) {
    pFile->iPos = 0;
    return 0;
  }

  if (iPosition < 0)
    iPosition = 0;
  else if (iPosition >= pFile->iSize)
    iPosition = pFile->iSize; // allow EOF position
  pFile->iPos = iPosition;
  return iPosition;
}

auto JPegImage::closeFile(void *handle) -> void { fclose((FILE *)handle); }

auto JPegImage::seekFile(JPegFile *pFile, int32_t iPosition) -> int32_t {
  if (pFile->iSize <= 0) {
    pFile->iPos = 0;
    fseek((FILE *)pFile->fHandle, 0, SEEK_SET);
    return 0;
  }

  if (iPosition < 0)
    iPosition = 0;
  else if (iPosition >= pFile->iSize)
    iPosition = pFile->iSize; // allow EOF position
  pFile->iPos = iPosition;
  fseek((FILE *)pFile->fHandle, iPosition, SEEK_SET);
  return iPosition;
}

auto JPegImage::openFile(const char *szFilename, int32_t *pFileSize) -> void * {
  FILE *f;
  f = fopen(szFilename, "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  *pFileSize = (int32_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  return (void *)f;
}

auto JPegImage::readFile(JPegFile *pFile, uint8_t *pBuf, int32_t iLen) -> int32_t {
  int32_t iBytesRead;

  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)fread(pBuf, 1, iBytesRead, (FILE *)pFile->fHandle);
  pFile->iPos += iBytesRead;
  return iBytesRead;
}

//
// The following functions are written in plain C and have no
// 3rd party dependencies, not even the C runtime library
//
//
// Initialize a JPEG file and callback access from a file on SD or memory
// returns 1 for success, 0 for failure
// Fills in the basic image info fields of the JPegImage structure
//
auto JPegImage::init() -> int32_t {
  return parseInfo(0); // gather info for image
}

//
// Unpack the Huffman tables
//
auto JPegImage::getHuffTables(uint8_t *pBuf, int32_t iLen) -> int32_t {
  int32_t i, j, iOffset, iTableOffset;
  uint8_t ucTable, *pHuffVals;

  iOffset   = 0;
  pHuffVals = (uint8_t *)usPixels; // temp holding area to save RAM

  // while there are tables to copy (we may have combined more than 1 table together)
  while (iLen > 17) {
    ucTable = pBuf[iOffset++]; // get table index
    if (ucTable & 0x10)        // convert AC offset of 0x10 into offset of 4
      ucTable ^= 0x14;
    ucHuffTableUsed |= (1 << ucTable); // mark this table as being defined
    if (ucTable <= 7) {
      // tables are 0-3, AC+DC
      iTableOffset = ucTable * HUFF_TABLEN;
      j            = 0; // total bits
      for (i = 0; i < 16; i++) {
        j += pBuf[iOffset];
        pHuffVals[iTableOffset + i] = pBuf[iOffset++];
      }
      iLen -= 17;                        // subtract length of bit lengths
      if (j == 0 || j > 256 || j > iLen) // bogus bit lengths
      {
        return -1;
      }
      iTableOffset += 16;
      for (i = 0; i < j; i++) { // copy huffman table
        pHuffVals[iTableOffset + i] = pBuf[iOffset++];
      }
      iLen -= j;
    }
  }
  return 0;
}

//
// Expand the Huffman tables for fast decoding
// returns 1 for success, 0 for failure
//
auto JPegImage::makeHuffTables(int32_t bThumbnail) -> int32_t {
  int32_t code, repeat, count, codestart;
  int32_t j;
  int32_t iLen, iTable;
  uint16_t *pTable, *pShort, *pLong;
  uint8_t *pHuffVals, *pucTable, *pucShort, *pucLong;
  uint32_t ul, *pLongTable;
  int32_t iBitNum; // current code bit length
  int32_t cc;      // code
  uint8_t *p, *pBits, ucCode;
  int32_t iMaxLength, iMaxMask;
  int32_t iTablesUsed;

  iTablesUsed = 0;
  pHuffVals   = (uint8_t *)usPixels;
  for (j = 0; j < 4; j++) {
    if (ucHuffTableUsed & (1 << j)) iTablesUsed++;
  }
  // first do DC components (up to 4 tables of 12-bit codes)
  // we can save time and memory for the DC codes by knowing that there exist short codes (<= 6
  // bits) and long codes (>6 bits, but the first 5 bits are 1's).  This allows us to create 2
  // tables: a 6-bit and 7 or 8-bit to handle any DC codes
  iMaxLength = 12;   // assume DC codes can be 12-bits
  iMaxMask   = 0x7f; // lower 7 bits after truncate 5 leading 1's
  for (iTable = 0; iTable < 4; iTable++) {
    if (ucHuffTableUsed & (1 << iTable)) {
      //         huffdcFast[iTable] = (int32_t *)PILIOAlloc(0x180); // short table = 128
      //         bytes, long table = 256 bytes
      pucShort = &ucHuffDC[iTable * DC_TABLE_SIZE];
      //         huffdc[iTable] = huffdcFast[iTable] + 0x20; // 0x20 longs = 128 bytes
      pucLong = &ucHuffDC[iTable * DC_TABLE_SIZE + 128];
      pBits   = &pHuffVals[iTable * HUFF_TABLEN];
      p       = pBits;
      p += 16; // point to bit data
      cc = 0;  // start with a code of 0
      for (iBitNum = 1; iBitNum <= 16; iBitNum++) {
        iLen = *pBits++; // get number of codes for this bit length
        if (iBitNum > iMaxLength && iLen > 0) {
          // we can't handle codes longer a certain length
          return 0;
        }
        while (iLen) {
          // if (iBitNum > 6) // do long table
          if ((cc >> (iBitNum - 5)) == 0x1f) {
            // first 5 bits are 1 - use long table
            count     = iMaxLength - iBitNum;
            codestart = cc << count;
            pucTable  = &pucLong[codestart & iMaxMask]; // use lower 7/8 bits of code
          } else {
            // do short table
            count = 6 - iBitNum;
            if (count < 0) return 0; // DEBUG - something went wrong
            codestart = cc << count;
            pucTable  = &pucShort[codestart];
          }
          ucCode = *p++; // get actual huffman code
          // does precalculating the DC value save time on ARM?
          if (ucCode != 0 && (ucCode + iBitNum) <= 6 && ucMode != 0xc2) {
            // we can fit the magnitude value in the code lookup (not for progressive)
            int32_t k, iLoop;
            uint8_t ucCoeff;
            uint8_t *d    = &pucTable[512];
            uint8_t ucMag = ucCode;
            ucCode |= ((iBitNum + ucCode) << 4); // add magnitude bits to length
            repeat = 1 << ucMag;
            iLoop  = 1 << (count - ucMag);
            for (j = 0; j < repeat; j++) { // calcuate the magnitude coeff already
              if (j & 1 << (ucMag - 1))    // positive number
                ucCoeff = (uint8_t)j;
              else // negative number
                ucCoeff = (uint8_t)(j - ((1 << ucMag) - 1));
              for (k = 0; k < iLoop; k++) {
                *d++ = ucCoeff;
              } // for k
            } // for j
          } else {
            ucCode |= (iBitNum << 4);
          }
          if (count) {
            // do it as dwords to save time
            repeat = (1 << count);
            memset(pucTable, ucCode, repeat);
            //                  pLongTable = (uint32_t *)pTable;
            //                  repeat = 1 << (count-2); // store as dwords (/4)
            //                  ul = code | (code << 16);
            //                  for (j=0; j<repeat; j++)
            //                     *pLongTable++ = ul;
          } else {
            pucTable[0] = ucCode;
          }
          cc++;
          iLen--;
        }
        cc <<= 1;
      }
    } // if table defined
  }
  // now do AC components (up to 4 tables of 16-bit codes)
  // We split the codes into a short table (9 bits or less) and a long table (first 5 bits are 1)
  if (ucMode == 0xc2) return 1; // don't calculate for progressive mode
  for (iTable = 0; iTable < 4; iTable++) {
    if (ucHuffTableUsed & (1 << (iTable + 4))) {
      // if this table is defined
      pBits = &pHuffVals[(iTable + 4) * HUFF_TABLEN];
      p     = pBits;
      p += 16; // point to bit data
      if (iTable * HUFF11SIZE >= (int32_t)sizeof(usHuffAC) / 2) return 0;
      pShort = &usHuffAC[iTable * HUFF11SIZE];
      pLong  = &usHuffAC[iTable * HUFF11SIZE + 1024];
      cc     = 0; // start with a code of 0
      // construct the decode table
      for (iBitNum = 1; iBitNum <= 16; iBitNum++) {
        iLen = *pBits++; // get number of codes for this bit length
        while (iLen) {
          if ((cc >> (iBitNum - 6)) == 0x3f) {
            // first 6 bits are 1 - use long table
            count     = 16 - iBitNum;
            codestart = cc << count;
            pTable    = &pLong[codestart & 0x3ff]; // use lower 10 bits of code
          } else {
            count = 10 - iBitNum;
            if (count < 0) {
              // an 11/12-bit? code - that doesn't fit our optimized scheme, see if we
              // can do a bigger table version
              if (count == -1 && iTablesUsed <= 4) { // DEBUG
                // we need to create "slow" tables
                // j = makeHuffTables_Slow(bThumbnail);
                return 0;
              } else
                return 0; // DEBUG - fatal error, more than 2 big tables we currently don't support
            }
            codestart = cc << count;
            pTable    = &pShort[codestart]; // 10 bits or shorter
          }
          code = *p++; // get actual huffman code
          if (bThumbnail && code != 0) {
            // add "extra" bits to code length since we skip these codes
            // get rid of extra bits in code and add increment (1) for AC index
            code = ((iBitNum + (code & 0xf)) << 8) | ((code >> 4) + 1);
          } else {
            code |= (iBitNum << 8);
          }
          if (count) {
            // do it as dwords to save time
            repeat     = 1 << (count - 1); // store as dwords (/2)
            ul         = code | (code << 16);
            pLongTable = (uint32_t *)pTable;
            for (j = 0; j < repeat; j++) *pLongTable++ = ul;
          } else {
            pTable[0] = (uint16_t)code;
          }
          cc++;
          iLen--;
        }
        cc <<= 1;
      } // for each bit length
    } // if table defined
  }
  return 1;
}

//
// tiffShort
// read a 16-bit unsigned integer from the given pointer
// and interpret the data as big endian (Motorola) or little endian (Intel)
//
auto JPegImage::tiffShort(uint8_t *p, int32_t bMotorola) -> uint16_t {
  uint16_t s;

  if (bMotorola)
    s = *p * 0x100 + *(p + 1); // big endian (AKA Motorola byte order)
  else
    s = *p + *(p + 1) * 0x100; // little endian (AKA Intel byte order)
  return s;
}

//
// tiffLong
// read a 32-bit unsigned integer from the given pointer
// and interpret the data as big endian (Motorola) or little endian (Intel)
//
auto JPegImage::tiffLong(uint8_t *p, int32_t bMotorola) -> uint32_t {
  uint32_t l;

  if (bMotorola)
    l = *p * 0x1000000 + *(p + 1) * 0x10000 + *(p + 2) * 0x100 + *(p + 3); // big endian
  else
    l = *p + *(p + 1) * 0x100 + *(p + 2) * 0x10000 + *(p + 3) * 0x1000000; // little endian
  return l;
}

//
// tiffValue
// read an integer value encoded in a TIFF TAG (12-byte structure)
// and interpret the data as big endian (Motorola) or little endian (Intel)
//
auto JPegImage::tiffValue(uint8_t *p, int32_t bMotorola) -> int32_t {
  int32_t i, iType;

  iType = tiffShort(p + 2, bMotorola);
  /* If pointer to a list of items, must be a long */
  if (tiffShort(p + 4, bMotorola) > 1) {
    iType = 4;
  }
  switch (iType) {
  case 3: /* Short */
    i = tiffShort(p + 8, bMotorola);
    break;
  case 4: /* Long */
  case 7: // undefined (treat it as a long since it's usually a multibyte buffer)
    i = tiffLong(p + 8, bMotorola);
    break;
  case 6: // signed byte
    i = (signed char)p[8];
    break;
  case 2:  /* ASCII */
  case 5:  /* Unsigned Rational */
  case 10: /* Signed Rational */
    i = tiffLong(p + 8, bMotorola);
    break;
  default: /* to suppress compiler warning */
    i = 0;
    break;
  }
  return i;
}

auto JPegImage::getTiffInfo(int32_t bMotorola, int32_t iOffset) -> void {
  int32_t iTag, iTagCount, i;
  uint8_t *cBuf = ucFileBuf;

  iTagCount = tiffShort(&cBuf[iOffset], bMotorola); /* Number of tags in this dir */
  if (iTagCount < 1 || iTagCount > 256)             // invalid tag count
    return;                                         /* Bad header info */
  /*--- Search the TIFF tags ---*/
  for (i = 0; i < iTagCount; i++) {
    uint8_t *p = &cBuf[iOffset + (i * 12) + 2];
    iTag       = tiffShort(p, bMotorola); /* current tag value */
    if (iTag == 274) {
      // orientation tag
      ucOrientation = tiffValue(p, bMotorola);
    } else if (iTag == 256) {
      // width of thumbnail
      iThumbWidth = tiffValue(p, bMotorola);
    } else if (iTag == 257) {
      // height of thumbnail
      iThumbHeight = tiffValue(p, bMotorola);
    } else if (iTag == 513) {
      // offset to JPEG data
      iThumbData = tiffValue(p, bMotorola);
    }
  }
}

auto JPegImage::getSOS(int32_t *iOff) -> int32_t {
  int16_t sLen;
  int32_t iOffset = *iOff;
  int32_t i, j;
  uint8_t uc, c, cc;
  uint8_t *buf = ucFileBuf;

  sLen = MOTOSHORT(&buf[iOffset]);
  iOffset += 2;

  // Assume no components in this scan
  for (i = 0; i < 4; i++) compInfo[i].component_needed = 0;

  uc                 = buf[iOffset++]; // get number of components
  ucComponentsInScan = uc;
  sLen -= 3;

  if (uc < 1 || uc > MAX_COMPS_IN_SCAN || sLen != (uc * 2 + 3)) {
    // check length of data packet
    return 1; // error
  }

  for (i = 0; i < uc; i++) {
    cc = buf[iOffset++];
    c  = buf[iOffset++];
    sLen -= 2;

    for (j = 0; j < 4; j++) {
      // search for component id
      if (compInfo[j].component_id == cc) break;
    }

    if (j == 4) {
      // error, not found
      return 1;
    }

    if ((c & 0xf) > 3 || (c & 0xf0) > 0x30) return 1; // bogus table numbers

    compInfo[j].dc_tbl_no        = c >> 4;
    compInfo[j].ac_tbl_no        = c & 0xf;
    compInfo[j].component_needed = 1; // mark this component as being included in the scan
  }

  iScanStart      = buf[iOffset++]; // Get the scan start (or lossless predictor) for this scan
  iScanEnd        = buf[iOffset++]; // Get the scan end for this scan
  c               = buf[iOffset++]; // successive approximation bits
  cApproxBitsLow  = c & 0xf;        // also point transform in lossless mode
  cApproxBitsHigh = c >> 4;

  *iOff = iOffset;
  return 0;
}

//
// Remove markers from the data stream to allow faster decode
// Stuffed zeros and restart interval markers aren't needed to properly decode
// the data, but they make reading VLC data slower, so I pull them out first
//
auto JPegImage::filter(uint8_t *pBuf, uint8_t *d, int32_t iLen, uint8_t *bFF) -> int32_t {

  uint8_t c, *s, *pEnd, *pStart;

  pStart = d;
  s      = pBuf;
  pEnd   = &s[iLen - 1]; // stop just shy of the end to not miss a final marker/stuffed 0
  if (*bFF) {
    // last byte was a FF, check the next one
    if (s[0] == 0) {
      // stuffed 0, keep the FF
      *d++ = 0xff;
    }
    s++;
    *bFF = 0;
  }

  while (s < pEnd) {
    c = *d++ = *s++;
    if (c == 0xff) {
      // marker or stuffed zeros?
      if (s[0] != 0) {
        // it's a marker, skip both
        d--;
      }
      s++; // for stuffed 0's, store the FF, skip the 00
    }
  }
  if (s == pEnd) {
    // need to test the last byte
    c = s[0];
    if (c == 0xff) {
      // last byte is FF, take care of it next time through
      *bFF = 1; // take care of it next time through
    } else {
      *d++ = c; // nope, just store it
    }
  }
  return (int32_t)(d - pStart); // filtered output length
}

//
// Read and filter more VLC data for decoding
//
auto JPegImage::getMoreData() -> void {
  int32_t iDelta = iVLCSize - iVLCOff;
  //    printf("Getting more data...size=%d, off=%d\n", iVLCSize, iVLCOff);
  // move any existing data down
  if (iDelta >= (JPEG_FILE_BUF_SIZE - 64) || iDelta < 0) {
    return; // buffer is already full; no need to read more data
  }

  if (iVLCOff != 0) {
    memcpy(ucFileBuf, &ucFileBuf[iVLCOff], iVLCSize - iVLCOff);
    iVLCSize -= iVLCOff;
    iVLCOff = 0;
    bb.pBuf = ucFileBuf; // reset VLC source pointer too
  }

  if (jpegFile.iPos < jpegFile.iSize && iVLCSize < JPEG_FILE_BUF_SIZE - 64) {
    int32_t i;
    // Try to read enough to fill the buffer
    i = (*pfnRead)(&jpegFile, &ucFileBuf[iVLCSize],
                   JPEG_FILE_BUF_SIZE - iVLCSize); // max length we can read
    // Filter out the markers
    iVLCSize += filter(&ucFileBuf[iVLCSize], &ucFileBuf[iVLCSize], i, &ucFF);
  }
}

//
// Parse the JPEG header, gather necessary info to decode the image
// Returns 1 for success, 0 for failure
//
auto JPegImage::parseInfo(int32_t bExtractThumb) -> int32_t {
  int32_t iBytesRead;
  int32_t i, iOffset, iTableOffset;
  uint8_t ucTable, *s      = ucFileBuf;
  uint16_t usMarker, usLen = 0;
  int32_t iFilePos = 0;

  pFramebuffer = nullptr; // this must be set AFTER calling this function
  // make sure usPixels is 16-byte aligned for S3 SIMD (and possibly others)
  i = (int32_t)(int64_t)usUnalignedPixels;
  i &= 15;
  if (i == 0) i = 16; // already 16-byte aligned
  usPixels = &usUnalignedPixels[(16 - i) >> 1];
  // do the same for the MCU buffers
  i = (int32_t)(int64_t)sUnalignedMCUs;
  i &= 15;
  if (i == 0) i = 16;
  sMCUs = &sUnalignedMCUs[(16 - i) >> 1];

  if (bExtractThumb) // seek to the start of the thumbnail image
  {
    iFilePos = iThumbData;
    (*pfnSeek)(&jpegFile, iFilePos);
  }
  iBytesRead = (*pfnRead)(&jpegFile, s, JPEG_FILE_BUF_SIZE);
  if (iBytesRead < 256) // a JPEG file this tiny? probably bad
  {
    iError = JPegError::INVALID_FILE;
    return 0;
  }
  iFilePos += iBytesRead;
  if (MOTOSHORT(ucFileBuf) != 0xffd8) {
    iError = JPegError::INVALID_FILE;
    return 0; // not a JPEG file
  }
  iOffset  = 2; /* Start at offset of first marker */
  usMarker = 0; /* Search for SOFx (start of frame) marker */
  while (usMarker != 0xffda && iOffset < jpegFile.iSize) {
    if (iOffset >= JPEG_FILE_BUF_SIZE / 2) // too close to the end, read more data
    {
      // Do we need to seek first?
      if (iOffset >= JPEG_FILE_BUF_SIZE) {
        iFilePos += (iOffset - iBytesRead);
        iOffset = 0;
        (*pfnSeek)(&jpegFile, iFilePos);
        iBytesRead = 0; // throw away any old data
      }
      if (iOffset > iBytesRead) { // something went wrong
        iError = JPegError::DECODE_ERROR;
        return 0;
      }
      // move existing bytes down
      if (iOffset) {
        memcpy(ucFileBuf, &ucFileBuf[iOffset], iBytesRead - iOffset);
        iBytesRead -= iOffset;
        iOffset = 0;
      }
      i = (*pfnRead)(&jpegFile, &ucFileBuf[iBytesRead], JPEG_FILE_BUF_SIZE - iBytesRead);
      iFilePos += i;
      iBytesRead += i;
    }
    usMarker = MOTOSHORT(&s[iOffset]);
    iOffset += 2;
    usLen = MOTOSHORT(&s[iOffset]); // marker length

    if (usMarker < 0xffc0 || usMarker == 0xffff) // invalid marker, could be generated by "Arles
                                                 // Image Web Page Creator" or Accusoft
    {
      iOffset++;
      continue; // skip 1 byte and try to resync
    }
    switch (usMarker) {
    case 0xffc1: // extended mode
    case 0xffc3: // lossless mode
      iError = JPegError::UNSUPPORTED_FEATURE;
      return 0; // currently unsupported modes

    case 0xffe1: // App1 (EXIF?)
      if (s[iOffset + 2] == 'E' && s[iOffset + 3] == 'x' &&
          (s[iOffset + 8] == 'M' || s[iOffset + 8] == 'I')) // the EXIF data we want
      {
        int32_t bMotorola, IFD, iTagCount;
        iEXIF = iFilePos - iBytesRead + iOffset + 8; // start of TIFF file
        // Get the orientation value (if present)
        bMotorola = (s[iOffset + 8] == 'M');
        IFD       = tiffLong(&s[iOffset + 12], bMotorola);
        iTagCount = tiffShort(&s[iOffset + 16], bMotorola);
        getTiffInfo(bMotorola, IFD + iOffset + 8);
        // The second IFD defines the thumbnail (if present)
        if (iTagCount >= 1 && iTagCount < 32) // valid number of tags for EXIF data 'page'
        {
          // point to next IFD
          IFD += (12 * iTagCount) + 2;
          IFD = tiffLong(&s[IFD + iOffset + 8], bMotorola);
          if (IFD != 0 && IFD + iOffset + 8 < JPEG_FILE_BUF_SIZE) // Thumbnail present?
          {
            ucHasThumb = 1;
            getTiffInfo(bMotorola, IFD + iOffset + 8); // info for second 'page' of TIFF
            iThumbData += iOffset + 8;                 // absolute offset in the file
          }
        }
      }
      break;
    case 0xffc0: // SOFx - start of frame (baseline)
    case 0xffc2: // (progressive)
      ucMode = (uint8_t)usMarker;
      ucBpp  = s[iOffset + 2]; // bits per sample
      iCropX = iCropY = 0;     // initialize crop rectangle to full image size
      iCropCY = iHeight = MOTOSHORT(&s[iOffset + 3]);
      iCropCX = iWidth = MOTOSHORT(&s[iOffset + 5]);
      ucNumComponents  = s[iOffset + 7];
      ucBpp            = ucBpp * ucNumComponents; /* Bpp = number of components * bits per sample */
      {
        usLen -= 8;
        iOffset += 8;
        //                    ucSubSample = s[iOffset+9]; // subsampling option for the
        //                    second color component
        for (i = 0; i < ucNumComponents; i++) {
          uint8_t ucSamp;
          compInfo[i].component_id    = s[iOffset++];
          compInfo[i].component_index = (uint8_t)i;
          ucSamp                      = s[iOffset++]; // get the h+v sampling factor
          if (i == 0)                                 // Y component?
            ucSubSample = ucSamp;
          //                        compInfo[i].h_samp_factor = ucSamp >> 4;
          //                        compInfo[i].v_samp_factor = ucSamp & 0xf;
          compInfo[i].quant_tbl_no = s[iOffset++]; // quantization table number
          if (compInfo[i].quant_tbl_no > 3) {
            iError = JPegError::DECODE_ERROR;
            return 0; // error
          }
          usLen -= 3;
        }
      }
      if (ucNumComponents == 1) {
        ucSubSample = 0; // use this to differentiate from color 1:1
      }
      break;
    case 0xffdd: // Restart Interval
      if (usLen == 4) iResInterval = MOTOSHORT(&s[iOffset + 2]);
      break;
    case 0xffc4: /* M_DHT */                      // get Huffman tables
      iOffset += 2;                               // skip length
      usLen -= 2;                                 // subtract length length
      if (getHuffTables(&s[iOffset], usLen) != 0) // bad tables?
      {
        iError = JPegError::DECODE_ERROR;
        return 0; // error
      }
      break;
    case 0xffdb: /* M_DQT */
      /* Get the quantization tables */
      /* first byte has PPPPNNNN where P = precision and N = table number 0-3 */
      iOffset += 2; // skip length
      usLen -= 2;   // subtract length length
      while (usLen > 0) {
        ucTable = s[iOffset++];  // table number
        if ((ucTable & 0xf) > 3) // invalid table number
        {
          iError = JPegError::DECODE_ERROR;
          return 0;
        }
        iTableOffset = (ucTable & 0xf) * DCTSIZE;
        if (ucTable & 0xf0) // if word precision
        {
          for (i = 0; i < DCTSIZE; i++) {
            sQuantTable[i + iTableOffset] = MOTOSHORT(&s[iOffset]);
            iOffset += 2;
          }
          usLen -= (DCTSIZE * 2 + 1);
        } else // byte precision
        {
          for (i = 0; i < DCTSIZE; i++) {
            sQuantTable[i + iTableOffset] = (uint16_t)s[iOffset++];
          }
          usLen -= (DCTSIZE + 1);
        }
      }
      break;
    } // switch on JPEG marker
    iOffset += usLen;
  } // while
  if (usMarker == 0xffda) // start of image
  {
    //        if (ucBpp != 8) // need to match up table IDs
    //        {
    iOffset -= usLen;
    getSOS(&iOffset);       // get Start-Of-Scan info for decoding
                            //        }
    if (!makeHuffTables(0)) // int32_t bThumbnail) DEBUG
    {
      iError = JPegError::UNSUPPORTED_FEATURE;
      return 0;
    }
    // Now the offset points to the start of compressed data
    i        = filter(&ucFileBuf[iOffset], ucFileBuf, iBytesRead - iOffset, &ucFF);
    iVLCOff  = 0;
    iVLCSize = i;
    getMoreData(); // read more VLC data
    return 1;
  }
  iError = JPegError::DECODE_ERROR;
  return 0;
}

//
// Fix and reorder the quantization table for faster decoding.*
//
auto JPegImage::fixQuantD() -> void {
  int32_t iTable, iTableOffset;
  int16_t sTemp[DCTSIZE];
  int32_t i;
  uint16_t *p;

  for (iTable = 0; iTable < ucNumComponents; iTable++) {
    iTableOffset = iTable * DCTSIZE;
    p            = (uint16_t *)&sQuantTable[iTableOffset];
    for (i = 0; i < DCTSIZE; i++) sTemp[i] = p[cZigZag[i]];
    memcpy(&sQuantTable[iTableOffset], sTemp,
           DCTSIZE * sizeof(int16_t)); // copy back to original spot

    // Prescale for DCT multiplication
    p = (uint16_t *)&sQuantTable[iTableOffset];
    for (i = 0; i < DCTSIZE; i++) {
      p[i] = (uint16_t)((p[i] * iScaleBits[i]) >> 12);
    }
  }
}

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : decodeMCUProgressive(char *, int32_t *, int32_t *, int32_t *, JPEGDATA *)   *
 *                                                                          *
 *  PURPOSE    : Decompress a macro block of Progressive JPEG data.         *
 *                                                                          *
 ****************************************************************************/
auto JPegImage::decodeMCUProgressive(int32_t iMCU, int32_t *iDCPredictor) -> int32_t {
  int32_t iCount;
  int32_t iIndex;
  uint8_t ucHuff, *pFastDC;
  uint16_t *pFast;
  uint32_t usHuff; // this prevents an unnecessary & 65535 for shorts
  int32_t iPositive, iNegative, iCoeff;
  int16_t *pMCU = &sMCUs[iMCU & 0xffffff];
  uint32_t ulBitOff;
  uint32_t ulCode, ulBits, ulTemp; // local copies to allow compiler to use register vars
  uint8_t *pBuf;

  ulBitOff = bb.ulBitOff;
  ulBits   = bb.ulBits;
  pBuf     = bb.pBuf;

  if (ulBitOff > (32 - 17)) {
    // need to get more data
    pBuf += (ulBitOff >> 3);
    ulBitOff &= 7;
    ulBits = MOTOLONG(pBuf);
  }

  iPositive = (1 << cApproxBitsLow);    // positive bit position being coded
  iNegative = ((-1) << cApproxBitsLow); // negative bit position being coded

  if (iScanStart == 0) {
    if (cApproxBitsHigh) {
      // successive approximation - simply encodes the specified bit
      ulCode = (ulBits >> (31 - ulBitOff)) & 1; // just get 1 bit
      ulBitOff += 1;
      if (ulCode) {
        //            (*iDCPredictor) |= iPositive;  // in case the scan is run more than once
        //            pMCU[0] = *iDCPredictor; // store in MCU[0]
        pMCU[0] |= iPositive;
      }
      goto mcu_done; // that's it
    }
    // get the DC component
    ulCode = (ulBits >> (32 - 12 - ulBitOff)) & 0xfff; // get as lower 12 bits
    if (ulCode >= 0xf80) {
      // long code
      ulCode = (ulCode & 0xff); // point to long table
    } else {
      ulCode >>= 6; // use first 6 bits of short code
    }
    pFastDC = &ucHuffDC[ucDCTable * DC_TABLE_SIZE];
    ucHuff  = pFastDC[ulCode]; // get the length+code
    if (ucHuff == 0) {
      return -1; // invalid code
    }
    ulBitOff += (ucHuff >> 4); // add the Huffman length
    ucHuff &= 0xf;             // get the actual code (SSSS)
    if (ucHuff) {
      // if there is a change to the DC value
      // get the 'extra' bits
      if (ulBitOff > (32 - 17)) {
        // need to get more data
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
      ulCode = ulBits << ulBitOff;
      ulTemp = ~(uint32_t)(((int32_t)ulCode) >> (32 - 1)); // slide sign bit across other 63/31 bits
      ulCode >>= (32 - ucHuff);
      ulCode -= ulTemp >> (32 - ucHuff);
      ulBitOff += ucHuff;        // add bit length
      ulCode <<= cApproxBitsLow; // successive approximation shift value
      (*iDCPredictor) += ulCode;
    }
    pMCU[0] = (int16_t)*iDCPredictor; // store in MCU[0]
  }
  // Now get the other 63 AC coefficients
  pFast = &usHuffAC[ucACTable * HUFF11SIZE];
  if (iScanStart) {
    iIndex = iScanStart; // starting index of this scan (progressive JPEG)
  } else {
    iIndex = 1; // special case when the DC component is included
  }
  if (cApproxBitsHigh) {
    // successive approximation - different method
    if (1) {
      //        if (*iSkip == 0) // only decode this block if not being skipped in EOB run
      for (; iIndex <= iScanEnd; iIndex++) {
        if (ulBitOff > (32 - 17)) { // need to get more data
          pBuf += (ulBitOff >> 3);
          ulBitOff &= 7;
          ulBits = MOTOLONG(pBuf);
        }
        ulCode = (ulBits >> (32 - 16 - ulBitOff)) & 0xffff; // get as lower 16 bits
        if (ulCode >= 0xf000) {
          // first 4 bits = 1, use long table
          ulCode = (ulCode & 0x1fff);
        } else {
          ulCode >>= 4; // use lower 12 bits (short table)
        }
        usHuff = pFast[ulCode];
        if (usHuff == 0) {
          return -1; // invalid code
        }
        ulBitOff += (usHuff >> 8); // add length
        usHuff &= 0xff;            // get code (RRRR/SSSS)
        iCoeff = 0;
        if (usHuff & 0xf) {
          if ((usHuff & 0xf) != 1) // size of new coefficient should always be one
            return -1;
          ulCode = (ulBits >> (32 - 1 - ulBitOff)) & 1; // just get 1 bit
          ulBitOff += 1;
          if (ulCode) {
            // 1 means use positive value; 0 = use negative
            iCoeff = iPositive;
          } else {
            iCoeff = iNegative;
          }
        } else {
          // since SSSS = 0, must be a ZRL or EOBn code
          if (usHuff != 0xf0) {
            // ZRL
            // EOBn code
            usHuff = (usHuff >> 4); // get the number of extra bits needed to code the count
            ulCode = ulBits >> (32 - usHuff - ulBitOff); // shift down by (SSSS) - extra length
            ulCode &= iBitMasks[usHuff];
            ulCode += (1 << usHuff); // plus base amount
            ulBitOff += usHuff;      // add extra length
            //*iSkip = ulCode; // return this skip amount
            break;
          }
        }
        // Advance over already-nonzero coefficients and RRRR still-zero coefficients
        // appending correction bits to the nonzeroes.  A correction bit is 1 if the abs
        // value of the coefficient must be increased.
        iCount = (usHuff >> 4); // get RRRR in lower 4 bits
        do {
          if (pMCU[iIndex]) {
            if (ulBitOff > (32 - 17)) { // need to get more data
              pBuf += (ulBitOff >> 3);
              ulBitOff &= 7;
              ulBits = MOTOLONG(pBuf);
            }
            ulCode = (ulBits >> (32 - 1 - ulBitOff)) & 1; // just get 1 bit
            ulBitOff++;
            if (ulCode) {
              if ((pMCU[iIndex] & iPositive) == 0) // only combine if not already done
              {
                if (pMCU[iIndex] >= 0)
                  pMCU[iIndex] += (int16_t)iPositive;
                else
                  pMCU[iIndex] += (int16_t)iNegative;
              }
            }
          } else // count the zero coeffs to skip
          {
            if (--iCount < 0) break; // done skipping zeros
          }
          iIndex++;
        } while (iIndex <= iScanEnd);
        if (iCoeff && iIndex < 0x40) {
          // store the non-zero coefficient
          pMCU[iIndex] = (int16_t)iCoeff;
        }
      } // for - AC coeffs
    } // if not skipped
    if (0) {
      //        if (*iSkip) // scan any remaining coefficient positions after the end-of-band
      for (; iIndex <= iScanEnd; iIndex++) {
        if (pMCU[iIndex]) {
          // only non-zero ones need correction
          if (ulBitOff > 15) {
            // need to grab more bytes to nibble on
            pBuf += 2; // grab 2 more bytes since that's what we really need
            ulBitOff -= 16;
            ulBits <<= 16;
            ulBits |= MOTOSHORT(&pBuf[2]);
          }
          ulCode = ulBits >> (32 - 1 - ulBitOff); // get 1 bit
          ulBitOff++;
          if (ulCode & 1) {
            // correction bit
            if ((pMCU[iIndex] & iPositive) == 0) {
              // only combine if not already done
              if (pMCU[iIndex] >= 0) {
                pMCU[iIndex] += (int16_t)iPositive;
              } else {
                pMCU[iIndex] += (int16_t)iNegative;
              }
            }
          } // if correction bit
        } // if coeff is non-zero
      } // for the rest of the AC coefficients
      //   (*iSkip)--; // count this block as completed
    } // if this block is being skipped
  } else {
    // normal AC decoding
    // if (*iSkip == 0) // if this block is not being skipped in a EOB run
    {
      while (iIndex <= iScanEnd) {
        if (ulBitOff > 15) {
          // need to grab more bytes to nibble on
          pBuf += 2; // grab 2 more bytes since that's what we really need
          ulBitOff -= 16;
          ulBits <<= 16;
          ulBits |= MOTOSHORT(&pBuf[2]);
        }
        ulCode = (ulBits >> (32 - 16 - ulBitOff)) & 0xffff; // get as lower 16 bits
        if (ulCode >= 0xf000) {                             // first 4 bits = 1, use long table
          ulCode = (ulCode & 0x1fff);
        } else {
          ulCode >>= 4; // use lower 12 bits (short table)
        }
        usHuff = pFast[ulCode];
        if (usHuff == 0) {
          return -1; // invalid code
        }
        ulBitOff += (usHuff >> 8); // add length
        usHuff &= 0xff;            // get code (RRRR/SSSS)
        //            if (usHuff == 0) // no more AC components
        //               {
        //               goto mcu_done;
        //               }
        if (usHuff == 0xf0) {
          // is it ZRL?
          iIndex += 16; // skip 16 AC coefficients
        } else {
          if (ulBitOff > 15) {
            pBuf += 2; // grab 2 more bytes since that's what we really need
            ulBitOff -= 16;
            ulBits <<= 16;
            ulBits |= MOTOSHORT(&pBuf[2]);
          }
          if ((usHuff & 0xf) == 0) {
            // special case for encoding EOB (end-of-band) codes (SSSS=0)
            usHuff = (usHuff >> 4); // get the number of extra bits needed to code the count
            ulCode = ulBits >> (32 - usHuff - ulBitOff); // shift down by (SSSS) - extra length
            ulCode &= iBitMasks[usHuff];
            ulCode += (1 << usHuff); // plus base amount
            ulBitOff += usHuff;      // add extra length
                                     // *iSkip = ulCode; // return this skip amount
            break;
          } else {
            iIndex += (usHuff >> 4); // skip amount
            usHuff &= 0xf;           // get (SSSS) - extra length
            ulCode = ulBits << ulBitOff;
            ulCode >>= (32 - usHuff);
            if (!(ulCode & 0x80000000 >> (32 - 16 - -usHuff))) // test for negative
              ulCode -= 0xffffffff >> (32 - 16 - -usHuff);
            ulBitOff += usHuff;               // add (SSSS) extra length
            ulCode <<= cApproxBitsLow;        // successive approximation shift value
            pMCU[iIndex++] = (int16_t)ulCode; // store AC coefficient
          }
        }
      } // while
    } // if this block not skipped
    //    if (*iSkip)
    //        (*iSkip)--; // count this block as being completed (or skipped)
  } // end of non-successive approx code
mcu_done:
  bb.pBuf     = pBuf;
  iVLCOff     = (int32_t)(pBuf - ucFileBuf);
  bb.ulBitOff = ulBitOff;
  bb.ulBits   = ulBits;
  return 0;
}

//
// Decode the DC and 2-63 AC coefficients of the current DCT block
// For 1/4 and 1/8 scaled images, we don't store most of the AC values since we
// won't use them. For skipped MCUs (outside crop area), we don't decode any AC values
//
auto JPegImage::decodeMCU(int32_t iMCU, int32_t *iDCPredictor) -> int32_t {
  uint32_t ulCode, ulTemp;
  uint8_t *pZig;
  signed char cCoeff;
  uint16_t *pFast;
  uint8_t ucHuff, *pucFast;
  uint32_t usHuff; // this prevents an unnecessary & 65535 for shorts
  uint32_t ulBitOff;
  uint32_t ulBits; // local copies to allow compiler to use register vars
  uint8_t *pBuf, *pEnd, *pEnd2;
  int16_t *pMCU = &sMCUs[iMCU];
  uint16_t u16MCUFlags;

  #define MIN_DCT_THRESHOLD 8

  ulBitOff = bb.ulBitOff;
  ulBits   = bb.ulBits;
  pBuf     = bb.pBuf;

  if (ulBitOff > (32 - 17)) {
    // need to get more data
    pBuf += (ulBitOff >> 3);
    ulBitOff &= 7;
    ulBits = MOTOLONG(pBuf);
  }

  if (iMCU < 0) {                    // skip this block (cropped, or grayscale output from color)
    pEnd2 = (uint8_t *)&cZigZag2[1]; // we only capture the DC value
  } else if (iOptions.test(JPEG_OPTION(SCALE_QUARTER)) ||
             iOptions.test(JPEG_OPTION(SCALE_EIGHTH))) {
    // reduced size DCT
    pMCU[1] = pMCU[8] = pMCU[9] = 0;
    pEnd2 = (uint8_t *)&cZigZag2[5];       // we only need to store the 4 elements we care about
  } else {                                 // decode all the AC coefficients
    memset(pMCU, 0, 64 * sizeof(int16_t)); // pre-fill with zero since we may skip coefficients
    pEnd2 = (uint8_t *)&cZigZag2[64];
  }

  u16MCUFlags = 0;
  pZig        = (uint8_t *)&cZigZag2[1];
  pEnd        = (uint8_t *)&cZigZag2[64];

  // get the DC component
  pucFast = &ucHuffDC[ucDCTable * DC_TABLE_SIZE];
  ulCode  = (ulBits >> (32 - 12 - ulBitOff)) & 0xfff; // get as lower 12 bits
  if (ulCode >= 0xf80) {
    // it's a long code
    // point to long table and trim to 7-bits + 0x80 offset into long table
    ulCode = (ulCode & 0xff);
  } else {
    // it's a short code, use first 6 bits only
    ulCode >>= 6;
  }

  ucHuff = pucFast[ulCode];
  cCoeff = (signed char)pucFast[ulCode + 512]; // get pre-calculated extra bits for "small" values

  if (ucHuff == 0) {
    // invalid code
    return -1;
  }
  ulBitOff += (ucHuff >> 4); // add the Huffman length
  ucHuff &= 0xf;             // get the actual code (SSSS)
  if (ucHuff) {
    // if there is a change to the DC value
    // get the 'extra' bits
    if (cCoeff) {
      (*iDCPredictor) += cCoeff;
    } else {
      if (ulBitOff > (32 - 17)) {
        // need to get more data
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
      ulCode = ulBits << ulBitOff;
      ulTemp = ~(uint32_t)(((int32_t)ulCode) >> (32 - 1)); // slide sign bit across other 63/31 bits
      ulCode >>= (32 - ucHuff);
      ulCode -= ulTemp >> (32 - ucHuff);
      ulBitOff += ucHuff; // add bit length
      (*iDCPredictor) += (int32_t)ulCode;
    }
  }
  if (iMCU >= 0) {                    // non-skipped block
    pMCU[0] = (int16_t)*iDCPredictor; // store in MCU[0]
  }
  if (ucACTable > 1) {
    return -1; // unsupported
  }
  if (iScanEnd == 0) { // first scan of progressive has only DC values
    return 0;          // we're done
  }
  // Now get the other 63 AC coefficients
  pFast = &usHuffAC[ucACTable * HUFF11SIZE];
  if (b11Bit) {
    // 11-bit "slow" tables used

    //            if (pHuffACFast == huffacFast[1]) // second table
    //                pFast = &ucAltHuff[0];
    while (pZig < pEnd) {
      if (ulBitOff > (32 - 17)) {
        // need to get more data
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
      ulCode = (ulBits >> (32 - 16 - ulBitOff)) & 0xffff; // get as lower 16 bits
      if (ulCode >= 0xf000) {
        // first 4 bits = 1, use long table
        ulCode = (ulCode & 0x1fff);
      } else {
        // use lower 12 bits (short table)
        ulCode >>= 4;
      }
      usHuff = pFast[ulCode];
      if (usHuff == 0) // invalid code
        return -1;
      ulBitOff += (usHuff >> 8); // add length
      usHuff &= 0xff;            // get code (RRRR/SSSS)
      if (usHuff == 0) {
        // no more AC components
        goto mcu_done;
      }
      pZig += (usHuff >> 4); // get the skip amount (RRRR)
      usHuff &= 0xf;         // get (SSSS) - extra length
      if (pZig < pEnd2 && usHuff) {
        ulCode = ulBits << ulBitOff;
        ulTemp = ~(uint32_t)(((int32_t)ulCode) >> (32 - 1)); // slide sign bit across other 63 bits
        ulCode >>= (32 - usHuff);
        ulCode -= ulTemp >> (32 - usHuff);
        u16MCUFlags |= 1 << (*pZig & 7); // keep track of occupied columns
        u16MCUFlags |= *pZig << 8;       // for testing occupied rows
        pMCU[*pZig] = (int16_t)ulCode;   // store AC coefficient (already reordered)
      }
      ulBitOff += usHuff; // add (SSSS) extra length
      pZig++;
      if (ulBitOff > (32 - 17)) {
        // need to get more data
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
    } // while
  } else {
    // 10-bit "fast" tables used
    while (pZig < pEnd) {
      if (ulBitOff > (32 - 17)) // need to get more data
      {
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
      ulCode = (ulBits >> (32 - 16 - ulBitOff)) & 0xffff; // get as lower 16 bits
      if (ulCode >= 0xfc00)                               // first 6 bits = 1, use long table
        ulCode = (ulCode & 0x7ff);                        // (ulCode & 0x3ff) + 0x400;
      else
        ulCode >>= 6; // use lower 10 bits (short table)
      usHuff = pFast[ulCode];
      if (usHuff == 0) // invalid code
        return -1;
      ulBitOff += (usHuff >> 8); // add length
      usHuff &= 0xff;            // get code (RRRR/SSSS)
      if (usHuff == 0)           // no more AC components
      {
        goto mcu_done;
      }
      pZig += (usHuff >> 4); // get the skip amount (RRRR)
      usHuff &= 0xf;         // get (SSSS) - extra length
      if (pZig < pEnd2 && usHuff) {
        ulCode = ulBits << ulBitOff;
        ulTemp = ~(uint32_t)(((int32_t)ulCode) >> (32 - 1)); // slide sign bit across other 63 bits
        ulCode >>= (32 - usHuff);
        ulCode -= ulTemp >> (32 - usHuff);
        u16MCUFlags |= 1 << (*pZig & 7); // keep track of occupied columns
        u16MCUFlags |= *pZig << 8;       // for testing occupied rows
        pMCU[*pZig] = (int16_t)ulCode;   // store AC coefficient (already reordered)
      }
      ulBitOff += usHuff; // add (SSSS) extra length
      pZig++;
      if (ulBitOff > (32 - 17)) // need to get more data
      {
        pBuf += (ulBitOff >> 3);
        ulBitOff &= 7;
        ulBits = MOTOLONG(pBuf);
      }
    } // while
  } // 10-bit tables
mcu_done:
  bb.pBuf     = pBuf;
  iVLCOff     = (int32_t)(pBuf - ucFileBuf);
  bb.ulBitOff = ulBitOff;
  bb.ulBits   = ulBits;
  u16MCUFlags = u16MCUFlags;
  return 0;
}

//
// Inverse DCT
//
auto JPegImage::inverseDCT(int32_t iMCUOffset, int32_t iQuantTable) -> void {
  int32_t iRow;
  int32_t tmp6, tmp7, tmp10, tmp11, tmp12, tmp13;
  int32_t z5, z10, z11, z12, z13;
  int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  int16_t *pQuant;
  uint8_t *pOutput;
  int16_t *pMCUSrc = &sMCUs[iMCUOffset];

  // my shortcut method appears to violate patent 20020080052
  // but the patent is invalidated by prior art:
  // http://netilium.org/~mad/dtj/DTJ/DTJK04/
  pQuant = &sQuantTable[iQuantTable * DCTSIZE];
  if (iOptions.test(JPEG_OPTION(SCALE_QUARTER))) {
    // special case
    /* Column 0 */
    tmp4 = pMCUSrc[0] * pQuant[0];
    tmp5 = pMCUSrc[8] * pQuant[8];
    tmp0 = tmp4 + tmp5;
    tmp2 = tmp4 - tmp5;
    /* Column 1 */
    tmp4 = pMCUSrc[1] * pQuant[1];
    tmp5 = pMCUSrc[9] * pQuant[9];
    tmp1 = tmp4 + tmp5;
    tmp3 = tmp4 - tmp5;
    /* Pass 2: process 2 rows, store into output array. */
    /* Row 0 */
    pOutput    = (uint8_t *)pMCUSrc; // store output pixels back into MCU
    pOutput[0] = ucRangeTable[(((tmp0 + tmp1) >> 5) & 0x3ff)];
    pOutput[1] = ucRangeTable[(((tmp0 - tmp1) >> 5) & 0x3ff)];
    /* Row 1 */
    pOutput[2] = ucRangeTable[(((tmp2 + tmp3) >> 5) & 0x3ff)];
    pOutput[3] = ucRangeTable[(((tmp2 - tmp3) >> 5) & 0x3ff)];
    return;
  }

  // do columns first
  u16MCUFlags |= 1; // column 0 must always be calculated
  for (int32_t iCol = 0; iCol < 8 && u16MCUFlags; iCol++) {
    if (u16MCUFlags & (1 << iCol)) {
      // column has data in it
      u16MCUFlags &= ~(1 << iCol); // unmark the col after done
      if ((u16MCUFlags & 0x2000) == 0) {
        // simpler calculations if only half populated
        // even part
        tmp10 = pMCUSrc[iCol] * pQuant[iCol];
        tmp1  = pMCUSrc[iCol + 16] * pQuant[iCol + 16]; // get 2nd row
        tmp12 = ((tmp1 * 106) >> 8);                    // used to be 362 - 1 (256)
        tmp0  = tmp10 + tmp1;
        tmp3  = tmp10 - tmp1;
        tmp1  = tmp10 + tmp12;
        tmp2  = tmp10 - tmp12;
        // odd part
        tmp4 = pMCUSrc[iCol + 8] * pQuant[iCol + 8]; // get 1st row
        tmp5 = pMCUSrc[iCol + 24];
        if (tmp5) {
          // this value is usually 0
          tmp5 *= pQuant[iCol + 24]; // get 3rd row
          tmp7  = tmp4 + tmp5;
          tmp11 = (((tmp4 - tmp5) * 362) >> 8); // 362>>8 = 1.414213562
          z5    = (((tmp4 - tmp5) * 473) >> 8); // 473>>8 = 1.8477
          tmp12 = ((-tmp5 * -669) >> 8) + z5;   // -669>>8 = -2.6131259
          tmp6  = tmp12 - tmp7;
          tmp5  = tmp11 - tmp6;
          tmp10 = ((tmp4 * 277) >> 8) - z5; // 277>>8 = 1.08239
          tmp4  = tmp10 + tmp5;
        } else {
          // simpler case when we only have 1 odd row to calculate
          tmp7 = tmp4;
          tmp5 = (145 * tmp4) >> 8;
          tmp6 = (217 * tmp4) >> 8;
          tmp4 = (-51 * tmp4) >> 8;
        }
        pMCUSrc[iCol]      = (int16_t)(tmp0 + tmp7); // row0
        pMCUSrc[iCol + 8]  = (int16_t)(tmp1 + tmp6); // row 1
        pMCUSrc[iCol + 16] = (int16_t)(tmp2 + tmp5); // row 2
        pMCUSrc[iCol + 24] = (int16_t)(tmp3 - tmp4); // row 3
        pMCUSrc[iCol + 32] = (int16_t)(tmp3 + tmp4); // row 4
        pMCUSrc[iCol + 40] = (int16_t)(tmp2 - tmp5); // row 5
        pMCUSrc[iCol + 48] = (int16_t)(tmp1 - tmp6); // row 6
        pMCUSrc[iCol + 56] = (int16_t)(tmp0 - tmp7); // row 7
      } else {
        // need to do the full calculation
        // even part
        tmp0 = pMCUSrc[iCol] * pQuant[iCol];
        tmp2 = pMCUSrc[iCol + 32]; // get 4th row
        if (tmp2) {
          // 4th row is most likely 0

          tmp2  = tmp2 * pQuant[iCol + 32];
          tmp10 = tmp0 + tmp2;
          tmp11 = tmp0 - tmp2;
        } else {
          tmp10 = tmp11 = tmp0;
        }
        tmp1 = pMCUSrc[iCol + 16] * pQuant[iCol + 16]; // get 2nd row
        tmp3 = pMCUSrc[iCol + 48];                     // get 6th row
        if (tmp3) {
          // 6th row is most likely 0
          tmp3  = tmp3 * pQuant[iCol + 48];
          tmp13 = tmp1 + tmp3;
          tmp12 = (((tmp1 - tmp3) * 362) >> 8) - tmp13; // 362>>8 = 1.414213562
        } else {
          tmp13 = tmp1;
          tmp12 = ((tmp1 * 362) >> 8) - tmp1;
        }
        tmp0 = tmp10 + tmp13;
        tmp3 = tmp10 - tmp13;
        tmp1 = tmp11 + tmp12;
        tmp2 = tmp11 - tmp12;
        // odd part
        tmp5 = pMCUSrc[iCol + 24] * pQuant[iCol + 24]; // get 3rd row
        tmp6 = pMCUSrc[iCol + 40];                     // get 5th row
        if (tmp6) {
          // very likely that row 5 = 0
          tmp6 = tmp6 * pQuant[iCol + 40];
          z13  = tmp6 + tmp5;
          z10  = tmp6 - tmp5;
        } else {
          z13 = tmp5;
          z10 = -tmp5;
        }
        tmp4 = pMCUSrc[iCol + 8] * pQuant[iCol + 8]; // get 1st row
        tmp7 = pMCUSrc[iCol + 56];                   // get 7th row
        if (tmp7) {
          // very likely that row 7 = 0
          tmp7 = tmp7 * pQuant[iCol + 56];
          z11  = tmp4 + tmp7;
          z12  = tmp4 - tmp7;
        } else {
          z11 = z12 = tmp4;
        }
        tmp7               = z11 + z13;
        tmp11              = (((z11 - z13) * 362) >> 8); // 362>>8 = 1.414213562
        z5                 = (((z10 + z12) * 473) >> 8); // 473>>8 = 1.8477
        tmp12              = ((z10 * -669) >> 8) + z5;   // -669>>8 = -2.6131259
        tmp6               = tmp12 - tmp7;
        tmp5               = tmp11 - tmp6;
        tmp10              = ((z12 * 277) >> 8) - z5; // 277>>8 = 1.08239
        tmp4               = tmp10 + tmp5;
        pMCUSrc[iCol]      = (int16_t)(tmp0 + tmp7); // row0
        pMCUSrc[iCol + 8]  = (int16_t)(tmp1 + tmp6); // row 1
        pMCUSrc[iCol + 16] = (int16_t)(tmp2 + tmp5); // row 2
        pMCUSrc[iCol + 24] = (int16_t)(tmp3 - tmp4); // row 3
        pMCUSrc[iCol + 32] = (int16_t)(tmp3 + tmp4); // row 4
        pMCUSrc[iCol + 40] = (int16_t)(tmp2 - tmp5); // row 5
        pMCUSrc[iCol + 48] = (int16_t)(tmp1 - tmp6); // row 6
        pMCUSrc[iCol + 56] = (int16_t)(tmp0 - tmp7); // row 7
      } // full calculation needed
    } // if column has data in it
  } // for each column

  // now do rows
  u16MCUFlags = u16MCUFlags;
  pOutput     = (uint8_t *)pMCUSrc; // store output pixels back into MCU
  for (iRow = 0; iRow < 64; iRow += 8) {
    // all rows must be calculated
    // even part
    if ((u16MCUFlags & 0xf0) == 0) {
      // quick and dirty calculation (right 4 columns are all 0's)
      if ((u16MCUFlags & 0xfc) == 0) {
        // very likely case (1 or 2 columns occupied)
        // even part
        tmp0 = tmp1 = tmp2 = tmp3 = pMCUSrc[iRow + 0];
        // odd part
        tmp7 = pMCUSrc[iRow + 1];
        tmp6 = (tmp7 * 217) >> 8;   // * 0.8477
        tmp5 = (tmp7 * 145) >> 8;   // * 0.5663
        tmp4 = -((tmp7 * 51) >> 8); // * -0.199
      } else {
        tmp10 = pMCUSrc[iRow + 0];
        tmp13 = pMCUSrc[iRow + 2];
        tmp12 = ((tmp13 * 106) >> 8); // 2-6 * 1.414
        tmp0  = tmp10 + tmp13;
        tmp3  = tmp10 - tmp13;
        tmp1  = tmp10 + tmp12;
        tmp2  = tmp10 - tmp12;
        // odd part
        z13   = pMCUSrc[iRow + 3];
        z11   = pMCUSrc[iRow + 1];
        tmp7  = z11 + z13;
        tmp11 = ((z11 - z13) * 362) >> 8; // * 1.414
        z5    = ((z11 - z13) * 473) >> 8; // * 1.8477
        tmp10 = ((z11 * 277) >> 8) - z5;  // * 1.08239
        tmp12 = ((z13 * 669) >> 8) + z5;  // * 2.61312
        tmp6  = tmp12 - tmp7;
        tmp5  = tmp11 - tmp6;
        tmp4  = tmp10 + tmp5;
      }
    } else {
      // need to do the full calculation
      tmp10 = pMCUSrc[iRow + 0] + pMCUSrc[iRow + 4];
      tmp11 = pMCUSrc[iRow + 0] - pMCUSrc[iRow + 4];
      tmp13 = pMCUSrc[iRow + 2] + pMCUSrc[iRow + 6];
      tmp12 = (((pMCUSrc[iRow + 2] - pMCUSrc[iRow + 6]) * 362) >> 8) - tmp13; // 2-6 * 1.414
      tmp0  = tmp10 + tmp13;
      tmp3  = tmp10 - tmp13;
      tmp1  = tmp11 + tmp12;
      tmp2  = tmp11 - tmp12;
      // odd part
      z13   = pMCUSrc[iRow + 5] + pMCUSrc[iRow + 3];
      z10   = pMCUSrc[iRow + 5] - pMCUSrc[iRow + 3];
      z11   = pMCUSrc[iRow + 1] + pMCUSrc[iRow + 7];
      z12   = pMCUSrc[iRow + 1] - pMCUSrc[iRow + 7];
      tmp7  = z11 + z13;
      tmp11 = ((z11 - z13) * 362) >> 8; // * 1.414
      z5    = ((z10 + z12) * 473) >> 8; // * 1.8477
      tmp10 = ((z12 * 277) >> 8) - z5;  // * 1.08239
      tmp12 = ((z10 * -669) >> 8) + z5; // * 2.61312
      tmp6  = tmp12 - tmp7;
      tmp5  = tmp11 - tmp6;
      tmp4  = tmp10 + tmp5;
    }
    // final output stage - scale down and range limit

    // I've tried various things to speed this up, but it always seems to take the same amount of
    // time

    pOutput[0] = ucRangeTable[(((tmp0 + tmp7) >> 5) & 0x3ff)];
    pOutput[1] = ucRangeTable[(((tmp1 + tmp6) >> 5) & 0x3ff)];
    pOutput[2] = ucRangeTable[(((tmp2 + tmp5) >> 5) & 0x3ff)];
    pOutput[3] = ucRangeTable[(((tmp3 - tmp4) >> 5) & 0x3ff)];
    pOutput[4] = ucRangeTable[(((tmp3 + tmp4) >> 5) & 0x3ff)];
    pOutput[5] = ucRangeTable[(((tmp2 - tmp5) >> 5) & 0x3ff)];
    pOutput[6] = ucRangeTable[(((tmp1 - tmp6) >> 5) & 0x3ff)];
    pOutput[7] = ucRangeTable[(((tmp0 - tmp7) >> 5) & 0x3ff)];

    pOutput += 8;
  } // for each row
}

auto JPegImage::putMCU8BitGray(int32_t x, int32_t iPitch) -> void {
  int32_t i, j, xcount, ycount;
  uint8_t *pDest, *pSrc = (uint8_t *)&sMCUs[0];

  if (pDitherBuffer) {
    pDest = &pDitherBuffer[x];
  } else {
    pDest = (uint8_t *)&usPixels[0];
    pDest += x;
  }
  if (ucSubSample <= 0x11) // single Y
  {
    if (iOptions.test(std::to_underlying(
            JPegOption::SCALE_HALF))) // special handling of 1/2 size (pixel averaging)
    {
      int32_t pix;
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          pix      = (pSrc[0] + pSrc[1] + pSrc[8] + pSrc[9] + 2) >> 2; // average 2x2 block
          pDest[j] = (uint8_t)pix;
          pSrc += 2;
        }
        pSrc += 8; // skip extra line
        pDest += iPitch;
      }
      return;
    }
    xcount = ycount = 8; // debug
    if (iOptions.test(JPEG_OPTION(SCALE_QUARTER)))
      xcount = ycount = 2;
    else if (iOptions.test(JPEG_OPTION(SCALE_EIGHTH)))
      xcount = ycount = 1;
    for (i = 0; i < ycount; i++) // do up to 8 rows
    {
      for (j = 0; j < xcount; j++) *pDest++ = *pSrc++;
      pDest -= xcount;
      pDest += iPitch; // next line
    }
    return;
  } // single Y source
  if (ucSubSample == 0x21) // stacked horizontally
  {
    if (iOptions.test(JPEG_OPTION(SCALE_EIGHTH))) {
      // only 2 pixels emitted
      pDest[0] = pSrc[0];
      pDest[1] = pSrc[128];
      return;
    } /* 1/8 */
    if (iOptions.test(JPEG_OPTION(SCALE_HALF))) {
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          int32_t pix;
          pix      = (pSrc[j * 2] + pSrc[j * 2 + 1] + pSrc[j * 2 + 8] + pSrc[j * 2 + 9] + 2) >> 2;
          pDest[j] = (uint8_t)pix;
          pix =
              (pSrc[j * 2 + 128] + pSrc[j * 2 + 129] + pSrc[j * 2 + 136] + pSrc[j * 2 + 137] + 2) >>
              2;
          pDest[j + 4] = (uint8_t)pix;
        }
        pSrc += 16;
        pDest += iPitch;
      }
      return;
    }
    if (iOptions.test(JPEG_OPTION(SCALE_QUARTER))) {
      // each MCU contributes a 2x2 block
      pDest[0]          = pSrc[0]; // Y0
      pDest[1]          = pSrc[1];
      pDest[iPitch]     = pSrc[2];
      pDest[iPitch + 1] = pSrc[3];

      pDest[2]          = pSrc[128]; // Y`
      pDest[3]          = pSrc[129];
      pDest[iPitch + 2] = pSrc[130];
      pDest[iPitch + 3] = pSrc[131];
      return;
    }
    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
        pDest[j]     = pSrc[j];
        pDest[j + 8] = pSrc[128 + j];
      }
      pSrc += 8;
      pDest += iPitch;
    }
  } // 0x21
  if (ucSubSample == 0x12) // stacked vertically
  {
    if (iOptions.test(JPEG_OPTION(SCALE_EIGHTH))) {
      // only 2 pixels emitted
      pDest[0]      = pSrc[0];
      pDest[iPitch] = pSrc[128];
      return;
    } /* 1/8 */
    if (iOptions.test(JPEG_OPTION(SCALE_HALF))) {
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          int32_t pix;
          pix      = (pSrc[j * 2] + pSrc[j * 2 + 1] + pSrc[j * 2 + 8] + pSrc[j * 2 + 9] + 2) >> 2;
          pDest[j] = (uint8_t)pix;
          pix =
              (pSrc[j * 2 + 128] + pSrc[j * 2 + 129] + pSrc[j * 2 + 136] + pSrc[j * 2 + 137] + 2) >>
              2;
          pDest[4 * iPitch + j] = (uint8_t)pix;
        }
        pSrc += 16;
        pDest += iPitch;
      }
      return;
    }

    if (iOptions.test(JPEG_OPTION(SCALE_QUARTER))) {
      // each MCU contributes a 2x2 block
      pDest[0]          = pSrc[0]; // Y0
      pDest[1]          = pSrc[1];
      pDest[iPitch]     = pSrc[2];
      pDest[iPitch + 1] = pSrc[3];

      pDest[iPitch * 2]     = pSrc[128]; // Y`
      pDest[iPitch * 2 + 1] = pSrc[129];
      pDest[iPitch * 3]     = pSrc[130];
      pDest[iPitch * 3 + 1] = pSrc[131];
      return;
    }

    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
        pDest[j]              = pSrc[j];
        pDest[8 * iPitch + j] = pSrc[128 + j];
      }
      pSrc += 8;
      pDest += iPitch;
    }
  } // 0x12

  if (ucSubSample == 0x22) {
    if (iOptions.test(JPEG_OPTION(SCALE_EIGHTH))) {
      // each MCU contributes 1 pixel
      pDest[0]          = pSrc[0];   // Y0
      pDest[1]          = pSrc[128]; // Y1
      pDest[iPitch]     = pSrc[256]; // Y2
      pDest[iPitch + 1] = pSrc[384]; // Y3
      return;
    }

    if (iOptions.test(JPEG_OPTION(SCALE_QUARTER))) {
      // each MCU contributes 2x2 pixels
      pDest[0]          = pSrc[0]; // Y0
      pDest[1]          = pSrc[1];
      pDest[iPitch]     = pSrc[2];
      pDest[iPitch + 1] = pSrc[3];

      pDest[2]          = pSrc[128]; // Y1
      pDest[3]          = pSrc[129];
      pDest[iPitch + 2] = pSrc[130];
      pDest[iPitch + 3] = pSrc[131];

      pDest[iPitch * 2]     = pSrc[256]; // Y2
      pDest[iPitch * 2 + 1] = pSrc[257];
      pDest[iPitch * 3]     = pSrc[258];
      pDest[iPitch * 3 + 1] = pSrc[259];

      pDest[iPitch * 2 + 2] = pSrc[384]; // Y3
      pDest[iPitch * 2 + 3] = pSrc[385];
      pDest[iPitch * 3 + 2] = pSrc[386];
      pDest[iPitch * 3 + 3] = pSrc[387];
      return;
    }

    if (iOptions.test(JPEG_OPTION(SCALE_HALF))) {
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          int32_t pix;
          pix      = (pSrc[j * 2] + pSrc[j * 2 + 1] + pSrc[j * 2 + 8] + pSrc[j * 2 + 9] + 2) >> 2;
          pDest[j] = (uint8_t)pix; // Y0
          pix =
              (pSrc[j * 2 + 128] + pSrc[j * 2 + 129] + pSrc[j * 2 + 136] + pSrc[j * 2 + 137] + 2) >>
              2;
          pDest[j + 4] = (uint8_t)pix; // Y1
          pix =
              (pSrc[j * 2 + 256] + pSrc[j * 2 + 257] + pSrc[j * 2 + 264] + pSrc[j * 2 + 265] + 2) >>
              2;
          pDest[iPitch * 4 + j] = (uint8_t)pix; // Y2
          pix =
              (pSrc[j * 2 + 384] + pSrc[j * 2 + 385] + pSrc[j * 2 + 392] + pSrc[j * 2 + 393] + 2) >>
              2;
          pDest[iPitch * 4 + j + 4] = (uint8_t)pix; // Y3
        }
        pSrc += 16;
        pDest += iPitch;
      }
      return;
    }

    xcount = iPitch - x;

    for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
        if (j < xcount) {
          pDest[j]              = pSrc[j];       // Y0
          pDest[iPitch * 8 + j] = pSrc[j + 256]; // Y2
        }
        if (j + 8 < xcount) {
          pDest[j + 8]              = pSrc[j + 128]; // Y1
          pDest[iPitch * 8 + j + 8] = pSrc[j + 384]; // Y3
        }
      }
      pSrc += 8;
      pDest += iPitch;
    }
  } // 0x22
}

// Dither the 8-bit gray pixels into 1, 2, or 4-bit gray
auto JPegImage::ditherImage(int32_t iWidth, int32_t iHeight) -> void {
  int32_t x, y, xmask = 0, iDestPitch = 0;
  int32_t cNew, lFErr, v = 0, h;
  int32_t e1, e2, e3, e4;
  uint8_t cOut;                                                     // forward errors for gray
  uint8_t *pSrc, *pDest, *errors, *pErrors = nullptr, *d, *pPixels; // destination 8bpp image
  uint8_t pixelmask = 0, shift = 0;

  errors    = (uint8_t *)usPixels; // plenty of space here
  errors[0] = errors[1] = errors[2] = 0;
  pDest = pSrc = pDitherBuffer.get(); // write the new pixels over the original

  switch (ucPixelType) {
  case JPegPixelType::FOUR_BIT_DITHERED:
    iDestPitch = (iWidth + 1) / 2;
    pixelmask  = 0xf0;
    shift      = 4;
    xmask      = 1;
    break;
  case JPegPixelType::TWO_BIT_DITHERED:
    iDestPitch = (iWidth + 3) / 4;
    pixelmask  = 0xc0;
    shift      = 2;
    xmask      = 3;
    break;
  case JPegPixelType::ONE_BIT_DITHERED:
    iDestPitch = (iWidth + 7) / 8;
    pixelmask  = 0x80;
    shift      = 1;
    xmask      = 7;
    break;
  default:
    return; // not a dithered output format
  }

  for (y = 0; y < iHeight; y++) {
    pPixels = &pSrc[y * iWidth];
    d       = &pDest[y * iDestPitch];
    pErrors = &errors[1]; // point to second pixel to avoid boundary check
    lFErr   = 0;
    cOut    = 0;
    for (x = 0; x < iWidth; x++) {
      cNew = *pPixels++; // get grayscale uint8_t pixel
      // add forward error
      cNew += lFErr;
      if (cNew > 255) cNew = 255;    // clip to uint8_t
      cOut <<= shift;                // pack new pixels into a byte
      cOut |= (cNew >> (8 - shift)); // keep top N bits
      if ((x & xmask) == xmask)      // store it when the byte is full
      {
        *d++ = cOut;
        cOut = 0;
      }
      // calculate the Floyd-Steinberg error for this pixel
      v  = cNew - (cNew & pixelmask); // new error for N-bit gray output (always positive)
      h  = v >> 1;
      e1 = (7 * h) >> 3; // 7/16
      e2 = h - e1;       // 1/16
      e3 = (5 * h) >> 3; // 5/16
      e4 = h - e3;       // 3/16
      // distribute error to neighbors
      lFErr      = e1 + pErrors[1];
      pErrors[1] = (uint8_t)e2;
      pErrors[0] += e3;
      pErrors[-1] += e4;
      pErrors++;
    } // for x
  } // for y
}

//
// Decode the image
// returns 0 for error, 1 for success
//
auto JPegImage::decodeImage() -> int32_t {
  int32_t cx, cy, x, y, mcuCX, mcuCY;
  int32_t iLum0, iLum1, iLum2, iLum3, iCr, iCb;
  int32_t iDCPred0, iDCPred1, iDCPred2;
  int32_t i, iQuant1, iQuant2, iQuant3, iErr;
  int32_t iSkipMask, bSkipRow;
  int32_t iDMASize, iDMAOffset;
  uint16_t *pAlignedPixels = usPixels;
  uint8_t c;
  int32_t iMCUCount, xoff, iPitch, bThumbnail = 0;
  int32_t bContinue = 1; // early exit if the DRAW callback wants to stop
  uint32_t l, *pl;
  uint8_t cDCTable0, cACTable0, cDCTable1, cACTable1, cDCTable2, cACTable2;
  JPegDraw jd;
  int32_t iMaxFill = 16, iScaleShift = 0;

  // Requested the Exif thumbnail
  if (ucMode == 0xc2) { // progressive mode - we only decode the first scan (DC values)
    iOptions.set(JPEG_OPTION(SCALE_EIGHTH)); // return 1/8 sized image
  }
  if (iOptions.test(JPEG_OPTION(EXIF_THUMBNAIL))) {
    if (iThumbData == 0 || iThumbWidth == 0) // doesn't exist
    {
      iError = JPegError::INVALID_PARAM;
      return 0;
    }
    if (!parseInfo(1)) // parse the embedded thumbnail file header
      return 0;        // something went wrong
  }
  // Fast downscaling options
  if (iOptions.test(JPEG_OPTION(SCALE_HALF)))
    iScaleShift = 1;
  else if (iOptions.test(JPEG_OPTION(SCALE_QUARTER))) {
    iScaleShift = 2;
    iMaxFill    = 1;
  } else if (iOptions.test(JPEG_OPTION(SCALE_EIGHTH))) {
    iScaleShift = 3;
    iMaxFill    = 1;
    bThumbnail  = 1;
  }

  // reorder and fix the quantization table for decoding
  fixQuantD();
  bb.ulBits   = MOTOLONG(&ucFileBuf[0]); // preload first 4/8 bytes
  bb.pBuf     = ucFileBuf;
  bb.ulBitOff = 0;

  cDCTable0 = compInfo[0].dc_tbl_no;
  cACTable0 = compInfo[0].ac_tbl_no;
  cDCTable1 = compInfo[1].dc_tbl_no;
  cACTable1 = compInfo[1].ac_tbl_no;
  cDCTable2 = compInfo[2].dc_tbl_no;
  cACTable2 = compInfo[2].ac_tbl_no;
  iDCPred0 = iDCPred1 = iDCPred2 = mcuCX = mcuCY = 0;

  switch (ucSubSample) // set up the parameters for the different subsampling options
  {
  case 0x00: // fake value to handle grayscale
  case 0x01: // fake value to handle sRGB/CMYK
  case 0x11:
    cx    = (iWidth + 7) >> 3; // number of MCU blocks
    cy    = (iCropY + iCropCY + 7) >> 3;
    iCr   = MCU1;
    iCb   = MCU2;
    mcuCX = mcuCY = 8;
    break;
  case 0x12:
    cx    = (iWidth + 7) >> 3; // number of MCU blocks
    cy    = (iCropY + iCropCY + 15) >> 4;
    iCr   = MCU2;
    iCb   = MCU3;
    mcuCX = 8;
    mcuCY = 16;
    break;
  case 0x21:
    cx    = (iWidth + 15) >> 4; // number of MCU blocks
    cy    = (iCropY + iCropCY + 7) >> 3;
    iCr   = MCU2;
    iCb   = MCU3;
    mcuCX = 16;
    mcuCY = 8;
    break;
  case 0x22:
    cx    = (iWidth + 15) >> 4; // number of MCU blocks
    cy    = (iCropY + iCropCY + 15) >> 4;
    iCr   = MCU4;
    iCb   = MCU5;
    mcuCX = mcuCY = 16;
    break;
  default: // to suppress compiler warning
    cx = cy = 0;
    iCr = iCb = 0;
    break;
  }
  // Scale down the MCUs by the requested amount
  mcuCX >>= iScaleShift;
  mcuCY >>= iScaleShift;

  iQuant1 = sQuantTable[compInfo[0].quant_tbl_no * DCTSIZE]; // DC quant values
  iQuant2 = sQuantTable[compInfo[1].quant_tbl_no * DCTSIZE];
  iQuant3 = sQuantTable[compInfo[2].quant_tbl_no * DCTSIZE];
  // luminance values are always in these positions
  iLum0     = MCU0;
  iLum1     = MCU1;
  iLum2     = MCU2;
  iLum3     = MCU3;
  iErr      = 0;
  iResCount = iResInterval;
  // Calculate how many MCUs we can fit in the pixel buffer to maximize LCD drawing speed
  iMCUCount = MAX_BUFFERED_PIXELS / (mcuCX * mcuCY);

  iDMASize = iDMAOffset = 0; // assume no DMA scheme
  if (ucPixelType == JPegPixelType::EIGHT_BIT_GRAYSCALE)
    iMCUCount *= 2;                   // each pixel is only 1 byte
  if (iMCUCount > cx) iMCUCount = cx; // don't go wider than the image

  // did the user set an upper bound on how many pixels per JPEGDraw callback?
  if (iMCUCount > iMaxMCUs) {
    iMCUCount = iMaxMCUs;
  } else if (iOptions.test(JPEG_OPTION(USES_DMA))) {
    // user wants a ping-pong buffer scheme
    // divide the pixel buffer in half
    iMCUCount /= 2;
    // offset to the second half of the buffer
    iDMASize = MAX_BUFFERED_PIXELS / 2;
  }

  if (ucPixelType != JPegPixelType::EIGHT_BIT_GRAYSCALE) {
    // dithered, override the max MCU count
    // do the whole row
    iMCUCount = cx;
  }

  if (iCropCX != iWidth /*(cx * mcuCX)*/) {
    // crop enabled
    if (iMCUCount * mcuCX > iCropCX) {
      // maximum width is the crop width
      iMCUCount = (iCropCX / mcuCX);
    }
  }

  jd.iBpp = 16;

  switch (ucPixelType) {
  case JPegPixelType::EIGHT_BIT_GRAYSCALE:
    jd.iBpp = 8;
    break;
  case JPegPixelType::FOUR_BIT_DITHERED:
    jd.iBpp = 4;
    break;
  case JPegPixelType::TWO_BIT_DITHERED:
    jd.iBpp = 2;
    break;
  case JPegPixelType::ONE_BIT_DITHERED:
    jd.iBpp = 1;
    break;
  }

  if (ucPixelType != JPegPixelType::EIGHT_BIT_GRAYSCALE) {
    jd.pPixels = (uint16_t *)pDitherBuffer.get();
  } else {
    jd.pPixels = usPixels;
  }

  jd.iHeight = mcuCY;

  for (y = 0; y < cy && bContinue && iErr == 0; y++) {
    bSkipRow = (y * mcuCY < iCropY);
    jd.x     = iXOffset;
    xoff     = 0; // start of new LCD output group

    if (pFramebuffer) {
      // user-supplied buffer is full width
      int32_t ty = (y * mcuCY) - iCropY;

      iPitch   = iCropCX; // size of cropped width
      usPixels = (uint16_t *)((uint8_t *)pFramebuffer + (ty * iPitch / 2));
      usPixels += (ty * iPitch / 2);
    } else {
      // use our internal buffer to do it a block at a time
      // pixels per line of LCD buffer
      iPitch = iMCUCount * mcuCX;
    }
    for (x = 0; x < cx && bContinue && iErr == 0; x++) {
      if (pFramebuffer == nullptr) {
        // make sure output is correct offset for DMA
        usPixels = &pAlignedPixels[iDMAOffset];
      }

      iSkipMask = 0; // assume not skipping
      if (ucMode != 0x52 && (bSkipRow || x * mcuCX < iCropX || x * mcuCX > iCropX + iCropCX)) {
        iSkipMask = MCU_SKIP;
      }

      // set the AC and DC tables
      ucACTable = cACTable0;
      ucDCTable = cDCTable0;

      // do the first luminance component
      if (ucMode == 0xc2) {
        iErr = decodeMCUProgressive(iLum0 | iSkipMask, &iDCPred0);
      } else {
        iErr = decodeMCU(iLum0 | iSkipMask, &iDCPred0);
      }
      if (u16MCUFlags == 0 || bThumbnail) {
        // no AC components, save some time
        pl = (uint32_t *)&sMCUs[iLum0];
        c  = ucRangeTable[((iDCPred0 * iQuant1) >> 5) & 0x3ff];
        l  = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
        // dct stores byte values
        for (i = 0; i < iMaxFill; i++) {
          // 8x8 bytes = 16 longs
          pl[i] = l;
        }
      } else {
        inverseDCT(iLum0, compInfo[0].quant_tbl_no); // first quantization table
      }
      // do the second luminance component
      if (ucSubSample > 0x11) {
        // subsampling
        if (ucMode == 0xc2) {
          iErr |= decodeMCUProgressive(iLum1 | iSkipMask, &iDCPred0);
        } else {
          iErr |= decodeMCU(iLum1 | iSkipMask, &iDCPred0);
        }
        if (u16MCUFlags == 0 || bThumbnail) {
          // no AC components, save some time
          c = ucRangeTable[((iDCPred0 * iQuant1) >> 5) & 0x3ff];
          l = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
          // dct stores byte values
          pl = (uint32_t *)&sMCUs[iLum1];
          for (i = 0; i < iMaxFill; i++) // 8x8 bytes = 16 longs
            pl[i] = l;
        } else {
          inverseDCT(iLum1, compInfo[0].quant_tbl_no); // first quantization table
        }

        if (ucSubSample == 0x22) {
          if (ucMode == 0xc2) {
            iErr |= decodeMCUProgressive(iLum2 | iSkipMask, &iDCPred0);
          } else {
            iErr |= decodeMCU(iLum2 | iSkipMask, &iDCPred0);
          }

          if (u16MCUFlags == 0 || bThumbnail) {
            // no AC components, save some time
            c = ucRangeTable[((iDCPred0 * iQuant1) >> 5) & 0x3ff];
            l = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
            // dct stores byte values
            pl = (uint32_t *)&sMCUs[iLum2];
            for (i = 0; i < iMaxFill; i++) // 8x8 bytes = 16 longs
              pl[i] = l;
          } else {
            inverseDCT(iLum2, compInfo[0].quant_tbl_no); // first quantization table
          }

          if (ucMode == 0xc2) {
            iErr |= decodeMCUProgressive(iLum3 | iSkipMask, &iDCPred0);
          } else {
            iErr |= decodeMCU(iLum3 | iSkipMask, &iDCPred0);
          }

          if (u16MCUFlags == 0 || bThumbnail) {
            // no AC components, save some time
            c = ucRangeTable[((iDCPred0 * iQuant1) >> 5) & 0x3ff];
            l = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
            // dct stores byte values
            pl = (uint32_t *)&sMCUs[iLum3];
            for (i = 0; i < iMaxFill; i++) // 8x8 bytes = 16 longs
              pl[i] = l;
          } else {
            inverseDCT(iLum3, compInfo[0].quant_tbl_no); // first quantization table
          }
        } // if 2:2 subsampling
      } // if subsampling used
      if (ucSubSample && ucNumComponents == 3) {
        // if color (not CMYK)
        // first chroma
        ucACTable = cACTable1;
        ucDCTable = cDCTable1;
        if (ucPixelType != JPegPixelType::EIGHT_BIT_GRAYSCALE) {
          // We're not going to use the color channels, so avoid as much work as possible
          if (ucMode == 0xc2) {
            iErr |= decodeMCUProgressive(MCU_SKIP, &iDCPred1);
            iErr |= decodeMCUProgressive(MCU_SKIP, &iDCPred2);
          } else {
            iErr |= decodeMCU(MCU_SKIP, &iDCPred1); // decode Cr block
            iErr |= decodeMCU(MCU_SKIP, &iDCPred2); // decode Cb block
          }
        } else {
          if (ucMode == 0xc2) {
            iErr |= decodeMCUProgressive(iCr | iSkipMask, &iDCPred1);
          } else {
            iErr |= decodeMCU(iCr | iSkipMask, &iDCPred1);
          }
          if (u16MCUFlags == 0 || bThumbnail) {
            // no AC components, save some time
            c = ucRangeTable[((iDCPred1 * iQuant2) >> 5) & 0x3ff];
            l = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
            // dct stores byte values
            pl = (uint32_t *)&sMCUs[iCr];
            for (i = 0; i < iMaxFill; i++) {
              // 8x8 bytes = 16 longs
              pl[i] = l;
            }
          } else {
            inverseDCT(iCr, compInfo[1].quant_tbl_no); // second quantization table
          }
          // second chroma
          ucACTable = cACTable2;
          ucDCTable = cDCTable2;
          if (ucMode == 0xc2) {
            iErr |= decodeMCUProgressive(iCb | iSkipMask, &iDCPred2);
          } else {
            iErr |= decodeMCU(iCb | iSkipMask, &iDCPred2);
          }
          if (u16MCUFlags == 0 || bThumbnail) {
            // no AC components, save some time
            c = ucRangeTable[((iDCPred2 * iQuant3) >> 5) & 0x3ff];
            l = c | ((uint32_t)c << 8) | ((uint32_t)c << 16) | ((uint32_t)c << 24);
            // dct stores byte values
            pl = (uint32_t *)&sMCUs[iCb];
            for (i = 0; i < iMaxFill; i++) // 8x8 bytes = 16 longs
              pl[i] = l;
          } else {
            inverseDCT(iCb, compInfo[2].quant_tbl_no);
          }
        }
      }

      // if color components present
      if (!iSkipMask) {
        // this MCU is not skipped
        putMCU8BitGray(xoff, iPitch); // grayscale or color is being drawn as grayscale
        xoff += mcuCX;
      }

      // if not skipped
      if (pFramebuffer == nullptr && (xoff == iPitch || x == cx - 1) && !iSkipMask) {
        // time to draw
        int32_t iAdjust;
        int32_t iCurW, iCurH;

        iAdjust   = (1 << iScaleShift) - 1;
        iCurW     = (iWidth + iAdjust) >> iScaleShift;
        iCurH     = (iHeight + iAdjust) >> iScaleShift;
        jd.iWidth = jd.iWidthUsed = iPitch; // width of each LCD block group
        jd.pUser                  = pUser;

        if (ucPixelType != JPegPixelType::EIGHT_BIT_GRAYSCALE) {
          // dither to 4/2/1 bits
          ditherImage(cx * mcuCX, mcuCY);
        }

        if (((jd.x - iXOffset) + iPitch) > iCurW) {
          // right edge has clipped pixels
          jd.iWidthUsed = iCurW - (jd.x - iXOffset);
        } else if (((jd.x - iXOffset) + iPitch) > iCropCX) {
          // not a full width
          jd.iWidthUsed = iCropCX - (jd.x - iXOffset);
        }

        jd.y = iYOffset + (y * mcuCY) - iCropY;

        if ((jd.y - iYOffset + mcuCY) > iCurH) { // last row needs to be trimmed
          jd.iHeight = iCurH - (jd.y - iYOffset);
        }

        if (ucPixelType != JPegPixelType::EIGHT_BIT_GRAYSCALE)
          jd.pPixels = (uint16_t *)pDitherBuffer.get();
        else
          jd.pPixels = usPixels;

        bContinue = (*pfnDraw)(&jd);
        iDMAOffset ^= iDMASize; // toggle ping-pong offset
        jd.x += iPitch;

        if (iCropCX != (cx * mcuCX) && (iPitch + jd.x) > (iCropX + iCropCX)) {
          // image is cropped, don't go past end
          iPitch = iCropCX - (jd.x - iXOffset); // x=0 of output is really iCropx
        } else if ((cx - 1 - x) < iMCUCount) {
          // change pitch for the last set of MCUs on this row
          iPitch = (cx - 1 - x) * mcuCX;
        }

        xoff = 0;
        if (iPitch & (mcuCX - 1)) { // we don't clip the MCU drawing, so expand it
          iPitch = (iPitch + (mcuCX - 1)) & ~(mcuCX - 1);
        }
      }
      if (iResInterval) {
        if (--iResCount == 0) {
          iResCount = iResInterval;
          iDCPred0 = iDCPred1 = iDCPred2 = 0; // reset DC predictors
          if (bb.ulBitOff & 7)                // need to start at the next even byte
          {
            bb.ulBitOff += (8 - (bb.ulBitOff & 7)); // new restart interval starts on byte boundary
          }
        } // if restart interval needs to reset
      } // if there is a restart interval
      // See if we need to feed it more data
      if (iVLCOff >= FILE_HIGHWATER) getMoreData(); // need more 'filtered' VLC data
    } // for x
  } // for y
  if (iErr != 0) iError = JPegError::DECODE_ERROR;
  return (iErr == 0);
}
