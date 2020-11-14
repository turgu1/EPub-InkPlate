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
#include <cstring>

#define BYTES_PER_PIXEL 3

Screen Screen::singleton;

const uint8_t Screen::LUT1BIT[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

void 
free_pixels(guchar * pixels, gpointer data)
{
  delete [] pixels;
}

inline void 
setrgb(guchar * a, int row, int col, int stride,
            guchar color) 
{
  int p = row * stride + col * BYTES_PER_PIXEL;
  a[p] = a[p+1] = a[p+2] = color;
}

void 
Screen::draw_bitmap(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  int16_t  x, 
  int16_t  y) //, bool show)
{
  if (bitmap_data == nullptr) return;
  
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  guchar    * g  = gdk_pixbuf_get_pixels(pb);
  
  // if (show) {
  //   unsigned char * p = bitmap_data;
  //   for (int j = 0; j < image_height; j++) {
  //     for (int i = 0; i < image_width; i++) {
  //       std::cout << std::setw(2) << std::setfill('0') << std::hex << (int) *p++;
  //     }
  //     std::cout << std::endl;
  //   }
  // }   

  if (x < 0) x = 0;
  if (y < 0) y = 0;

  int16_t x_max = x + width;
  int16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  static int16_t err[601];
  int16_t error;
  memset(err, 0, 601*2);

  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = q * width, k = 0; i < (x_max - 1); i++, p++, k++) {
      int32_t v = bitmap_data[p] + err[k + 1];
      if (v > 128) {
        error = (v - 255);
        setrgb(g, j, i, id.stride, 255);
      }
      else {
        error = v;
        setrgb(g, j, i, id.stride, 0);
      }
      if (k != 0) {
        err[k - 1] += error / 8;
      }
      err[k]     += 3 * error / 8;
      err[k + 1]  =     error / 8;
      err[k + 2] += 3 * error / 8;
    }
  }
}

// procedure dither ( width, height,      --size of image
// get-pixel(x,y), set_output-pixel(x,y)  --access to pixels 
// error_arr: array[0..width] of integer; --holds the errors 
// error:integer;                         --error for current pixel 

// j: integer <- 0; 
// until j = height do 
//   i: integer <- 0; 
//   until i = width - 1 loop                          -- don't go all the way to edge 
//     val <- get_pixel(x,y) + error_arr[x + 1];       -- pixel value with error 
//     if val > 128                                    -- above threshold 
//     then set_output_pixel(x,y); error <- val - 255; -- output white 
//     else error <- val;                              -- leave output black 
//     if x != O then                                  -- don't spread left at edge 
//       error_arr[x - 1] <- error_arr[x - 1] + error / 8; --lower left pixel 
//     error_arr[x] <- error arr[x] + 3 * error / 8;    --pixel below 
//     error_arr[x + 1] <- error / 8; --initial error for lower right 
//     error_arr[x + 2] <- error_arr[x + 2] + 3 * error / 8; --pixel to the right 
//     x <- x + 1; 
//   ­endloop; 
//   y <- y + 1; 
// endloop; 

// void 
// Screen::draw_bitmap(
//   const unsigned char * bitmap_data, 
//   uint16_t width, 
//   uint16_t height, 
//   int16_t  x, 
//   int16_t  y) //, bool show)
// {
//   if (bitmap_data == nullptr) return;
  

//   GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
//   guchar    * g  = gdk_pixbuf_get_pixels(pb);
  
//   // if (show) {
//   //   unsigned char * p = bitmap_data;
//   //   for (int j = 0; j < image_height; j++) {
//   //     for (int i = 0; i < image_width; i++) {
//   //       std::cout << std::setw(2) << std::setfill('0') << std::hex << (int) *p++;
//   //     }
//   //     std::cout << std::endl;
//   //   }
//   // }   

//   if (x < 0) x = 0;
//   if (y < 0) y = 0;

//   int16_t x_max = x + width;
//   int16_t y_max = y + height;

//   if (y_max > HEIGHT) y_max = HEIGHT;
//   if (x_max > WIDTH ) x_max = WIDTH;
//   for (int j = y, q = 0; j < y_max; j++, q++) {
//     for (int i = x, p = q * width; i < x_max; i++, p++) {
//       int v = bitmap_data[p];
//       if (v != 255) {
//         v &= 0xE0; // 8 levels of grayscale
//         setrgb(g, j, i, id.stride, v);
//       }
//     }
//   }
// }

void 
Screen::draw_rectangle(
  uint16_t width, 
  uint16_t height, 
  int16_t  x, 
  int16_t  y,
  uint8_t  color) //, bool show)
{
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  guchar    * g  = gdk_pixbuf_get_pixels(pb);
  
  int16_t x_max = x + width;
  int16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (int i = x; i < x_max; i++) {
    setrgb(g, y, i, id.stride, color);
    setrgb(g, y_max - 1, i, id.stride, color);
  }
  for (int j = y; j < y_max; j++) {
    setrgb(g, j, x, id.stride, color);
    setrgb(g, j, x_max - 1, id.stride, color);
  }
}

void 
Screen::draw_glyph(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
  uint16_t pitch,
  int16_t x, 
  int16_t y) //, bool show)
{
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  guchar    * g  = gdk_pixbuf_get_pixels(pb);

  int x_max = x + width;
  int y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = (q * pitch) << 3; i < x_max; i++, p++) {
      // int v = (255 - bitmap_data[p]);
      // if (v != 255) {
      //   v &= 0xE0; // 8 levels of grayscale
      //   setrgb(g, j, i, id.stride, v);
      // }
      uint8_t v = bitmap_data[p >> 3] & LUT1BIT[p & 7];
      if (v) setrgb(g, j, i, id.stride, 0);
    }
  }
}

void 
Screen::clear()
{
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white
}

void 
Screen::test() 
{
  static int N = 0;

  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);

  gdk_pixbuf_fill(pb, 0xFFFFFFFF); // clear to white

  guchar * g = gdk_pixbuf_get_pixels(pb);

  for (int r = 0; r < id.rows; r++)
    for (int c = 0; c < id.cols; c++)
      if ((r + N) / 20 % 2 && (c + N) / 20 % 2)
        setrgb(g, r, c, id.stride, 0);

  N = (N + 1) % 100;

  update();
}

void 
Screen::update()
{
  gtk_image_set_from_pixbuf(GTK_IMAGE(id.image), gtk_image_get_pixbuf(id.image));
}

static void destroy_app( GtkWidget *widget, gpointer   data )
{
  gtk_main_quit();
}

void
Screen::setup()
{
  static GtkWidget 
    * vbox1,
    * vbox2,
    * hbox1,
    * hbox2;

  gtk_init(nullptr, nullptr);

  id.rows    = HEIGHT;
  id.cols    = WIDTH;
  id.stride  = WIDTH * BYTES_PER_PIXEL;
  id.stride += (4 - (id.stride % 4)) % 4; // ensure multiple of 4

  guchar * pixels = (guchar *) calloc(HEIGHT * id.stride, 1);
  
  for (int r = 0; r < HEIGHT; r++)
    for (int c = 0; c < WIDTH; c++)
        setrgb(pixels, r, c, id.stride, 255);

  GdkPixbuf *pb = gdk_pixbuf_new_from_data(
    pixels,
    GDK_COLORSPACE_RGB, // colorspace
    0,                  // has_alpha
    8,                  // bits-per-sample
    WIDTH,              // rows
    HEIGHT,             // cols
    id.stride,          // rowstride
    free_pixels,        // destroy_fn
    nullptr             // destroy_fn_data
  );

  id.image = GTK_IMAGE(gtk_image_new_from_pixbuf(pb));

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "epub-inkplate");
  gtk_container_set_border_width(GTK_CONTAINER (window), 10);  
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL,    5);
  hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,  5);
  vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL,    5);
  hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,  5);

  gtk_box_set_homogeneous(GTK_BOX(vbox1), FALSE);
  gtk_box_set_homogeneous(GTK_BOX(hbox1), FALSE);
  gtk_box_set_homogeneous(GTK_BOX(vbox2), FALSE);
  gtk_box_set_homogeneous(GTK_BOX(hbox2), TRUE);

    left_button = gtk_button_new_with_label("Left"  );
   right_button = gtk_button_new_with_label("Right" );
      up_button = gtk_button_new_with_label("Up"    );
    down_button = gtk_button_new_with_label("Down"  );
  select_button = gtk_button_new_with_label("Select");
    home_button = gtk_button_new_with_label("Home"  );

  gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(id.image     ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(up_button    ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox2), GTK_WIDGET(left_button  ), FALSE, TRUE,  0);
  gtk_box_pack_start(GTK_BOX(hbox2), GTK_WIDGET(right_button ), FALSE, TRUE,  0);
  gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(hbox2        ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), GTK_WIDGET(down_button  ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(vbox2        ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(select_button), TRUE,  TRUE,  0);
  gtk_box_pack_start(GTK_BOX(hbox1), GTK_WIDGET(home_button  ), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(hbox1), FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER(window), GTK_WIDGET(vbox1));

  g_signal_connect(window, "destroy", G_CALLBACK(destroy_app), nullptr);

  gtk_widget_show_all(window);
}