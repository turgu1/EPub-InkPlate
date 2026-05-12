// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"

#include "font.hpp"
#include "himem.hpp"
#include "picture.hpp"
#include "svg_decoder.hpp"

using SvgPicturePtr = HimemUniquePtr<class SvgPicture>;

class SvgPicture : public Picture {

private:
  static constexpr char const *TAG = "SvgPicture";

  FontPtr &font;

  SvgPicture(const HimemString &filename, Dim max, bool loadBitmap, FontPtr &font,
             bool fromFile = false);

  auto loadFromZip(const HimemString &filename, Dim max, bool loadBitmap) -> void;
  auto loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void;
  auto decodeFromBuffer(const uint8_t *data, uint32_t dataSize, Dim max, bool loadBitmap) -> void;

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(const HimemString &filename, Dim max, bool loadBitmap, FontPtr &font,
                          bool fromFile = false) {
    return makeUniqueHimem<SvgPicture>(filename, max, loadBitmap, font, fromFile);
  }

  ~SvgPicture() override = default;

  struct PictureData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } pictureData;
};
