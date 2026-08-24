// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// Using GTK on Linux to emulate InkPlate screen
//
// As all GTK related code is located in this module, we also implement
// some part of the event manager code here...

#define __SCREEN__ 1
#include "screen.hpp"

#include <iomanip>

#define BYTES_PER_PIXEL 3

Screen                    Screen::singleton;
std::unique_ptr<guchar[]> Screen::pixels = nullptr;

uint16_t                  Screen::width;
uint16_t                  Screen::height;

const uint8_t             Screen::LUT1BIT[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

auto freePixels(guchar *pixels, gpointer data) -> void {
  if (Screen::pixels != nullptr) {
    Screen::pixels.reset();
  }
}

auto Screen::setPixel(Pos pos, guchar color) -> void {
  guchar *a     = gdk_pixbuf_get_pixels(pb);
  int p = pos.y * pictureData.stride + pos.x * BYTES_PER_PIXEL;
  a[p] = a[p + 1] = a[p + 2] = color;
}

#define NIBBLE(k, p) static_cast<uint8_t>(((k & 1) ? (bitmapData[p] & 0x0F) : (bitmapData[p] >> 4)) << 4)

auto Screen::drawPicture(PicturePtr &picture, Pos pos) -> void {

  pb = gtk_image_get_pixbuf(pictureData.picture);

  auto       dim        = picture->getDim();
  auto       bitmapData = picture->getBitmap();
  auto       bpp        = picture->getBitsPerPixel();

  if (pos.x > width) { pos.x = 0; }
  if (pos.y > height) { pos.y = 0; }

  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  // LOG_I("Picture position: [{}, {}], dimensions: [{}, {}], bpp: {}", pos.x, pos.y, dim.width, dim.height, bpp);

  if (pixelResolution == PixelResolution::MONO) {
    static int16_t err[601];
    int16_t        error;
    memset(err, 0, 601 * 2);

    if (bpp == 4) {
      uint16_t w = (dim.width + 1) >> 1;
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        // LOG_I("Nibbles of line {}: {:#04x} <--> {:#04x} {:#04x}", q, NIBBLE(0, (q*w)), NIBBLE(0, ((q+1)*w)-1), NIBBLE(1, ((q+1)*w)-1));
        uint32_t p = q * w;
        for (uint16_t i = pos.x, k = 0; i < (xMax - 1); ++i, ++k) {
          int32_t v = NIBBLE(k, p) + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            setPixel({i, j}, 255);
          } else {
            error = v;
            setPixel({i, j}, 0);
          }
          if (k != 0) {
            err[k - 1] += error / 8;
          }
          err[k] += 3 * error / 8;
          err[k + 1] = error / 8;
          err[k + 2] += 3 * error / 8;

          if (k & 1) { ++p; }
        }
      }
    } else {
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * dim.width;
        for (uint16_t i = pos.x, k = 0; i < (xMax - 1); ++i, ++p, ++k) {
          int32_t v = bitmapData[p] + err[k + 1];
          if (v > 128) {
            error = (v - 255);
            setPixel({i, j}, 255);
          } else {
            error = v;
            setPixel({i, j}, 0);
          }
          if (k != 0) {
            err[k - 1] += error / 8;
          }
          err[k] += 3 * error / 8;
          err[k + 1] = error / 8;
          err[k + 2] += 3 * error / 8;
        }
      }
    }
  }
  else {
    if (bpp == 4) {
      uint16_t w = (dim.width + 1) >> 1;
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * w;
        for (uint16_t i = pos.x, k = 0; i < xMax; ++i, ++k) {
          setPixel({i, j}, NIBBLE(k, p));
          if (k & 1) { ++p; }
        }
      }
    } else {
      for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
        uint32_t p = q * dim.width;
        for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
          setPixel({i, j}, bitmapData[p]);
        }
      }
    }
  }
}

#undef NIBBLE

auto Screen::drawRectangle(Dim dim, Pos pos, Color color, int8_t thickness) -> void {

  pb = gtk_image_get_pixbuf(pictureData.picture);

  uint16_t    xMax = pos.x + dim.width;
  uint16_t    yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  for (uint16_t i = pos.x; i < xMax; ++i) {
    setPixel({i, pos.y}, color);
    setPixel({i, uint16_t(yMax - 1)}, color);
  }
  for (uint16_t j = pos.y; j < yMax; ++j) {
    setPixel({pos.x, j}, color);
    setPixel({uint16_t(xMax - 1), j}, color);
  }
}

/**
 * Draws a solid horizontal row using single pixel iterations.
 */
auto Screen::drawHLine(uint16_t x0, uint16_t x1, uint16_t y, uint8_t color) -> void {
  for (uint16_t x = x0; x <= x1; ++x) {
    setPixel({ x, y }, color);
  }
}

/**
 * Draws a rounded rectangle into any arbitrary bitmap format via functional abstraction.
 * 
 * @param dim        The rectangle dimension
 * @param p0         Top-left position coordinates
 * @param color      Color payload value (0/1 for 1-bpp, 0-15 for 4-bpp)
 * @param radius     Corner rounding radius
 * @param thickness  Line thickness inward (-1 solid, -2 for 1-in-2 grid, -3 for 1-in-3 grid)
 */
auto Screen::drawRoundedRectangle(Dim dim, Pos p0, uint8_t color, int8_t thickness, int8_t radius) -> void {
  if (dim.width == 0 || dim.height == 0) return;

  // Compute inclusive outer boundaries
  Pos p1 = { uint16_t(p0.x + dim.width - 1), uint16_t(p0.y + dim.height - 1) };

  // if (pixelResolution == PixelResolution::MONO) {
  //   color = (color == Color::BLACK) ? platform.getEInk().getMonoBlack() : platform.getEInk().getMonoWhite();
  // } else {
  //   color = (color == Color::BLACK) ? platform.getEInk().getGrayBlack() : platform.getEInk().getGrayWhite();
  // }

  // Validate and clamp radius
  if (2 * radius > (p1.x - p0.x)) radius = (p1.x - p0.x) / 2;
  if (2 * radius > (p1.y - p0.y)) radius = (p1.y - p0.y) / 2;
  if (radius < 0) radius = 0;

  // Identify modes cleanly
  bool pattern_fill = (thickness < -1);
  bool solid_fill   = (thickness == -1);
  
  if (!solid_fill && !pattern_fill) {
    int32_t req_thickness = thickness * 2;
    int32_t max_width = p1.x - p0.x + 1;
    int32_t max_height = p1.y - p0.y + 1;
    if (req_thickness >= max_width || req_thickness >= max_height) {
      solid_fill = true;
    }
  }

  if (solid_fill || pattern_fill) {
    int8_t spacing = (pattern_fill) ? -thickness : 1;

    // Aligned grid filling logic to completely prevent 45-degree striping
    auto drawInteriorLine = [this, spacing, color](uint16_t x0, uint16_t x1, uint16_t y) {
      if (spacing > 1) {
        // Only allow drawing on rows that match the vertical spacing rule
        if (y % spacing == 0) {
          for (uint16_t x = x0; x <= x1; ++x) {
            // Only illuminate pixels matching the horizontal spacing rule
            if (x % spacing == 0) {
              setPixel({ x, y }, color);
            }
          }
        }
      } else {
        // Mode -1: Solid fill
        drawHLine(x0, x1, y, color);
      }
    };

    uint16_t cx_left  = p0.x + radius;
    uint16_t cx_right = p1.x - radius;
    uint16_t cy_top   = p0.y + radius;
    uint16_t cy_bot   = p1.y - radius;

    // 1. Render the outer structural shell first to guarantee a continuous solid contour line
    drawHLine(cx_left, cx_right, p0.y, color);
    drawHLine(cx_left, cx_right, p1.y, color);
    for (uint16_t y = cy_top; y <= cy_bot; ++y) {
      setPixel({ p0.x, y }, color);
      setPixel({ p1.x, y }, color);
    }
    if (radius > 0) {
      int16_t cx = 0;
      int16_t cy = radius;
      int16_t d  = 3 - 2 * radius;
      while (cx <= cy) {
        setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_top - cx) }, color);
        setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_top - cy) }, color);
        setPixel({ uint16_t(cx_right + cx), uint16_t(cy_top - cy) }, color);
        setPixel({ uint16_t(cx_right + cy), uint16_t(cy_top - cx) }, color);
        setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_bot + cx) }, color);
        setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_bot + cy) }, color);
        setPixel({ uint16_t(cx_right + cx), uint16_t(cy_bot + cy) }, color);
        setPixel({ uint16_t(cx_right + cy), uint16_t(cy_bot + cx) }, color);
        if (d < 0) { d += 4 * cx + 6; } else { d += 4 * (cx - cy) + 10; cy--; }
        cx++;
      }
    }

    // 2. Render interior dither contents strictly inside the border boundaries
    if (radius > 0) {
      int16_t cx = 0;
      int16_t cy = radius;
      int16_t d  = 3 - 2 * radius;
      while (cx <= cy) {
        // Avoid drawing over the outermost perimeter structural pixels
        if (cy_top - cy > p0.y) {
          drawInteriorLine(cx_left - cx + 1, cx_right + cx - 1, cy_top - cy);
          drawInteriorLine(cx_left - cx + 1, cx_right + cx - 1, cy_bot + cy);
        }
        if (cy_top - cx > p0.y) {
          drawInteriorLine(cx_left - cy + 1, cx_right + cy - 1, cy_top - cx);
          drawInteriorLine(cx_left - cy + 1, cx_right + cy - 1, cy_bot + cx);
        }
        if (d < 0) { d += 4 * cx + 6; } else { d += 4 * (cx - cy) + 10; cy--; }
        cx++;
      }
    }

    uint16_t y_start = p0.y + radius + 1;
    uint16_t y_end   = p1.y - radius - 1;
    if (radius == 0) { y_start = p0.y + 1; y_end = p1.y - 1; }

    for (uint16_t y = y_start; y <= y_end; y++) {
      drawInteriorLine(p0.x + 1, p1.x - 1, y);
    }

  } else {
    // --- THICK OUTLINE MODE ---
    for (uint8_t i = 0; i < thickness; i++) {
      if ((p0.x + i) > (p1.x - i) || (p0.y + i) > (p1.y - i)) break;

      uint16_t cur_x0     = p0.x + i;
      uint16_t cur_y0     = p0.y + i;
      uint16_t cur_x1     = p1.x - i;
      uint16_t cur_y1     = p1.y - i;
      int16_t  cur_radius = radius - i; 

      if (cur_radius < 0) cur_radius = 0;

      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y0, color);
      drawHLine(cur_x0 + cur_radius, cur_x1 - cur_radius, cur_y1, color);

      for (uint16_t y = cur_y0 + cur_radius; y <= cur_y1 - cur_radius; ++y) {
        setPixel({ cur_x0, y }, color);
        setPixel({ cur_x1, y }, color);
      }

      if (cur_radius > 0) {
        uint16_t cx_left  = cur_x0 + cur_radius;
        uint16_t cx_right = cur_x1 - cur_radius;
        uint16_t cy_top   = cur_y0 + cur_radius;
        uint16_t cy_bot   = cur_y1 - cur_radius;
        
        int16_t cx = 0;
        int16_t cy = cur_radius;
        int16_t d  = 3 - 2 * cur_radius; 

        while (cx <= cy) {
          setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_top - cx) }, color);
          setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_top - cy) }, color);
          setPixel({ uint16_t(cx_right + cx), uint16_t(cy_top - cy) }, color);
          setPixel({ uint16_t(cx_right + cy), uint16_t(cy_top - cx) }, color);
          setPixel({ uint16_t(cx_left  - cy), uint16_t(cy_bot + cx) }, color);
          setPixel({ uint16_t(cx_left  - cx), uint16_t(cy_bot + cy) }, color);
          setPixel({ uint16_t(cx_right + cx), uint16_t(cy_bot + cy) }, color);
          setPixel({ uint16_t(cx_right + cy), uint16_t(cy_bot + cx) }, color);

          if (d < 0) { d += 4 * cx + 6; } else { d += 4 * (cx - cy) + 10; cy--; }
          ++cx;
        }
      }
    }
  }
}

auto Screen::colorizeRegion(Dim dim, Pos pos, Color color) -> void {
  pb = gtk_image_get_pixbuf(pictureData.picture);

  int16_t    xMax = pos.x + dim.width;
  int16_t    yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  for (uint16_t j = pos.y; j < yMax; ++j) {
    for (uint16_t i = pos.x; i < xMax; ++i) {
      setPixel({i, j}, color);
    }
  }
}

auto Screen::drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch, bool inverse) -> void {
  pb = gtk_image_get_pixbuf(pictureData.picture);

  int        xMax = pos.x + dim.width;
  int        yMax = pos.y + dim.height;

  if (yMax > height) { yMax = height; }
  if (xMax > width) { xMax = width; }

  if (pixelResolution == PixelResolution::MONO) {
    for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
      uint32_t p = (q * pitch) << 3;
      for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
        // int v = (255 - bitmapData[p]);
        // if (v != 255) {
        //   v &= 0xE0; // 8 levels of grayscale
        //   setPixel({i, j}, v);
        // }
        uint8_t v = bitmapData[p >> 3] & LUT1BIT[p & 7];
        if (inverse) {
          if (v) { setPixel({i, j}, 0xFF); }
        } else {
          if (v) { setPixel({i, j}, 0); }
        }
      }
    }
  } else {
    for (uint16_t j = pos.y, q = 0; j < yMax; ++j, ++q) {
      uint32_t p = q * pitch;
      for (uint16_t i = pos.x; i < xMax; ++i, ++p) {
        // int v = (255 - bitmapData[p]);
        // if (v != 255) {
        //   v &= 0xF0; // 16 levels of grayscale
        //   setPixel({i, j}, v);
        // }
        uint8_t v = (255 - bitmapData[p]) & 0xF0;
        if (v != 0xF0) { setPixel({i, j}, inverse ? ~v : v); }
      }
    }
  }
}

auto Screen::clear() -> void {
  pb = gtk_image_get_pixbuf(pictureData.picture);
  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white
}

auto Screen::test() -> void {
  static int N = 0;

  pb = gtk_image_get_pixbuf(pictureData.picture);

  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white


  for (uint16_t r = 0; r < pictureData.rows; ++r)
    for (uint16_t c = 0; c < pictureData.cols; ++c)
      if ((r + N) / 20 % 2 && (c + N) / 20 % 2) { setPixel({c, r}, 0); }

  N = (N + 1) % 100;

  update();
}

auto Screen::update(bool noFull) -> void {
  gtk_image_set_from_pixbuf(GTK_IMAGE(pictureData.picture),
                            gtk_image_get_pixbuf(pictureData.picture));
}

extern void exitApp();

static auto destroyApp(GtkWidget *widget, gpointer data) -> void {
  char const *TAG = "DestroyApp";
  LOG_I("Exiting application...");
  exitApp();
  gtk_main_quit();
  if (Screen::pixels != nullptr) {
    Screen::pixels.reset();
  }
}

auto Screen::setup(PixelResolution resolution, Orientation orientation) -> void {
  setOrientation(orientation);
  setPixelResolution(resolution);

  #if TOUCH_TRIAL
    static GtkWidget *vbox1;
  #else
    static GtkWidget *vbox1, *hbox1;
  #endif

  gtk_init(nullptr, nullptr);

  pictureData.rows   = height;
  pictureData.cols   = width;
  pictureData.stride = width * BYTES_PER_PIXEL;
  pictureData.stride += (4 - (pictureData.stride % 4)) % 4; // ensure multiple of 4

  pixels = std::make_unique<guchar[]>(height * pictureData.stride);
  memset(pixels.get(), 255, height * pictureData.stride); // clear to white


  pb = gdk_pixbuf_new_from_data(pixels.get(),
        GDK_COLORSPACE_RGB, // colorspace
        0,                  // has_alpha
        8,                  // bits-per-sample
        width,              // rows
        height,             // cols
        pictureData.stride, // rowstride
        freePixels,         // destroy_fn
        nullptr             // destroy_fn_data
      );

  for (uint16_t r = 0; r < height; ++r)
    for (uint16_t c = 0; c < width; ++c) setPixel({c, r}, 255);

  pictureData.picture = GTK_IMAGE(gtk_image_new_from_pixbuf(pb));
  // g_object_unref(
  //     pb); // transfer sole ownership to the GtkImage; freePixels fires when it's destroyed

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "epub-inkplate");
  gtk_container_set_border_width(GTK_CONTAINER(window), 10);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_set_homogeneous(GTK_BOX(vbox1), FALSE);

  #if TOUCH_TRIAL
    pictureBox = gtk_event_box_new();

    gtk_container_add(GTK_CONTAINER(pictureBox), (GtkWidget *)pictureData.picture);
    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(pictureBox), FALSE, FALSE, 0);
  #else

    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(pictureData.picture), FALSE, FALSE, 0);
    hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    gtk_box_set_homogeneous(GTK_BOX(hbox1), FALSE);

    leftButton   = gtk_button_new_with_label(" < ");
    rightButton  = gtk_button_new_with_label(" > ");
    upButton     = gtk_button_new_with_label(" << ");
    downButton   = gtk_button_new_with_label(" >> ");
    selectButton = gtk_button_new_with_label("Select");
    homeButton   = gtk_button_new_with_label("DClick-Select");

    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(upButton),     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(leftButton),   FALSE, TRUE,  0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(rightButton),  FALSE, TRUE,  0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(downButton),   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(selectButton), TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(homeButton),   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(hbox1),        FALSE, FALSE, 0);
  #endif

  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox1));

  g_signal_connect(window, "destroy", G_CALLBACK(destroyApp), nullptr);

  gtk_widget_show_all(window);
}

auto Screen::setPixelResolution(PixelResolution resolution, bool force) -> void {
  if (force || (pixelResolution != resolution)) {
    pixelResolution = resolution;
  }
}

auto Screen::setOrientation(Orientation orient) -> void {
  orientation = orient;
  if ((orientation == Orientation::LEFT) || (orientation == Orientation::RIGHT)) {
    width  = 540; //600;
    height = 960; //800;
  } else {
    width  = 960; //800;
    height = 540; //600;
  }
}
