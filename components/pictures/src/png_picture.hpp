// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "picture.hpp"

using PngPicturePtr = HimemUniquePtr<class PngPicture>;

class PngPicture : public Picture {

private:
  static constexpr char const *TAG = "PngPicture";
  const uint16_t WORK_SIZE         = 10 * 1024;
  int8_t scale;

  PngPicture(std::string filename, Dim max, bool loadBitmap);

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(std::string filename, Dim max, bool loadBitmap) {
    return makeUniqueHimem<PngPicture>(filename, max, loadBitmap);
  }

  ~PngPicture() override = default;

 [[nodiscard]] inline auto getScaleFactor() -> int8_t { return scale; }

  struct PictureData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } pictureData;

 [[nodiscard]] inline auto getPictureData() -> const PictureData * {
    pictureData = {.dim = dim, .bitmap = bitmap.get()};
    return &pictureData;
  }
};