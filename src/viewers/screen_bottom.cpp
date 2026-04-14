#define _SCREEN_BOTTOM_ 1
#include "viewers/screen_bottom.hpp"

#include "controllers/clock.hpp"
#include "models/config.hpp"
#include "screen.hpp"
#include "viewers/battery_viewer.hpp"

#include <ctime>
#include <iomanip>

const std::string ScreenBottom::dw[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

auto ScreenBottom::show(PagePtr &page, int16_t pageNbr, int16_t pageCount) -> void {
  Font *font = fonts.get(FONT);

  Page::Format fmt = {
      .fontIndex = FONT,
      .fontSize  = FONT_SIZE,
      .align     = CSS::Align::CENTER,
  };

  page->setLimits(fmt);

  std::ostringstream ostr;

  int8_t showHeap = 0;

  #if EPUB_INKPLATE_BUILD
    config.get(Config::Ident::SHOW_HEAP, &showHeap);
  #endif

  uint16_t h = font->getCharsHeight(FONT_SIZE) + 10;

  LOG_D("Dim [%u, %u], Pos[%u, %u]", Screen::getWidth(), h, 0, Screen::getHeight() - h);

  page->clearRegion(Dim(Screen::getWidth(), h), Pos(0, Screen::getHeight() - h));

  // page->putHighlight(Dim(Screen::getWidth(), h ),
  //                    Pos(0, Screen::getHeight() - h));

  if (showHeap == 0) {
    if (pageNbr != -1) {
      ostr << pageNbr + 1 << " / " << pageCount;

      page->putStrAt(ostr.str(),
                     Pos(Page::HORIZONTAL_CENTER,
                         Screen::getHeight() + font->getDescenderHeight(FONT_SIZE) - 2),
                     fmt);
    } else if (pageCount != -1) {
      ostr << "PgCalc... " << pageCount << "%";

      page->putStrAt(ostr.str(),
                     Pos(Page::HORIZONTAL_CENTER,
                         Screen::getHeight() + font->getDescenderHeight(FONT_SIZE) - 2),
                     fmt);
    }
  }

  #if EPUB_INKPLATE_BUILD

    if (showHeap != 0) {
      ostr.str(std::string());
      ostr << uxTaskGetStackHighWaterMark(nullptr) << " / "
           << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) << " / "
           << heap_caps_get_free_size(MALLOC_CAP_8BIT);
      fmt.align = CSS::Align::RIGHT;
      page->putStrAt(ostr.str(),
                     Pos(Page::HORIZONTAL_CENTER,
                         Screen::getHeight() + font->getDescenderHeight(FONT_SIZE) - 2),
                     fmt);
    }

    BatteryViewer::show(page);
  #endif

  #if DATE_TIME_RTC
    int8_t showRtc = 0;
    config.get(Config::Ident::SHOW_RTC, &showRtc);

    if (showRtc != 0) {
      struct tm time;

      time_t epoch;
      Clock::getDateTime(epoch);
      localtime_r(&epoch, &time);

      ostr.str(std::string());
      ostr << dw[static_cast<int8_t>(time.tm_wday)] << " - " << std::setfill('0') << std::setw(2)
           << +(time.tm_mon + 1) << '/' << std::setw(2) << +time.tm_mday << ' ' << std::setw(2)
           << +time.tm_hour << ':' << std::setw(2) << +time.tm_min;

      fmt.align = CSS::Align::RIGHT;
      page->putStrAt(ostr.str(),
                     Pos(Page::HORIZONTAL_CENTER,
                         Screen::getHeight() + font->getDescenderHeight(FONT_SIZE) - 2),
                     fmt);
    }
  #endif
}