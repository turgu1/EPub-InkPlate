#define _SCREEN_BOTTOM_ 1
#include "viewers/screen_bottom.hpp"

#include "controllers/clock.hpp"
#include "models/config.hpp"
#include "viewers/battery_viewer.hpp"
#include "screen.hpp"

#include <iomanip>
#include <ctime>

const std::string ScreenBottom::dw[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void
ScreenBottom::show(int16_t page_nbr, int16_t page_count)
{
  Font * font = fonts.get(FONT);

  Page::Format fmt = {
      .line_height_factor =   1.0,
      .font_index         =  FONT,
      .font_size          = FONT_SIZE,
      .indent             =     0,
      .margin_left        =     0,
      .margin_right       =     0,
      .margin_top         =     0,
      .margin_bottom      =     0,
      .screen_left        =    10,
      .screen_right       =    10,
      .screen_top         =    10,
      .screen_bottom      =    10,
      .width              =     0,
      .height             =     0,
      .vertical_align     =     0,
      .trim               =  true,
      .pre                = false,
      .font_style         = Fonts::FaceStyle::NORMAL,
      .align              = CSS::Align::CENTER,
      .text_transform     = CSS::TextTransform::NONE,
      .display            = CSS::Display::INLINE
    };

  page.set_limits(fmt);

  std::ostringstream ostr;

  uint16_t h = font->get_chars_height(FONT_SIZE) + 10;

  LOG_D("Dim [%u, %u], Pos[%u, %u]", 
        Screen::get_width(), h, 
        0, Screen::get_height() - h);

  page.clear_region(Dim(Screen::get_width(), h),
                    Pos(0, Screen::get_height() - h));

  // page.put_highlight(Dim(Screen::get_width(), h ),
  //                    Pos(0, Screen::get_height() - h));

  if (page_nbr != -1) {
    ostr << page_nbr + 1 << " / " << page_count;

    page.put_str_at(ostr.str(), 
                    Pos(Page::HORIZONTAL_CENTER, 
                        Screen::get_height() + font->get_descender_height(FONT_SIZE) - 2), 
                    fmt);
  }
  else if (page_count != -1) {
    ostr << "PgCalc... " << page_count << "%";

    page.put_str_at(ostr.str(), 
                    Pos(Page::HORIZONTAL_CENTER, 
                        Screen::get_height() + font->get_descender_height(FONT_SIZE) - 2), 
                    fmt);
  }

  #if EPUB_INKPLATE_BUILD
    int8_t show_heap;
    config.get(Config::Ident::SHOW_HEAP, &show_heap);

    if (show_heap != 0) {
      ostr.str(std::string());
      ostr << uxTaskGetStackHighWaterMark(nullptr)
           << " / "
           << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) 
           << " / " 
           << heap_caps_get_free_size(MALLOC_CAP_8BIT);
      fmt.align = CSS::Align::RIGHT;
      page.put_str_at(ostr.str(), 
                      Pos(Page::HORIZONTAL_CENTER, 
                          Screen::get_height() + font->get_descender_height(FONT_SIZE) - 2), 
                      fmt);
    }

    BatteryViewer::show();
  #endif

  #if DATE_TIME_RTC
    int8_t show_rtc;
    config.get(Config::Ident::SHOW_RTC, &show_rtc);

    if (show_rtc != 0) {
      struct tm time;

      time_t epoch;
      Clock::get_date_time(epoch);
      localtime_r(&epoch, &time);

      ostr.str(std::string());
      ostr << dw[static_cast<int8_t>(time.tm_wday)] << " - "
           << std::setfill('0') 
           << std::setw(2) << +(time.tm_mon + 1)  << '/' 
           << std::setw(2) << +time.tm_mday << ' '
           << std::setw(2) << +time.tm_hour << ':' 
           << std::setw(2) << +time.tm_min;

      fmt.align = CSS::Align::RIGHT;
      page.put_str_at(ostr.str(),
                      Pos(Page::HORIZONTAL_CENTER, 
                          Screen::get_height() + font->get_descender_height(FONT_SIZE) - 2), 
                      fmt);
    }
  #endif

}