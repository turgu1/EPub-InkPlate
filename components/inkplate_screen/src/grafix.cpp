#include "grafix.hpp"


/**
 * Draws a solid horizontal row using single pixel iterations.
 */
auto Grafix::drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void {
  for (uint16_t x = x0; x <= x1; ++x) {
    screen.setPixel({ x, y }, color);
  }
}

/**
 * Draws a rounded rectangle into any arbitrary bitmap format via functional abstraction.
 * 
 * @param dim        The rectangle dimention
 * @param p0         Top-left position coordinates
 * @param radius     Corner rounding radius
 * @param thickness  Line thickness growing inward (-1 for solid fill)
 * @param color      Color payload value (0/1 for 1-bpp, 0-15 for 4-bpp)
 */
auto Grafix::drawRoundedRect(Dim dim, Pos p0, uint8_t radius, uint8_t thickness, uint8_t color) -> void {
  Pos p1 = { p0.x + dim.width, p0.y + dim.heigh };

  // Validate and clamp radius
  if (2 * radius > (p1.x - p0.x)) radius = (p1.x - p0.x) / 2;
  if (2 * radius > (p1.y - p0.y)) radius = (p1.y - p0.y) / 2;
  if (r < 0) radius = 0;

  // Detect if drawing in solid fill mode
  bool fill = (thickness == -1 || (thickness * 2) >= (p1.x - p0.x + 1) || (thickness * 2) >= (p1.y - p0.y + 1));

  if (fill) {
    // --- SOLID FILL MODE ---
    uint16_t cx_left  = p0.x + radius;
    uint16_t cx_right = p1.x - radius;
    uint16_t cy_top   = p0.y + radius;
    uint16_t cx       = 0;
    uint16_t cy       = radius;
    uint16_t d        = 3 - 2 * radius;
    uint16_t last_cy  = -1;

    // 1. Fill top rounded canopy
    while (cx <= cy) {
      if (cy != last_cy) {
        drawHLine(cx_left - cx, cx_right + cx, cy_top - cy, color);
        if (cy != 0) drawHLine(cx_left - cy, cx_right + cy, cy_top - cx, color);
        last_cy = cy;
      }
      if (d < 0) {
        d += 4 * cx + 6;
      } else {
        d += 4 * (cx - cy) + 10;
        cy--;
        if (cy >= cx) drawHLine(cx_left - cx, cx_right + cx, cy_top - cy, color);
      }
      cx++;
    }

    // 2. Fill massive center body square
    for (uint16_t y = p0.y + radius; y <= p1.y - radius; y++) {
      drawHLine(p0.x, p1.x, y, color);
    }

    // 3. Fill bottom rounded floor
    uint16_t cy_bot = p1.y - radius;
    cx = 0; cy = radius; d = 3 - 2 * radius; last_cy = -1;
    while (cx <= cy) {
      if (cy != last_cy) {
        drawHLine(cx_left - cx, cx_right + cx, cy_bot + cy, color);
        if (cy != 0) drawHLine(cx_left - cy, cx_right + cy, cy_bot + cx, color);
        last_cy = cy;
      }
      if (d < 0) {
        d += 4 * cx + 6;
      } else {
        d += 4 * (cx - cy) + 10;
        cy--;
        if (cy >= cx) drawHLine(cx_left - cx, cx_right + cx, cy_bot + cy, color);
      }
      cx++;
    }
  } else {

    // --- THICK OUTLINE MODE ---
    for (uint8_t i = 0; i < thickness; i++) {
      uint16_t cur_x0     = p0.x + i;
      uint16_t cur_y0     = p0.y + i;
      uint16_t cur_x1     = p1.x - i;
      uint16_t cur_y1     = p1.y - i;
      uint16_t cur_radius = radius - i; 

      if (cur_radius < 0) cur_radius = 0;

      // Draw horizontal borders
      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y0, color);
      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y1, color);

      // Draw vertical borders
      for (uint16_t y = cur_y0 + cur_radius; y <= cur_y1 - cur_radius; y++) {
        screen.setPixel({ cur_x0, y }, color);
        screen.setPixel({ cur_x1, y }, color);
      }

      // Draw corner arc accents
      if (cur_radius > 0) {
        uint16_t cx_left  = cur_x0 + cur_radius;
        uint16_t cx_right = cur_x1 - cur_radius;
        uint16_t cy_top   = cur_y0 + cur_radius;
        uint16_t cy_bot   = cur_y1 - cur_radius;
        uint16_t cx       = 0;
        uint16_t cy       = cur_radius;
        uint16_t d        = 3 - 2 * cur_radius;

        while (cx <= cy) {
          screen.setPixel({ cx_left  - cy, cy_top - cx }, color);
          screen.setPixel({ cx_left  - cx, cy_top - cy }, color);
          screen.setPixel({ cx_right + cx, cy_top - cy }, color);
          screen.setPixel({ cx_right + cy, cy_top - cx }, color);
          screen.setPixel({ cx_left  - cy, cy_bot + cx }, color);
          screen.setPixel({ cx_left  - cx, cy_bot + cy }, color);
          screen.setPixel({ cx_right + cx, cy_bot + cy }, color);
          screen.setPixel({ cx_right + cy, cy_bot + cx }, color);

          if (d < 0) { d += 4 * cx + 6; } 
          else { d += 4 * (cx - cy) + 10; cy--; }
          cx++;
        }
      }
    }
  }
}
