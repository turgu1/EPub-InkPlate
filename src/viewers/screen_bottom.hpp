#pragma once

#include "global.hpp"

#include "models/font.hpp"
#include "viewers/page.hpp"

class ScreenBottom {
public:
  static constexpr int16_t FONT      = 1;
  static constexpr int16_t FONT_SIZE = 9;

  static auto show(PagePtr &page, int16_t pageNbr = -1, int16_t pageCount = -1) -> void;

private:
  static constexpr char const *TAG = "ScreenBottom";
  static const std::string dw[7];
};