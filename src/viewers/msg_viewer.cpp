// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __MSG_VIEWER__ 1
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/page.hpp"

#include <cstdarg>
#include <memory>

#if EPUB_INKPLATE_BUILD
  #include "inkplate_platform.hpp"
  #include "nvs.h"
#endif

#define BUFFER_SIZE 250

auto MsgViewer::show(MsgType msgType, bool pressAKey, bool clearScreen, const char *title,
                     const char *fmtStr, ...) -> ConfirmDataPtr {
  HimemUniquePtr<char[]> buff = makeUniqueHimem<char[]>(BUFFER_SIZE);
  auto page                   = Page::Make(appFonts);
  auto confirmData            = ConfirmData::Make();

  uint16_t width = Screen::getWidth() - 60;

  if (page->getComputeMode() == Page::ComputeMode::LOCATION)
    return nullptr; // Cannot be used durint location computation

  va_list args;
  va_start(args, fmtStr);
  vsnprintf(buff.get(), BUFFER_SIZE, fmtStr, args);
  va_end(args);

  Page::Format fmt = {
      .fontIndex    = ICONS_FONT_INDEX,
      .fontSize     = 24,
      .marginLeft   = 10,
      .marginRight  = 10,
      .marginTop    = 30,
      .marginBottom = 10,
      .screenLeft   = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenRight  = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenTop    = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .screenBottom = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .align        = CSS::Align::CENTER,
  };

  page->setComputeMode(Page::ComputeMode::DISPLAY);

  page->start(fmt);

  page->clearRegion(Dim(width, HEIGHT), Pos(fmt.screenLeft, fmt.screenTop));

  page->putRounded(Dim(width - 4, HEIGHT - 4), Pos(fmt.screenLeft + 2, fmt.screenTop + 2));

  FontPtr &iconFont = appFonts.getFont(0);

  Glyph *glyph = iconFont->getGlyph(iconChar[msgType], 24);

  if (glyph != nullptr) {
    page->putCharAt(
        iconChar[msgType],
        Pos(fmt.screenLeft + 50 - (glyph->dim.width >> 1), (Screen::getHeight() >> 1) + 20), fmt);
  }

  fmt.fontIndex = SYSTEM_REGULAR_FONT_INDEX;
  fmt.fontSize  = 10;

  // Title

  page->setLimits(fmt);
  page->newParagraph(fmt);
  page->addText(title, fmt);
  page->endParagraph(fmt);

  // Message

  fmt.align      = CSS::Align::LEFT;
  fmt.marginTop  = 80;
  fmt.marginLeft = 100;

  page->setLimits(fmt);
  page->newParagraph(fmt);
  page->addText(buff.get(), fmt);
  page->endParagraph(fmt);

  // Press a Key option

  if (pressAKey) {
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    if (msgType != MsgType::CONFIRM) {
      fmt.align      = CSS::Align::CENTER;
      fmt.fontSize   = 9;
      fmt.marginLeft = 10;
      fmt.marginTop  = 200;

      page->setLimits(fmt);
      page->newParagraph(fmt);
      page->addText("[Please TAP the screen]", fmt);
      page->endParagraph(fmt);
    } else {
      FontPtr &font = appFonts.getFont(1);

      Dim dim, okDim;
      font->getASCIISize("CANCEL", &dim, 10);
      font->getASCIISize("OK", &okDim, 10);
      confirmData->buttonsDim = Dim(dim.width + 20, dim.height + 20);
      confirmData->okPos =
          Pos((Screen::getWidth() >> 1) - 20 - confirmData->buttonsDim.width, fmt.screenTop + 220);
      confirmData->cancelPos = Pos((Screen::getWidth() >> 1) + 20, fmt.screenTop + 220);

      page->putRounded(confirmData->buttonsDim, confirmData->okPos);
      page->putRounded(Dim(confirmData->buttonsDim.width + 2, confirmData->buttonsDim.height + 2),
                       Pos(confirmData->okPos.x - 1, confirmData->okPos.y - 1));
      page->putRounded(Dim(confirmData->buttonsDim.width + 4, confirmData->buttonsDim.height + 4),
                       Pos(confirmData->okPos.x - 2, confirmData->okPos.y - 2));

      page->putStrAt(
          "OK",
          Pos(confirmData->okPos.x + (confirmData->buttonsDim.width >> 1) - (okDim.width >> 1),
              confirmData->okPos.y + (confirmData->buttonsDim.height >> 1) + (okDim.height >> 1)),
          fmt);

      page->putRounded(confirmData->buttonsDim, confirmData->cancelPos);
      page->putRounded(Dim(confirmData->buttonsDim.width + 2, confirmData->buttonsDim.height + 2),
                       Pos(confirmData->cancelPos.x - 1, confirmData->cancelPos.y - 1));
      page->putRounded(Dim(confirmData->buttonsDim.width + 4, confirmData->buttonsDim.height + 4),
                       Pos(confirmData->cancelPos.x - 2, confirmData->cancelPos.y - 2));

      page->putStrAt(
          "CANCEL",
          Pos(confirmData->cancelPos.x + (confirmData->buttonsDim.width >> 1) - (dim.width >> 1),
              confirmData->cancelPos.y + (confirmData->buttonsDim.height >> 1) + (dim.height >> 1)),
          fmt);
    }
#else
    fmt.align      = CSS::Align::CENTER;
    fmt.fontSize   = 9;
    fmt.marginLeft = 10;
    fmt.marginTop  = 200;

    page->setLimits(fmt);
    page->newParagraph(fmt);
    page->addText(msgType == MsgType::CONFIRM ? "[Press SELECT to Confirm]" : "[Press any key]",
                  fmt);
    page->endParagraph(fmt);
#endif
  }

  page->paint(clearScreen, !clearScreen, true);

  return confirmData;
}

/**
 * @brief Evaluate confirmation input and update dialog state.
 *
 * Returns completion status together with the same confirmation payload,
 * updated with the chosen result when a valid action is detected.
 */
auto MsgViewer::confirm(const EventMgr::Event &event, ConfirmDataPtr confirmData)
    -> std::pair<bool, ConfirmDataPtr> {
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

  if (event.kind == EventMgr::EventKind::TAP) {
    if ((event.x >= confirmData->okPos.x) &&
        (event.x <= (confirmData->okPos.x + confirmData->buttonsDim.width)) &&
        (event.y >= confirmData->okPos.y) &&
        (event.y <= (confirmData->okPos.y + confirmData->buttonsDim.height))) {
      confirmData->ok = true;
      return {true, std::move(confirmData)};
    } else if ((event.x >= confirmData->cancelPos.x) &&
               (event.x <= (confirmData->cancelPos.x + confirmData->buttonsDim.width)) &&
               (event.y >= confirmData->cancelPos.y) &&
               (event.y <= (confirmData->cancelPos.y + confirmData->buttonsDim.height))) {
      confirmData->ok = false;
      return {true, std::move(confirmData)};
    }
  }
  return {false, std::move(confirmData)};
#else
  confirmData->ok = event.kind == EventMgr::EventKind::SELECT;
  return {true, std::move(confirmData)};
#endif
}

#if 1
auto MsgViewer::showProgress(const char *title, ...) -> std::pair<PagePtr, ProgressDataPtr> {

  auto page         = Page::Make(appFonts);
  auto progressData = ProgressData::Make();

  uint16_t width = Screen::getWidth() - 60;

  Page::Format fmt = {
      .indent       = 0,
      .marginLeft   = 10,
      .marginRight  = 10,
      .marginTop    = 30, // 70,
      .marginBottom = 10,
      .screenLeft   = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenRight  = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenTop    = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .screenBottom = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .align        = CSS::Align::CENTER,
  };

  char buff[200];

  va_list args;
  va_start(args, title);
  vsnprintf(buff, 200, title, args);
  va_end(args);

  page->start(fmt);

  page->clearRegion(Dim(width, HEIGHT), Pos(fmt.screenLeft, fmt.screenTop));

  page->putRounded(Dim(width - 4, HEIGHT - 4), Pos(fmt.screenLeft + 2, fmt.screenTop + 2));

  // Title

  page->setLimits(fmt);
  page->newParagraph(fmt);
  std::string buffer = buff;
  page->addText(buffer, fmt);
  page->endParagraph(fmt);

  // Progress zone

  progressData->dim = Dim(width - 100, Screen::getHeight() / 18);
  progressData->pos = Pos(((Screen::getWidth() - width) >> 1) + 50, (Screen::getHeight() >> 1));

  progressData->previousWidth = 0;

  page->putHighlight(progressData->dim, progressData->pos);

  progressData->dim.width -= 10;
  progressData->dim.height -= 10;
  progressData->pos.x += 5;
  progressData->pos.y += 5;

  page->paint(false);

  return {std::move(page), std::move(progressData)};
}

auto MsgViewer::updateProgress(PagePtr page, ProgressDataPtr progressData, uint16_t percent,
                               const char *fmtStr, ...) -> std::pair<PagePtr, ProgressDataPtr> {

  if (percent > 100) return {std::move(page), std::move(progressData)};

  uint16_t width = Screen::getWidth() - 60;

  Page::Format fmt = {
      .marginTop    = 30, // 70,
      .screenLeft   = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenRight  = static_cast<uint16_t>((Screen::getWidth() - width) >> 1),
      .screenTop    = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .screenBottom = static_cast<uint16_t>((Screen::getHeight() - HEIGHT) >> 1),
      .align        = CSS::Align::CENTER,
  };

  char buff[80];

  va_list args;
  va_start(args, fmtStr);
  vsnprintf(buff, 80, fmtStr, args);
  va_end(args);

  page->start(fmt);

  uint16_t progressWidth =
      percent >= 100
          ? progressData->dim.width
          : static_cast<uint16_t>(static_cast<uint32_t>(progressData->dim.width * percent) / 100);

  if (progressWidth > progressData->previousWidth) {
    progressWidth -= progressData->previousWidth;
    page->setRegion(Dim{progressWidth, progressData->dim.height},
                    Pos{static_cast<uint16_t>(progressData->pos.x + progressData->previousWidth),
                        static_cast<uint16_t>(progressData->pos.y)});

    page->clearRegion(
        Dim(width - 20, 30),
        Pos(fmt.screenLeft + 10, progressData->pos.y + (progressData->dim.height << 1) - 25));

    page->putStrAt(
        buff,
        Pos{static_cast<uint16_t>(progressData->pos.x + (progressData->dim.width >> 1)),
            static_cast<uint16_t>(progressData->pos.y + (progressData->dim.height << 1))},
        fmt);

    progressData->previousWidth += progressWidth;

    page->paint(false, false, true);
  }

  return {std::move(page), std::move(progressData)};
}
#endif

/**
 * @brief Last-resort OOM handler.
 *
 * Clears persistent state on embedded builds, forces a full refresh so the
 * alert is visible, displays a restart instruction, and finally deep-sleeps
 * (embedded) or exits (Linux).
 */
auto MsgViewer::outOfMemory(const char *reason) -> void {
#if EPUB_INKPLATE_BUILD
  nvs_handle_t nvsHandle;
  esp_err_t err;

  if ((err = nvs_open("EPUB-InkPlate", NVS_READWRITE, &nvsHandle)) == ESP_OK) {
    if (nvs_erase_all(nvsHandle) == ESP_OK) {
      nvs_commit(nvsHandle);
    }
    nvs_close(nvsHandle);
  }
#endif

  screen.forceFullUpdate();

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  #define MSG "Press the WakeUp Button to restart."
  #define INT_PIN TouchScreen::INTERRUPT_PIN
  #define LEVEL 0
#else
  #define MSG "Press a key to restart."
  #if EXTENDED_CASE
    #define INT_PIN PressKeys::INTERRUPT_PIN
  #else
    #define INT_PIN TouchKeys::INTERRUPT_PIN
  #endif
  #define LEVEL 1
#endif

  show(ALERT, true, true, "OUT OF MEMORY!!",
       "It's a bit sad that the device is now out of "
       "memory to continue. The reason: %s. "
       "The device is now entering into Deep Sleep. " MSG,
       reason);

#undef MSG

#if EPUB_INKPLATE_BUILD
  inkplate_platform.deep_sleep(INT_PIN, LEVEL); // Never return
#else
  exit(0);
#endif
}