// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "memory_pool.hpp"

#include <forward_list>
#include <mutex>
#include <unordered_map>

using MemoryFontPtr = himemUniquePtr<uint8_t[]>;

class Font {

private:
  static constexpr char const *TAG = "Font";

  std::mutex mutex;

public:
  Font();
  virtual ~Font() {};

  inline auto isReady() const -> bool { return ready; }

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
  //   virtual Glyph * getGlyph(uint32_t charcode, int16_t glyph_size, bool debugging = false);
  // #else
  virtual auto getGlyph(uint32_t charcode, int16_t glyphSize) -> Glyph *;
  // #endif

  virtual auto getGlyph(uint32_t charcode, uint32_t nextCharcode, int16_t glyphSize,
                          int16_t &kern, bool &ignoreNext) -> Glyph *;

  void clearCache();

  void getSize(const char *str, Dim *dim, int16_t glyphSize);

  inline void setFontsCacheIndex(int16_t index) { fontsCacheIndex = index; }
  inline auto getFontsCacheIndex() -> int16_t { return fontsCacheIndex; }
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

  using Glyphs      = std::unordered_map<uint32_t, Glyph *>; ///< Cache for the glyphs' bitmap
  using GlyphsCache = std::unordered_map<int16_t, Glyphs>;
  using BytePool    = uint8_t[BYTE_POOL_SIZE];
  using BytePools   = std::forward_list<BytePool *>;

  GlyphsCache cache;
  int16_t fontsCacheIndex;
  int8_t currentFontSize;
  bool ready;

  MemoryPool<Glyph> bitmapGlyphPool;

  BytePools bytePools;
  uint16_t bytePoolIdx;

  void addBuffToBytePool();

  MemoryFontPtr memoryFont; ///< Buffer for memory fonts

  /**
   * @brief Set the font face object
   *
   * Get a font file loaded and ready to supply glyphs.
   *
   * @param fontFilename The filename of the font.
   * @return true The font was found and retrieved
   * @return false Some error (file not found, unsupported format)
   */
  auto setFontFaceFromFile(const std::string fontFilename) -> bool;

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

  /**
   * @brief Set the font face object
   *
   * Get a font from memory loaded and ready to supply glyphs. Note
   * that the buffer will be freed when the face will be removed.
   *
   * @param buffer The buffer containing the font.
   * @param size   The buffer size in bytes.
   * @return true The font was found and retrieved.
   * @return false Some error (file not found, unsupported format).
   */
  virtual auto setFontFaceFromMemory(MemoryFontPtr buffer, int32_t size) -> bool = 0;
  virtual auto getGlyphInternal(uint32_t charcode, int16_t glyphSize) -> Glyph *  = 0;
  virtual auto adjustLigatureAndKern(Glyph *glyph, uint16_t glyphSize, uint32_t nextCharcode,
                                       int16_t &kern, bool &ignoreNext) -> Glyph *  = 0;
};
