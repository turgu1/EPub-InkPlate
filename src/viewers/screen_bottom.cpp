#define _SCREEN_BOTTOM_ 1
#include "viewers/screen_bottom.hpp"

#include "models/config.hpp"
#include "viewers/battery_viewer.hpp"
#include "screen.hpp"

#include <iomanip>

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
  
  page.clear_region(Dim(screen.WIDTH, font->get_line_height(FONT_SIZE)),
                    Pos(0, screen.HEIGHT - font->get_line_height(FONT_SIZE)));
                    
  if ((page_nbr != -1) && (page_count != -1)) {
    ostr << page_nbr + 1 << " / " << page_count;

    page.put_str_at(ostr.str(), 
                    Pos(Page::HORIZONTAL_CENTER, 
                        Screen::HEIGHT + font->get_descender_height(FONT_SIZE) - 2), 
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
                          Screen::HEIGHT + font->get_descender_height(FONT_SIZE) - 2), 
                      fmt);
    }

    BatteryViewer::show();
  #endif

  #if DATE_TIME_RTC
    int8_t show_rtc;
    config.get(Config::Ident::SHOW_RTC, &show_rtc);

    if (show_rtc != 0) {
      uint16_t y;
      uint8_t m, d, h, mm, s;

      #if EPUB_INKPLATE_BUILD
        RTC::WeekDay dow;
        rtc.get_date_time(y, m, d, h, mm, s, dow);
      #else
        int8_t dow = 1;
        y = 2022; m = d = 1, h = mm = s = 0;
      #endif

      ostr.str(std::string());
      ostr << dw[(int8_t) dow] << " - "
           << std::setfill('0') 
           << std::setw(2) <<  +m << '/' 
           << std::setw(2) <<  +d << ' '
           << std::setw(2) <<  +h << ':' 
           << std::setw(2) << +mm;

      fmt.align = CSS::Align::RIGHT;
      page.put_str_at(ostr.str(),
                      Pos(Page::HORIZONTAL_CENTER, 
                          Screen::HEIGHT + font->get_descender_height(FONT_SIZE) - 2), 
                      fmt);
    }
  #endif

}