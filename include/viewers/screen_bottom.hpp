#pragma once

#include "global.hpp"
#include "models/font.hpp"
#include "viewers/page.hpp"

class ScreenBottom
{
  public:
    static constexpr int16_t FONT      = 1;
    static constexpr int16_t FONT_SIZE = 9;

    static void show(int16_t page_nbr = -1, int16_t page_count = -1);

  private:
    static const std::string dw[7];
};