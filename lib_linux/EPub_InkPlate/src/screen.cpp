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

Screen Screen::singleton;

uint16_t Screen::width;
uint16_t Screen::height;

const uint8_t Screen::LUT1BIT[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

void freePixels(guchar *pixels, gpointer data) { delete[] pixels; }

inline void setrgb(guchar *a, int row, int col, int stride, guchar color) {
  int p = row * stride + col * BYTES_PER_PIXEL;
  a[p] = a[p + 1] = a[p + 2] = color;
}

void Screen::drawPicture(PicturePtr &picture, Pos pos) {

  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  auto dim        = picture->getDim();
  auto bitmapData = picture->getBitmap();

  if (pos.x > width) pos.x = 0;
  if (pos.y > height) pos.y = 0;

  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  if (pixelResolution == PixelResolution::ONE_BIT) {
    static int16_t err[601];
    int16_t error;
    memset(err, 0, 601 * 2);

    for (int j = pos.y, q = 0; j < yMax; j++, q++) {
      for (int i = pos.x, p = q * dim.width, k = 0; i < (xMax - 1); i++, p++, k++) {
        int32_t v = bitmapData[p] + err[k + 1];
        if (v > 128) {
          error = (v - 255);
          setrgb(g, j, i, pictureData.stride, 255);
        } else {
          error = v;
          setrgb(g, j, i, pictureData.stride, 0);
        }
        if (k != 0) {
          err[k - 1] += error / 8;
        }
        err[k] += 3 * error / 8;
        err[k + 1] = error / 8;
        err[k + 2] += 3 * error / 8;
      }
    }
  } else {
    for (int j = pos.y, q = 0; j < yMax; j++, q++) {
      for (int i = pos.x, p = q * dim.width; i < xMax; i++, p++) {
        setrgb(g, j, i, pictureData.stride, bitmapData[p]);
      }
    }
  }
}

void Screen::drawRectangle(Dim dim, Pos pos,
                           Color color) //, bool show)
{
  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  for (int i = pos.x; i < xMax; i++) {
    setrgb(g, pos.y, i, pictureData.stride, color);
    setrgb(g, yMax - 1, i, pictureData.stride, color);
  }
  for (int j = pos.y; j < yMax; j++) {
    setrgb(g, j, pos.x, pictureData.stride, color);
    setrgb(g, j, xMax - 1, pictureData.stride, color);
  }
}

void Screen::drawArc(uint16_t xMid, uint16_t yMid, uint8_t radius, Corner corner, Color color) {
  int16_t f     = 1 - radius;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * radius;
  int16_t x     = 0;
  int16_t y     = radius;

  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  // Bottom middle
  //  drawPixel(xMid, yMid + radius);

  // Top Middle
  //  drawPixel(xMid, yMid - radius);

  // Right Middle
  //  drawPixel(xMid + radius, yMid);

  // Left Middle
  //  drawPixel(xMid - radius, yMid);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    switch (corner) {
    case Corner::TOP_LEFT:
      setrgb(g, yMid - y, xMid - x, pictureData.stride, color);
      setrgb(g, yMid - x, xMid - y, pictureData.stride, color);
      break;

    case Corner::TOP_RIGHT:
      setrgb(g, yMid - y, xMid + x, pictureData.stride, color);
      setrgb(g, yMid - x, xMid + y, pictureData.stride, color);
      break;

    case Corner::LOWER_LEFT:
      setrgb(g, yMid + y, xMid - x, pictureData.stride, color);
      setrgb(g, yMid + x, xMid - y, pictureData.stride, color);
      break;

    case Corner::LOWER_RIGHT:
      setrgb(g, yMid + y, xMid + x, pictureData.stride, color);
      setrgb(g, yMid + x, xMid + y, pictureData.stride, color);
      break;
    }
  }
}

void Screen::drawRoundRectangle(Dim dim, Pos pos,
                                Color color) //, bool show)
{
  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  for (int i = pos.x + 10; i < xMax - 10; i++) {
    setrgb(g, pos.y, i, pictureData.stride, color);
    setrgb(g, yMax - 1, i, pictureData.stride, color);
  }
  for (int j = pos.y + 10; j < yMax - 10; j++) {
    setrgb(g, j, pos.x, pictureData.stride, color);
    setrgb(g, j, xMax - 1, pictureData.stride, color);
  }

  drawArc(pos.x + 10, pos.y + 10, 10, Corner::TOP_LEFT, color);
  drawArc(pos.x + dim.width - 11, pos.y + 10, 10, Corner::TOP_RIGHT, color);
  drawArc(pos.x + 10, pos.y + dim.height - 11, 10, Corner::LOWER_LEFT, color);
  drawArc(pos.x + dim.width - 11, pos.y + dim.height - 11, 10, Corner::LOWER_RIGHT, color);
}

void Screen::colorizeRegion(Dim dim, Pos pos, Color color) {
  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  int16_t xMax = pos.x + dim.width;
  int16_t yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  for (int j = pos.y; j < yMax; j++) {
    for (int i = pos.x; i < xMax; i++) {
      setrgb(g, j, i, pictureData.stride, color);
    }
  }
}

void Screen::drawGlyph(const unsigned char *bitmapData, Dim dim, Pos pos, uint16_t pitch) {
  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  guchar *g     = gdk_pixbuf_get_pixels(pb);

  int xMax = pos.x + dim.width;
  int yMax = pos.y + dim.height;

  if (yMax > height) yMax = height;
  if (xMax > width) xMax = width;

  if (pixelResolution == PixelResolution::ONE_BIT) {
    for (int j = pos.y, q = 0; j < yMax; j++, q++) {
      for (int i = pos.x, p = (q * pitch) << 3; i < xMax; i++, p++) {
        // int v = (255 - bitmapData[p]);
        // if (v != 255) {
        //   v &= 0xE0; // 8 levels of grayscale
        //   setrgb(g, j, i, pictureData.stride, v);
        // }
        uint8_t v = bitmapData[p >> 3] & LUT1BIT[p & 7];
        if (v) setrgb(g, j, i, pictureData.stride, 0);
      }
    }
  } else {
    for (int j = pos.y, q = 0; j < yMax; j++, q++) {
      for (int i = pos.x, p = q * pitch; i < xMax; i++, p++) {
        // int v = (255 - bitmapData[p]);
        // if (v != 255) {
        //   v &= 0xE0; // 8 levels of grayscale
        //   setrgb(g, j, i, pictureData.stride, v);
        // }
        uint8_t v = (255 - bitmapData[p]) & 0xE0;
        if (v != 0xE0) setrgb(g, j, i, pictureData.stride, v);
      }
    }
  }
}

void Screen::clear() {
  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);
  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white
}

void Screen::test() {
  static int N = 0;

  GdkPixbuf *pb = gtk_image_get_pixbuf(pictureData.picture);

  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white

  guchar *g = gdk_pixbuf_get_pixels(pb);

  for (int r = 0; r < pictureData.rows; r++)
    for (int c = 0; c < pictureData.cols; c++)
      if ((r + N) / 20 % 2 && (c + N) / 20 % 2) setrgb(g, r, c, pictureData.stride, 0);

  N = (N + 1) % 100;

  update();
}

void Screen::update(bool noFull) {
  gtk_image_set_from_pixbuf(GTK_IMAGE(pictureData.picture),
                            gtk_image_get_pixbuf(pictureData.picture));
}

extern void exitApp();

static void destroy_app(GtkWidget *widget, gpointer data) {
  exitApp();
  gtk_main_quit();
}

void Screen::setup(PixelResolution resolution, Orientation orientation) {
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

  guchar *pixels = (guchar *)calloc(height * pictureData.stride, 1);

  for (int r = 0; r < height; r++)
    for (int c = 0; c < width; c++) setrgb(pixels, r, c, pictureData.stride, 255);

  GdkPixbuf *pb = gdk_pixbuf_new_from_data(pixels,
                                           GDK_COLORSPACE_RGB, // colorspace
                                           0,                  // has_alpha
                                           8,                  // bits-per-sample
                                           width,              // rows
                                           height,             // cols
                                           pictureData.stride, // rowstride
                                           freePixels,         // destroy_fn
                                           nullptr             // destroy_fn_data
  );

  pictureData.picture = GTK_IMAGE(gtk_image_new_from_pixbuf(pb));

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "epub-inkplate");
  gtk_container_set_border_width(GTK_CONTAINER(window), 10);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_set_homogeneous(GTK_BOX(vbox1), FALSE);

  #if TOUCH_TRIAL
    picture_box = gtk_event_box_new();

    gtk_container_add(GTK_CONTAINER(picture_box), (GtkWidget *)pictureData.picture);
    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(picture_box), FALSE, FALSE, 0);
  #else

    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(pictureData.picture), FALSE, FALSE, 0);
    hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    gtk_box_set_homogeneous(GTK_BOX(hbox1), FALSE);

    left_button   = gtk_button_new_with_label(" < ");
    right_button  = gtk_button_new_with_label(" > ");
    up_button     = gtk_button_new_with_label(" << ");
    down_button   = gtk_button_new_with_label(" >> ");
    select_button = gtk_button_new_with_label("Select");
    home_button   = gtk_button_new_with_label("DClick-Select");

    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(up_button), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(left_button), FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(right_button), FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(down_button), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(select_button), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(home_button), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(hbox1), FALSE, FALSE, 0);
  #endif

  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox1));

  g_signal_connect(window, "destroy", G_CALLBACK(destroy_app), nullptr);

  gtk_widget_show_all(window);
}

void Screen::setPixelResolution(PixelResolution resolution, bool force) {
  if (force || (pixelResolution != resolution)) {
    pixelResolution = resolution;
  }
}

void Screen::setOrientation(Orientation orient) {
  orientation = orient;
  if ((orientation == Orientation::LEFT) || (orientation == Orientation::RIGHT)) {
    width  = 600;
    height = 800;
  } else {
    width  = 800;
    height = 600;
  }
}
