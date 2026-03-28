// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "image.hpp"

class JPegImage : public Image {
public:
  JPegImage(std::string filename, Dim max, bool load_bitmap, bool from_file = false);
  ~JPegImage() = default;

  struct ImageData {
    Dim dim{0, 0};
    uint8_t *bitmap{nullptr};
  } image_data;

private:
  static constexpr char const *TAG  = "JPegImage";
  static constexpr size_t WORK_SIZE = 20 * 1024;

  void LoadFromFile(std::string filename, Dim max, bool load_bitmap);
};