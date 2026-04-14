// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _FONT_ 1
#include "models/font.hpp"
#include "viewers/msg_viewer.hpp"

#include "alloc.hpp"
#include "screen.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

Font::Font() {
  memoryFont      = nullptr;
  currentFontSize = -1;
  ready           = false;
}

void Font::addBuffToBytePool() {
  BytePool *pool = (BytePool *)allocate(BYTE_POOL_SIZE);
  if (pool == nullptr) {
    LOG_E("Unable to allocated memory for bytes pool.");
    MsgViewer::outOfMemory("ttf pool allocation");
  }
  bytePools.push_front(pool);

  bytePoolIdx = 0;
}

auto Font::bytePoolAlloc(uint16_t size) -> uint8_t * {
  if (size > BYTE_POOL_SIZE) {
    LOG_E("Byte Pool Size NOT BIG ENOUGH!!!");
    std::abort();
  }
  if (bytePools.empty() || (bytePoolIdx + size) > BYTE_POOL_SIZE) {
    LOG_D("Adding new Byte Pool buffer.");
    addBuffToBytePool();
  }

  uint8_t *buff = &(*bytePools.front())[bytePoolIdx];
  bytePoolIdx += size;

  return buff;
}

void Font::clearCache() {
  std::scoped_lock guard(mutex);

  LOG_D("Clear cache...");
  for (auto const &entry : cache) {
    for (auto const &glyph : entry.second) {
      bitmapGlyphPool.deleteElement(glyph.second);
    }
  }

  for (auto *buff : bytePools) {
    free(buff);
  }
  bytePools.clear();

  cache.clear();
  cache.reserve(50);
}

auto Font::getGlyph(uint32_t charcode, int16_t glyphSize) -> Glyph * {
  {
    std::scoped_lock guard(mutex);

    return ready ? getGlyphInternal(charcode, glyphSize) : nullptr;
  }
}

auto Font::getGlyph(uint32_t charcode, uint32_t nextCharcode, int16_t glyphSize, int16_t &kern,
                    bool &ignoreNext) -> Glyph * {
  {
    std::scoped_lock guard(mutex);

    ignoreNext   = false;
    Glyph *glyph = getGlyphInternal(charcode, glyphSize);

    if (glyph != nullptr) {
      if (glyph->ligatureAndKernPgmIndex >= 0) {
        int16_t k; // This is a FIX16...
        glyph = adjustLigatureAndKern(glyph, glyphSize, nextCharcode, k, ignoreNext);
        kern  = glyph->advance + k;
      } else {
        kern = glyph->advance;
      }
    }

    return (glyph == nullptr) ? nullptr : glyph;
  }
}

auto Font::setFontFaceFromFile(const std::string fontFilename) -> bool {
  LOG_D("setFontFaceFromFile() ...");

  FILE *fontFile;
  if ((fontFile = fopen(fontFilename.c_str(), "r")) == nullptr) {
    LOG_E("setFontFaceFromFile: Unable to open font file '%s'", fontFilename.c_str());
    return false;
  } else {
    struct stat statBuf;
    fstat(fileno(fontFile), &statBuf);
    int32_t length = statBuf.st_size;

    LOG_D("Font File Length: %" PRIi32, length);

    auto buffer = makeUniqueHimem<uint8_t[]>(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %" PRIi32, (int32_t)(length + 1));
      MsgViewer::outOfMemory("font buffer allocation");
    }

    if (fread(buffer.get(), length, 1, fontFile) != 1) {
      LOG_E("setFontFaceFromFile: Unable to read file content");
      fclose(fontFile);
      return false;
    }

    fclose(fontFile);

    buffer[length] = 0;

    if (setFontFaceFromMemory(std::move(buffer), length)) {
      return true;
    } else {
      LOG_E("Font Filename in trouble: '%s'", fontFilename.c_str());
      return false;
    }
  }
}

void Font::getSize(const char *str, Dim *dim, int16_t glyphSize) {
  {
    std::scoped_lock guard(mutex);

    int16_t maxUp   = 0;
    int16_t maxDown = 0;

    dim->width = 0;

    while (*str) {
      Glyph *glyph = getGlyphInternal(*str++, glyphSize);
      if (glyph != nullptr) {
        dim->width += glyph->advance;

        int16_t up   = -glyph->yoff;
        int16_t down = glyph->dim.height + glyph->yoff;

        if (up > maxUp) maxUp = up;
        if (down > maxDown) maxDown = down;
      }
    }

    dim->height = maxUp + maxDown;
  }
}