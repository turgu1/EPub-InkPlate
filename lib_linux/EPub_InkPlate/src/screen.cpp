// Using GTK on Linux to emulate InkPlate screen
//
// As all GTK related code is located in this module, we also implement
// some part of the event manager code here...

#include "eventmgr.hpp"

#define __SCREEN__ 1
#include "screen.hpp"

#include "epub.hpp"
#include <iomanip>

#define BYTES_PER_PIXEL 3

static const char * TAG = "Screen";

Screen Screen::singleton;

void 
free_pixels(guchar * pixels, gpointer data)
{
  delete [] pixels;
}

#define BUTTON_EVENT(button, msg) \
  static void button##_clicked(GObject * button, GParamSpec * property, gpointer data) { \
    event_mgr.button(); \
  }

BUTTON_EVENT(left,   "Left Clicked"  )
BUTTON_EVENT(right,  "Right Clicked" )
BUTTON_EVENT(up,     "Up Clicked"    )
BUTTON_EVENT(down,   "Down Clicked"  )
BUTTON_EVENT(select, "Select Clicked")
BUTTON_EVENT(home,   "Home Clicked"  )

inline void 
setrgb(guchar * a, int row, int col, int stride,
            guchar r, guchar g, guchar b) 
{
  int p = row * stride + col * BYTES_PER_PIXEL;
  a[p] = r; a[p+1] = g; a[p+2] = b;
}

void 
Screen::put_bitmap(const unsigned char * bitmap_data, 
                   int16_t width, 
                   int16_t height, 
                   int16_t x, int16_t y) //, bool show)
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
  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = q * width; i < x_max; i++) {
      int v = bitmap_data[p];
      if (v != 255) {
        v &= 0xE0; // 8 levels of grayscale
        setrgb(g, j, i, id.stride, v, v, v);
      }
      p++;
    }
  }
}

void 
Screen::put_highlight(int16_t width, int16_t height, 
                   int16_t x, int16_t y) //, bool show)
{
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  guchar    * g  = gdk_pixbuf_get_pixels(pb);
  
  int16_t x_max = x + width;
  int16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  int v = 0xE0;

  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = q * width; i < x_max; i++, p++) {
      setrgb(g, j, i, id.stride, v, v, v);
    }
  }
}


void 
Screen::clear_region(int16_t width, int16_t height, 
                     int16_t x, int16_t y) //, bool show)
{
  GdkPixbuf * pb = gtk_image_get_pixbuf(id.image);
  guchar    * g  = gdk_pixbuf_get_pixels(pb);
  
  int16_t x_max = x + width;
  int16_t y_max = y + height;

  if (y_max > HEIGHT) y_max = HEIGHT;
  if (x_max > WIDTH ) x_max = WIDTH;

  int v = 0xFF;

  for (int j = y, q = 0; j < y_max; j++, q++) {
    for (int i = x, p = q * width; i < x_max; i++, p++) {
      setrgb(g, j, i, id.stride, v, v, v);
    }
  }
}

void 
Screen::put_bitmap_invert(
  const unsigned char * bitmap_data, 
  uint16_t width, 
  uint16_t height, 
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
    for (int i = x, p = q * width; i < x_max; i++) {
      int v = (255 - bitmap_data[p]);
      if (v != 255) {
        v &= 0xE0; // 8 levels of grayscale
        setrgb(g, j, i, id.stride, v, v, v);
      }
      p++;
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
        setrgb(g, r, c, id.stride, 0, 0, 0);

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
  epub.close_file();
  gtk_main_quit();
}

Screen::Screen()
{
  static GtkWidget 
    * window, 
    * vbox1,
    * vbox2,
    * hbox1,
    * hbox2,
    * left_button,
    * right_button,
    * up_button,
    * down_button,
    * select_button,
    * home_button;

  gtk_init(nullptr, nullptr);

  id.rows    = HEIGHT;
  id.cols    = WIDTH;
  id.stride  = WIDTH * BYTES_PER_PIXEL;
  id.stride += (4 - (id.stride % 4)) % 4; // ensure multiple of 4

  guchar * pixels = (guchar *) calloc(HEIGHT * id.stride, 1);
  
  for (int r = 0; r < HEIGHT; r++)
    for (int c = 0; c < WIDTH; c++)
        setrgb(pixels, r, c, id.stride, 255, 255, 255);

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

  g_signal_connect(G_OBJECT(  left_button), "clicked", G_CALLBACK(  left_clicked), (gpointer) window);
  g_signal_connect(G_OBJECT( right_button), "clicked", G_CALLBACK( right_clicked), (gpointer) window);
  g_signal_connect(G_OBJECT(    up_button), "clicked", G_CALLBACK(    up_clicked), (gpointer) window);
  g_signal_connect(G_OBJECT(  down_button), "clicked", G_CALLBACK(  down_clicked), (gpointer) window);
  g_signal_connect(G_OBJECT(select_button), "clicked", G_CALLBACK(select_clicked), (gpointer) window);
  g_signal_connect(G_OBJECT(  home_button), "clicked", G_CALLBACK(  home_clicked), (gpointer) window);

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