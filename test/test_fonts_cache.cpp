// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "test_stats.hpp"
#include "ttf2.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace {

int gPass = 0;
int gFail = 0;

#define FC_CHECK(cond, msg)                                                                        \
  do {                                                                                             \
    if (cond) {                                                                                    \
      std::printf("  PASS %s\n", msg);                                                             \
      ++gPass;                                                                                     \
    } else {                                                                                       \
      std::printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg);                                 \
      ++gFail;                                                                                     \
    }                                                                                              \
  } while (0)

static auto makeDescriptor(const char *name, const char *filePath) -> FontFaceDescriptorPtr {
  auto descr = FontFaceDescriptor::Make();
  if (!descr) return nullptr;

  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) return nullptr;

  const auto size = static_cast<std::size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  auto buffer = makeUniqueHimem<uint8_t[]>(size + 1);
  if (!buffer) return nullptr;

  file.read(reinterpret_cast<char *>(buffer.get()), static_cast<std::streamsize>(size));
  if (file.gcount() != static_cast<std::streamsize>(size)) return nullptr;

  buffer[size]        = 0;
  descr->name         = name;
  descr->style        = FaceStyle::NORMAL;
  descr->filename     = filePath;
  descr->fontData     = std::move(buffer);
  descr->fontDataSize = size;
  return descr;
}

static auto testTtfCachePoolLifecycle() -> void {
  std::printf("  [1. TTF cache pool lifecycle]\n");

  FT_Library library = nullptr;
  FC_CHECK(FT_Init_FreeType(&library) == 0, "FreeType library init succeeds");
  if (!library) return;

  auto descr = makeDescriptor("RobotoCondensed-Regular",
                              "test/fixtures/config_data/fonts/RobotoCondensed-Regular.otf");
  FC_CHECK(descr != nullptr, "TTF fixture descriptor created");
  if (!descr) {
    FT_Done_FreeType(library);
    return;
  }

  FontPtr font = TTF::Make(descr, library);
  FC_CHECK(font != nullptr && font->isReady(), "TTF font is ready");
  if (!font || !font->isReady()) {
    FT_Done_FreeType(library);
    return;
  }

  FC_CHECK(font->getGlyphCacheLevelCount() == 0, "TTF cache starts empty");
  FC_CHECK(font->getGlyphCacheEntryCount() == 0, "TTF cache has no glyph entries initially");

  Glyph *gA1 = font->getGlyph('A', 18);
  Glyph *gB1 = font->getGlyph('B', 18);
  Glyph *gA2 = font->getGlyph('A', 18);

  FC_CHECK(gA1 != nullptr && gB1 != nullptr, "TTF glyphs A and B are loaded");
  FC_CHECK(gA1 == gA2, "TTF repeated lookup reuses cached glyph pointer");
  FC_CHECK(font->getGlyphCacheLevelCount() == 1, "TTF cache has one font-size bucket");
  FC_CHECK(font->getGlyphCacheEntryCount() >= 2, "TTF cache stores at least two glyphs");

  const std::size_t allocatedBeforeClear = font->getGlyphMapPoolAllocatedBytes();
  const std::size_t freedBeforeClear     = font->getGlyphMapPoolFreedBytes();
  FC_CHECK(allocatedBeforeClear > 0, "TTF glyph map pool allocated bytes increased");

  font->clearCache();
  FC_CHECK(font->getGlyphCacheLevelCount() == 0, "TTF cache buckets cleared");
  FC_CHECK(font->getGlyphCacheEntryCount() == 0, "TTF cache entries cleared");
  FC_CHECK(font->getGlyphMapPoolFreedBytes() > freedBeforeClear,
           "TTF glyph map pool freed bytes increased after clear");

  FT_Done_FreeType(library);
}

static auto testTtfCacheChurnMonotonic() -> void {
  std::printf("  [3. TTF cache churn monotonic deltas]\n");

  FT_Library library = nullptr;
  FC_CHECK(FT_Init_FreeType(&library) == 0, "FreeType init succeeds for churn test");
  if (!library) return;

  auto descr = makeDescriptor("RobotoCondensed-Regular",
                              "test/fixtures/config_data/fonts/RobotoCondensed-Regular.otf");
  FC_CHECK(descr != nullptr, "TTF churn descriptor created");
  if (!descr) {
    FT_Done_FreeType(library);
    return;
  }

  FontPtr font = TTF::Make(descr, library);
  FC_CHECK(font != nullptr && font->isReady(), "TTF font ready for churn test");
  if (!font || !font->isReady()) {
    FT_Done_FreeType(library);
    return;
  }

  std::size_t prevAllocated = font->getGlyphMapPoolAllocatedBytes();
  std::size_t prevFreed     = font->getGlyphMapPoolFreedBytes();

  for (int cycle = 0; cycle < 3; ++cycle) {
    Glyph *g1 = font->getGlyph('A' + cycle, 18);
    Glyph *g2 = font->getGlyph('B' + cycle, 18);
    FC_CHECK(g1 != nullptr && g2 != nullptr, "TTF churn cycle loads glyphs");

    const std::size_t allocAfterFill = font->getGlyphMapPoolAllocatedBytes();
    FC_CHECK(allocAfterFill >= prevAllocated, "TTF allocated bytes are monotonic");

    font->clearCache();
    FC_CHECK(font->getGlyphCacheEntryCount() == 0, "TTF churn clear empties cache");

    const std::size_t freedAfterClear = font->getGlyphMapPoolFreedBytes();
    FC_CHECK(freedAfterClear > prevFreed, "TTF freed bytes increase after each clear");

    prevAllocated = allocAfterFill;
    prevFreed     = freedAfterClear;
  }

  FT_Done_FreeType(library);
}

static auto runTtfChurnStress(int cycles) -> void {
  std::printf("  [stress] TTF churn (%d cycles)\n", cycles);

  FT_Library library = nullptr;
  FC_CHECK(FT_Init_FreeType(&library) == 0, "FreeType init succeeds for TTF stress");
  if (!library) return;

  auto descr = makeDescriptor("RobotoCondensed-Regular",
                              "test/fixtures/config_data/fonts/RobotoCondensed-Regular.otf");
  FC_CHECK(descr != nullptr, "TTF stress descriptor created");
  if (!descr) {
    FT_Done_FreeType(library);
    return;
  }

  FontPtr font = TTF::Make(descr, library);
  FC_CHECK(font != nullptr && font->isReady(), "TTF font ready for stress");
  if (!font || !font->isReady()) {
    FT_Done_FreeType(library);
    return;
  }

  bool loadOk      = true;
  bool allocMono   = true;
  bool clearOk     = true;
  bool freedGrowth = true;

  std::size_t prevAllocated = font->getGlyphMapPoolAllocatedBytes();
  std::size_t prevFreed     = font->getGlyphMapPoolFreedBytes();

  for (int i = 0; i < cycles; ++i) {
    Glyph *g1 = font->getGlyph('A' + (i % 20), 18);
    Glyph *g2 = font->getGlyph('B' + (i % 20), 18);
    if (!g1 || !g2) loadOk = false;

    const std::size_t allocAfterFill = font->getGlyphMapPoolAllocatedBytes();
    if (allocAfterFill < prevAllocated) allocMono = false;

    font->clearCache();
    if (font->getGlyphCacheEntryCount() != 0) clearOk = false;

    const std::size_t freedAfterClear = font->getGlyphMapPoolFreedBytes();
    if (freedAfterClear <= prevFreed) freedGrowth = false;

    prevAllocated = allocAfterFill;
    prevFreed     = freedAfterClear;
  }

  FC_CHECK(loadOk, "TTF stress: glyph loads succeed across all cycles");
  FC_CHECK(allocMono, "TTF stress: allocated bytes remain monotonic");
  FC_CHECK(clearOk, "TTF stress: cache is empty after each clear");
  FC_CHECK(freedGrowth, "TTF stress: freed bytes increase after each clear");

  FT_Done_FreeType(library);
}

} // namespace

auto testFontsCache() -> TestStats {
  gPass = 0;
  gFail = 0;

  std::printf("\n=== Fonts Cache Tests ===\n");

  testTtfCachePoolLifecycle();
  testTtfCacheChurnMonotonic();

  std::printf("--- Fonts Cache: %d passed, %d failed ---\n", gPass, gFail);
  return TestStats{gPass, gFail};
}

auto testFontsCacheStress() -> TestStats {
  gPass = 0;
  gFail = 0;

  std::printf("\n=== Fonts Cache Stress Tests ===\n");

  runTtfChurnStress(100);

  std::printf("--- Fonts Cache Stress: %d passed, %d failed ---\n", gPass, gFail);
  return TestStats{gPass, gFail};
}
