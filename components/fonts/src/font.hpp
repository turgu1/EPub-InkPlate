// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "char_pool.hpp"
#include "fonts_db.hpp"
#include "himem.hpp"
#include "himem_pool.hpp"

#include <forward_list>
// #include <mutex>

using FontPtr = HimemUniquePtr<class Font>;

class Font {
public:
  Font();
  virtual ~Font() { clearCache(); }

  [[nodiscard]] inline auto isReady() const -> bool { return ready; }

  enum class SizeUnit : uint8_t {
    POINTS, ///< Size in points (1/72th of an inch)
    PIXELS, ///< Size in pixels
  };

private:
  static constexpr char const *TAG = "Font";

  static constexpr auto toCacheKey(int16_t size, SizeUnit unit) -> int16_t {
    return (unit == SizeUnit::PIXELS) ? static_cast<int16_t>(-size) : size;
  }

public:
  /**
   * @brief Get a glyph object
   *
   * Retrieve a glyph object and convert it to it's bitmap représentation.
   * The cache will be updated to contain the new glyph, if it's not
   * already there.
   *
   * @param charcode Character code as a unicode number.
   * @param nextCharcode Next character code (for ligatures and kerning), or 0 if none.
   * @param glyphSize Size of the glyph.
   * @param unit Unit of the glyph size (points or pixels).
   * @return Glyph The glyph associated to the unicode character,
   *         boolean indicating if nextCharCode is to be ignored (ligature conversion).
   */

  virtual auto getGlyph(char32_t charcode, char32_t nextCharcode, int16_t glyphSize)
      -> std::tuple<Glyph *, int16_t, bool>;

  virtual auto getGlyph(char32_t charcode, int16_t glyphSize) -> Glyph *;

  auto clearCache() -> void;

  auto getASCIISize(const char *str, Dim *dim, int16_t glyphSize) -> void;

  enum class GlyphLoadMode : uint8_t {
    METRICS_ONLY, ///< Only metric fields; buffer == nullptr
    WITH_BITMAP   ///< Full glyph including rasterized bitmap (default)
  };

  auto setGlyphLoadMode(GlyphLoadMode mode) -> void { glyphLoadMode = mode; }
  [[nodiscard]] auto getGlyphLoadMode() const -> GlyphLoadMode { return glyphLoadMode; }

  auto setPreferAntialiasing(bool enabled) -> void {
    if (preferAntialiasing == enabled) return;
    preferAntialiasing = enabled;
    clearCache();
  }
  [[nodiscard]] auto isAntialiasingPreferred() const -> bool { return preferAntialiasing; }

  [[nodiscard]] auto getGlyphCacheLevelCount() const -> std::size_t { return cache.size(); }

  [[nodiscard]] auto getGlyphCacheEntryCount() const -> std::size_t {
    std::size_t count = 0;
    for (const auto &bucket : cache) count += bucket.second.size();
    return count;
  }

  [[nodiscard]] auto getGlyphMapPoolAllocatedBytes() const -> std::size_t {
    return glyphsMapPool ? glyphsMapPool->getTotalAllocated() : 0;
  }

  [[nodiscard]] auto getGlyphMapPoolFreedBytes() const -> std::size_t {
    return glyphsMapPool ? glyphsMapPool->getTotalFreed() : 0;
  }

  inline auto setFontsCacheIndex(int16_t index) -> void { fontsCacheIndex = index; }
  [[nodiscard]] inline auto getFontsCacheIndex() -> int16_t { return fontsCacheIndex; }

  auto bytePoolAlloc(uint16_t size) -> uint8_t *;

  inline auto setCurrentSizeUnit(SizeUnit unit) -> void { currentSizeUnit = unit; }
  [[nodiscard]] inline auto getCurrentSizeUnit() const -> SizeUnit { return currentSizeUnit; }

  /**
   * @brief Face normal line height
   *
   *
   * @return int32_t Normal line height of the face in pixels related to the glyph size
   */
  virtual auto getLineHeight(int16_t glyphSize) -> int32_t = 0;

  /**
   * @brief Get Face characters height related to the glyph size
   *
   */
  virtual auto getCharsHeight(int16_t glyphSize) -> int32_t {
    const Glyph *g = getOrCreateGlyph('E', glyphSize);
    return (g == nullptr) ? 0 : (g->dim.height - getDescenderHeight(glyphSize));
  };

  /**
   * @brief Face descender height
   *
   * @return int32_t The face descender height in pixels related to the glyph size.
   */
  virtual auto getDescenderHeight(int16_t glyphSize) -> int32_t = 0;

protected:
  static constexpr uint16_t BYTE_POOL_SIZE = 16384 * 2;

  static constexpr int32_t ligaturesSize = 22;
  static constexpr struct Ligature {
    char32_t firstChar;
    char32_t nextChar;
    char32_t replacement;
  } ligatures[ligaturesSize] = {
      {0x0021, 0x2018, 0x00A1}, // !, ‘, ¡
      {0x002C, 0x002C, 0x201E}, // , , „
      {0x002D, 0x002D, 0x2013}, // -, -, –
      {0x002E, 0x002E, 0x2024}, // ., ., ‥
      {0x003C, 0x003C, 0x00AB}, // <, <, «
      {0x003E, 0x003E, 0x00BB}, // >, >, »
      {0x003F, 0x2018, 0x00BF}, // ?, ‘, ¿
      {0x0041, 0x0045, 0x00C6}, // A, E, Æ
      {0x0049, 0x004A, 0x0132}, // I, J, Ĳ
      {0x004F, 0x0045, 0x0152}, // O, E, Œ
      {0x0061, 0x0065, 0x00E6}, // a, e, æ
      {0x0066, 0x0066, 0xFB00}, // f, f, ﬀ
      {0x0066, 0x0069, 0xFB01}, // f, i, ﬁ
      {0x0066, 0x006C, 0xFB02}, // f, l, ﬂ
      {0x0069, 0x006A, 0x0133}, // i, j, ĳ
      {0x006F, 0x0065, 0x0153}, // o, e, œ
      {0x2013, 0x002D, 0x2014}, // –, -, —
      {0x2018, 0x2018, 0x201C}, // ‘, ‘, “
      {0x2019, 0x2019, 0x201D}, // ’, ’, ”
      {0x2025, 0x002E, 0x2026}, // ‥, ., …
      {0xFB00, 0x0069, 0xFB03}, // ﬀ ,i, ﬃ
      {0xFB00, 0x006C, 0xFB04}, // ﬀ ,l, ﬄ
  };

  using GlyphsAlloc = CharPoolAllocator<std::pair<const uint32_t, Glyph *>>;
  using Glyphs =
      std::unordered_map<uint32_t, Glyph *, std::hash<uint32_t>, std::equal_to<uint32_t>,
                         GlyphsAlloc>; ///< Cache for glyph pointers, allocated from a pool
  using GlyphsCache = HimemUnorderedMap<int16_t, Glyphs>;

  using BytePool  = uint8_t[BYTE_POOL_SIZE];
  using BytePools = std::forward_list<BytePool *>;

  GlyphsCache cache;
  CharPoolPtr glyphsMapPool{nullptr};
  int16_t fontsCacheIndex;

  int8_t currentFontSize;
  bool ready;
  GlyphLoadMode glyphLoadMode{GlyphLoadMode::WITH_BITMAP};
  bool preferAntialiasing{false};
  SizeUnit currentSizeUnit{SizeUnit::POINTS};

  HimemPool<Glyph> bitmapGlyphPool{100};

  BytePools bytePools;
  uint16_t bytePoolIdx;

  auto addBuffToBytePool() -> void;
  auto getOrCreateGlyph(char32_t charcode, int16_t glyphSize) -> Glyph *;

  virtual auto getGlyphInternal(Glyph &glyph, char32_t charcode, int16_t glyphSize) -> bool = 0;
  virtual auto getKern(Glyph &glyph, char32_t nextCharcode) -> int16_t                      = 0;
};
