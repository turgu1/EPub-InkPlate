// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _FONT_ 1
#include "font.hpp"

#include "alloc.hpp"
#include "screen.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

Font::Font() {
  glyphsMapPool = CharPool::Make();
  if (!glyphsMapPool) {
    LOG_E("Unable to allocate memory for glyphs map pool.");
  }

  cache.reserve(50);
  currentFontSize = -1;
  ready           = false;
}

auto Font::getOrCreateGlyph(char32_t charcode, int16_t glyphSize) -> Glyph * {
  auto key = toCacheKey(glyphSize, currentSizeUnit);

  auto it = cache.find(key);
  if (it == cache.end()) {
    Glyphs glyphs(0, std::hash<uint32_t>{}, std::equal_to<uint32_t>{},
                  GlyphsAlloc{glyphsMapPool.get()});
    it = cache.emplace(key, std::move(glyphs)).first;
  }

  auto git = it->second.find(charcode);
  if (git != it->second.end()) return git->second;

  Glyph *glyph = it->second.emplace(charcode, bitmapGlyphPool.allocate()).first->second;

  if ((glyph == nullptr) || !getGlyphInternal(*glyph, charcode, glyphSize)) {
    if (glyph != nullptr) {
      bitmapGlyphPool.deleteElement(glyph);
    }
    it->second.erase(charcode);
    return nullptr;
  }

  return glyph;
}

auto Font::addBuffToBytePool() -> void {
  BytePool *pool = (BytePool *)allocate(BYTE_POOL_SIZE);
  if (pool == nullptr) {
    LOG_E("Unable to allocated memory for bytes pool.");
    return;
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

auto Font::clearCache() -> void {
  // std::scoped_lock guard(mutex);

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

auto Font::getGlyph(char32_t charcode, int16_t glyphSize) -> Glyph * {

  return ready ? getOrCreateGlyph(charcode, glyphSize) : nullptr;
}

auto Font::getGlyph(char32_t charcode, char32_t nextCharcode, int16_t glyphSize)
    -> std::tuple<Glyph *, int16_t, bool> {

  bool ignoreNext = false;
  int16_t kern    = 0;

  char32_t resultCharcode = charcode;

  if (nextCharcode != 0) {

    for (int i = 0; i < ligaturesSize; ++i) {
      const Ligature &lig = ligatures[i];

      // ligatures are sorted by firstChar, so we can stop searching
      if (lig.firstChar > charcode) break;

      if ((lig.firstChar == charcode) && (lig.nextChar == nextCharcode)) {
        resultCharcode = lig.replacement;
        ignoreNext     = true;
        break;
      }
    }
  }

  Glyph *glyph = getOrCreateGlyph(resultCharcode, glyphSize);

  if (glyph == nullptr) {
    ignoreNext = false;
    glyph      = getOrCreateGlyph(charcode, glyphSize);
  }

  if ((glyph != nullptr) && (nextCharcode != 0)) {
    kern = getKern(*glyph, nextCharcode);
  }

  return std::make_tuple(glyph, kern, ignoreNext);
}

// Only used for ASCII chars, so no need to be protected by
// mutex as it is expected to be called only during form
// fields dimensions computation, which is done before
// showing the form and thus before any user interaction.
auto Font::getASCIISize(const char *str, Dim *dim, int16_t glyphSize) -> void {
  {
    // std::scoped_lock guard(mutex);

    int16_t maxUp   = 0;
    int16_t maxDown = 0;

    dim->width = 0;

    while (*str) {
      Glyph *glyph = getOrCreateGlyph(*str++, glyphSize);
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