// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "picture.hpp"

using PngPicturePtr = himem_unique_ptr<class PngPicture>;

class PngPicture : public Picture {

private:
  static constexpr char const *TAG = "PngPicture";
  const uint16_t WORK_SIZE         = 10 * 1024;
  int8_t scale;

  PngPicture(std::string filename, Dim max, bool load_bitmap);

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make(std::string filename, Dim max, bool load_bitmap) {
    return make_unique_himem<PngPicture>(filename, max, load_bitmap);
  }

  ~PngPicture() override = default;

  inline int8_t get_scale_factor() { return scale; }

  struct PictureData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } picture_data;

  inline const PictureData *get_picture_data() {
    picture_data = {.dim = dim, .bitmap = bitmap.get()};
    return &picture_data;
  }
};