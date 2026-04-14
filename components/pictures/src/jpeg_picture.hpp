// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "picture.hpp"

using JPegPicturePtr = himemUniquePtr<class JPegPicture>;

class JPegPicture : public Picture {

private:
  static constexpr char const *TAG  = "JPegPicture";
  static constexpr size_t WORK_SIZE = 20 * 1024;

  JPegPicture(std::string filename, Dim max, bool loadBitmap, bool fromFile = false);

  void loadFromFile(std::string filename, Dim max, bool loadBitmap);

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(std::string filename, Dim max, bool loadBitmap, bool fromFile = false) {
    return makeUniqueHimem<JPegPicture>(filename, max, loadBitmap, fromFile);
  }

  ~JPegPicture() override = default;

  struct PictureData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } pictureData;
};