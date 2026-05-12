#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "pugixml.hpp"

#include <memory>
#include <string>
#include <vector>

using FontFaceDescriptorPtr = HimemUniquePtr<class FontFaceDescriptor>;
class FontFaceDescriptor {
private:
  FontFaceDescriptor() = default;

public:
  ~FontFaceDescriptor() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make() -> FontFaceDescriptorPtr {
    return makeUniqueHimem<FontFaceDescriptor>();
  }

  HimemString name;
  FaceStyle style;
  HimemString filename;
  FileContentPtr fontData{nullptr};
  size_t fontDataSize{0};
};

class FontsDB {

public:
private:
  static constexpr char const *TAG = "FontsDB";

  HimemVector<FontFaceDescriptorPtr> fontFaceDescriptors;

  int32_t fontSize{0};
  uint8_t standardFontCount{0};

  HimemString fontNames[8];
  HimemString regularFname[8];
  HimemString boldFname[8];
  HimemString italicFname[8];
  HimemString boldItalicFname[8];

  auto checkFile(const HimemString &filename) -> bool;
  auto add(const HimemString &name, FaceStyle style, const HimemString &filename) -> bool;
  auto replace(int16_t index, const HimemString &name, FaceStyle style, const HimemString &filename)
      -> bool;

  [[nodiscard]] inline auto checkRes(pugi::xml_parse_result res) -> bool {
    return res.status == pugi::status_ok;
  }

public:
  FontsDB()  = default;
  ~FontsDB() = default;

  auto load(uint8_t configFontIndex) -> bool;
  auto adjustDefaultFont(uint8_t fontIndex) -> void;
  auto getFile(const char *filename, size_t size) -> HimemUniquePtr<char[]>;

  [[nodiscard]] inline auto getStandardFontCount() const -> uint8_t { return standardFontCount; }
  [[nodiscard]] inline auto getFontFaceCount() const -> uint8_t {
    return fontFaceDescriptors.size();
  }

  [[nodiscard]] inline auto isDefaultFont(uint8_t fontIndex) const -> bool {
    if (fontFaceDescriptors.size() < 3) {
      return false;
    } else {
      return fontFaceDescriptors.at(3)->name.compare(fontNames[fontIndex]) == 0;
    }
  }

  auto inline getStandardFontName(uint8_t index) const -> const HimemString & {
    if (index >= standardFontCount) {
      LOG_E("getStandardFontName(): Wrong index: %d vs size: %u", index, standardFontCount);
      return fontNames[0]; // Return a default font name instead of nullptr
    } else {
      return fontNames[index];
    }
  }

  auto getFontFaceDescriptor(uint8_t index) const -> const FontFaceDescriptorPtr * {
    if (index >= fontFaceDescriptors.size()) {
      LOG_E("getFontFaceDescriptor(): Wrong index: %d vs size: %u", index,
            fontFaceDescriptors.size());
      return nullptr;
    } else {
      return &fontFaceDescriptors.at(index);
    }
  }
};