#pragma once

#include "global.hpp"

#include <cstdint>
#include <functional>

using SetPixel = std::function<void(Pos, uint8_t)>;

class Grafix {
  private:
    static auto drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void;
    
  public:
    Grafix(std::function<void(Pos, uint8_t)> & setPixel) : setPixel(setPixel) {}

    static auto drawRoundedRect(Dim dim, Pos p0, uint8_t radius, 
                                int8_t thickness = 1, uint8_t color = 1) -> void;
}