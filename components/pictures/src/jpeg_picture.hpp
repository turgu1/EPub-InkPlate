// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "picture.hpp"

using JPegPicturePtr = himem_unique_ptr<class JPegPicture>;

class JPegPicture : public Picture {

private:
  static constexpr char const *TAG  = "JPegPicture";
  static constexpr size_t WORK_SIZE = 20 * 1024;

  JPegPicture(std::string filename, Dim max, bool load_bitmap, bool from_file = false);

  void LoadFromFile(std::string filename, Dim max, bool load_bitmap);

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make(std::string filename, Dim max, bool load_bitmap, bool from_file = false) {
    return make_unique_himem<JPegPicture>(filename, max, load_bitmap, from_file);
  }

  ~JPegPicture() override = default;

  struct PictureData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } picture_data;
};