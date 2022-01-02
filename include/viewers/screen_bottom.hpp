#pragma once

#include "global.hpp"
#include "models/font.hpp"
#include "viewers/page.hpp"

class ScreenBottom
{
  public:
    static constexpr int16_t FONT      = 5;
    static constexpr int16_t FONT_SIZE = 9;

    static void show(int16_t page_nbr, int16_t page_count);

  private:
    static const char * dw[7];
};