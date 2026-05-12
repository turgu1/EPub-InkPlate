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
#include <unordered_map>
// #include <mutex>

using FontPtr = HimemUniquePtr<class Font>;

class Font {

private:
  static constexpr char const *TAG = "Font";

  // std::mutex mutex;

public:
  Font();
  virtual ~Font() {}

  [[nodiscard]] inline auto isReady() const -> bool { return ready; }

  /**
   * @brief Get a glyph object
   *
   * Retrieve a glyph object and convert it to it's bitmap représentation.
   * The cache will be updated to contain the new glyph, if it's not
   * already there.
   *
   * @param charcode Character code as a unicode number.
   * @return Glyph The glyph associated to the unicode character.
   */
  // #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
  //   virtual Glyph * getGlyph(char32_t charcode, int16_t glyph_size, bool debugging = false);
  // #else
  virtual auto getGlyph(char32_t charcode, int16_t glyphSize) -> Glyph *;
  // #endif

  /**
   * @brief Get a glyph using an explicit pixel height.
   *
   * Default implementation falls back to getGlyph(). Font backends that
   * support pixel sizing (like FreeType) should override this method.
   */
  virtual auto getGlyphPx(char32_t charcode, int16_t pixelHeight) -> Glyph *;

  virtual auto getGlyph(char32_t charcode, char32_t nextCharcode, int16_t glyphSize)
      -> std::tuple<Glyph *, int16_t, bool>;

  virtual auto getGlyphPx(char32_t charcode, char32_t nextCharcode, int16_t pixelHeight)
      -> std::tuple<Glyph *, int16_t, bool>;

  auto clearCache() -> void;

  auto getSize(const char *str, Dim *dim, int16_t glyphSize) -> void;

  enum class GlyphLoadMode : uint8_t {
    METRICS_ONLY,   ///< Only metric fields; buffer == nullptr
        WITH_BITMAP ///< Full glyph including rasterized bitmap (default)
  };

  auto setGlyphLoadMode(GlyphLoadMode mode) -> void { glyphLoadMode_ = mode; }
  [[nodiscard]] auto getGlyphLoadMode() const -> GlyphLoadMode { return glyphLoadMode_; }
  auto setPreferAntialiasing(bool enabled) -> void {
    if (preferAntialiasing_ == enabled) return;
    preferAntialiasing_ = enabled;
    clearCache();
  }
  [[nodiscard]] auto isAntialiasingPreferred() const -> bool { return preferAntialiasing_; }

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
    const Glyph *g = getGlyphInternal('E', glyphSize);
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
  using BytePool    = uint8_t[BYTE_POOL_SIZE];
  using BytePools   = std::forward_list<BytePool *>;

  GlyphsCache cache;
  CharPoolPtr glyphsMapPool{nullptr};
  int16_t fontsCacheIndex;
  int8_t currentFontSize;
  bool ready;
  GlyphLoadMode glyphLoadMode_{GlyphLoadMode::WITH_BITMAP};
  bool preferAntialiasing_{false};

  HimemPool<Glyph> bitmapGlyphPool{100};

  BytePools bytePools;
  uint16_t bytePoolIdx;

  auto addBuffToBytePool() -> void;
  auto getOrCreateGlyphs(int16_t glyphSize) -> Glyphs &;

  // /**
  //  * @brief Set the font face object
  //  *
  //  * Get a font file loaded and ready to supply glyphs.
  //  *
  //  * @param fontFilename The filename of the font.
  //  * @return true The font was found and retrieved
  //  * @return false Some error (file not found, unsupported format)
  //  */
  // auto setFontFaceFromFile(const std::string fontFilename) -> bool;

  /**
   * @brief Set the font size
   *
   * Set the font size. This will be used to set various general metrics
   * required from the font structure.
   *
   * @param size The size of the glyphs in points (1/72th of an inch).
   * @return true The font was resized.
   * @return false Not able to resize the font.
   */
  auto setFontSize(int16_t size) -> bool;

  // /**
  //  * @brief Set the font face object
  //  *
  //  * Get a font from memory loaded and ready to supply glyphs. Note
  //  * that the buffer will be freed when the face will be removed.
  //  *
  //  * @param buffer The buffer containing the font.
  //  * @param size   The buffer size in bytes.
  //  * @return true The font was found and retrieved.
  //  * @return false Some error (file not found, unsupported format).
  //  */
  // virtual auto setFontFace(const FontFaceDescriptorPtr *descr) -> bool            = 0;
  virtual auto getGlyphInternal(char32_t charcode, int16_t glyphSize) -> Glyph * = 0;
  virtual auto getKern(Glyph &glyph, char32_t nextCharcode) -> int16_t           = 0;
};
