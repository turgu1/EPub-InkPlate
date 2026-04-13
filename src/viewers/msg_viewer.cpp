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

auto MsgViewer::show(MsgType msg_type, bool press_a_key, bool clear_screen, const char *title,
                     const char *fmt_str, ...) -> ConfirmDataPtr {
  himem_unique_ptr<char[]> buff = make_unique_himem<char[]>(BUFFER_SIZE);
  auto page                     = Page::Make();
  auto confirm_data             = ConfirmData::Make();

  uint16_t width = Screen::get_width() - 60;

  if (page->get_compute_mode() == Page::ComputeMode::LOCATION)
    return nullptr; // Cannot be used durint location computation

  va_list args;
  va_start(args, fmt_str);
  vsnprintf(buff.get(), BUFFER_SIZE, fmt_str, args);
  va_end(args);

  Page::Format fmt = {
      .font_index    = 0,
      .font_size     = 24,
      .margin_left   = 10,
      .margin_right  = 10,
      .margin_top    = 30,
      .margin_bottom = 10,
      .screen_left   = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
      .screen_right  = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
      .screen_top    = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
      .screen_bottom = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
      .align         = CSS::Align::CENTER,
  };

  page->set_compute_mode(Page::ComputeMode::DISPLAY);

  page->start(fmt);

  page->clear_region(Dim(width, HEIGHT), Pos(fmt.screen_left, fmt.screen_top));

  page->put_rounded(Dim(width - 4, HEIGHT - 4), Pos(fmt.screen_left + 2, fmt.screen_top + 2));

  Font *font = fonts.get(0);

  if (font == nullptr) {
    LOG_E("Internal error (Drawings Font not available!");
    return nullptr;
  }

  Glyph *glyph = font->get_glyph(icon_char[msg_type], 24);

  if (glyph != nullptr) {
    page->put_char_at(
        icon_char[msg_type],
        Pos(fmt.screen_left + 50 - (glyph->dim.width >> 1), (Screen::get_height() >> 1) + 20), fmt);
  }

  fmt.font_index = 1;
  fmt.font_size  = 10;

  // Title

  page->set_limits(fmt);
  page->new_paragraph(fmt);
  page->add_text(title, fmt);
  page->end_paragraph(fmt);

  // Message

  fmt.align       = CSS::Align::LEFT;
  fmt.margin_top  = 80;
  fmt.margin_left = 100;

  page->set_limits(fmt);
  page->new_paragraph(fmt);
  page->add_text(buff.get(), fmt);
  page->end_paragraph(fmt);

  // Press a Key option

  if (press_a_key) {
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      if (msg_type != MsgType::CONFIRM) {
        fmt.align       = CSS::Align::CENTER;
        fmt.font_size   = 9;
        fmt.margin_left = 10;
        fmt.margin_top  = 200;

        page->set_limits(fmt);
        page->new_paragraph(fmt);
        page->add_text("[Please TAP the screen]", fmt);
        page->end_paragraph(fmt);
      } else {
        font = fonts.get(1);

        if (font == nullptr) {
          LOG_E("Internal error (Main Font not available!");
          return nullptr;
        }

        Dim dim, ok_dim;
        font->get_size("CANCEL", &dim, 10);
        font->get_size("OK", &ok_dim, 10);
        confirm_data->buttons_dim = Dim(dim.width + 20, dim.height + 20);
        confirm_data->ok_pos =
            Pos((Screen::get_width() >> 1) - 20 - confirm_data->buttons_dim.width,
                fmt.screen_top + 220);
        confirm_data->cancel_pos = Pos((Screen::get_width() >> 1) + 20, fmt.screen_top + 220);

        page->put_rounded(confirm_data->buttons_dim, confirm_data->ok_pos);
        page->put_rounded(
            Dim(confirm_data->buttons_dim.width + 2, confirm_data->buttons_dim.height + 2),
            Pos(confirm_data->ok_pos.x - 1, confirm_data->ok_pos.y - 1));
        page->put_rounded(
            Dim(confirm_data->buttons_dim.width + 4, confirm_data->buttons_dim.height + 4),
            Pos(confirm_data->ok_pos.x - 2, confirm_data->ok_pos.y - 2));

        page->put_str_at("OK",
                         Pos(confirm_data->ok_pos.x + (confirm_data->buttons_dim.width >> 1) -
                                 (ok_dim.width >> 1),
                             confirm_data->ok_pos.y + (confirm_data->buttons_dim.height >> 1) +
                                 (ok_dim.height >> 1)),
                         fmt);

        page->put_rounded(confirm_data->buttons_dim, confirm_data->cancel_pos);
        page->put_rounded(
            Dim(confirm_data->buttons_dim.width + 2, confirm_data->buttons_dim.height + 2),
            Pos(confirm_data->cancel_pos.x - 1, confirm_data->cancel_pos.y - 1));
        page->put_rounded(
            Dim(confirm_data->buttons_dim.width + 4, confirm_data->buttons_dim.height + 4),
            Pos(confirm_data->cancel_pos.x - 2, confirm_data->cancel_pos.y - 2));

        page->put_str_at("CANCEL",
                         Pos(confirm_data->cancel_pos.x + (confirm_data->buttons_dim.width >> 1) -
                                 (dim.width >> 1),
                             confirm_data->cancel_pos.y + (confirm_data->buttons_dim.height >> 1) +
                                 (dim.height >> 1)),
                         fmt);
      }
    #else
      fmt.align       = CSS::Align::CENTER;
      fmt.font_size   = 9;
      fmt.margin_left = 10;
      fmt.margin_top  = 200;

      page->set_limits(fmt);
      page->new_paragraph(fmt);
      page->add_text(msg_type == MsgType::CONFIRM ? "[Press SELECT to Confirm]" : "[Press any key]",
                     fmt);
      page->end_paragraph(fmt);
    #endif
  }

  page->paint(clear_screen, !clear_screen, true);

  return confirm_data;
}

/**
 * @brief Evaluate confirmation input and update dialog state.
 *
 * Returns completion status together with the same confirmation payload,
 * updated with the chosen result when a valid action is detected.
 */
auto MsgViewer::confirm(const EventMgr::Event &event, ConfirmDataPtr confirm_data)
    -> std::pair<bool, ConfirmDataPtr> {
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL

    if (event.kind == EventMgr::EventKind::TAP) {
      if ((event.x >= confirm_data->ok_pos.x) &&
          (event.x <= (confirm_data->ok_pos.x + confirm_data->buttons_dim.width)) &&
          (event.y >= confirm_data->ok_pos.y) &&
          (event.y <= (confirm_data->ok_pos.y + confirm_data->buttons_dim.height))) {
        confirm_data->ok = true;
        return {true, std::move(confirm_data)};
      } else if ((event.x >= confirm_data->cancel_pos.x) &&
                 (event.x <= (confirm_data->cancel_pos.x + confirm_data->buttons_dim.width)) &&
                 (event.y >= confirm_data->cancel_pos.y) &&
                 (event.y <= (confirm_data->cancel_pos.y + confirm_data->buttons_dim.height))) {
        confirm_data->ok = false;
        return {true, std::move(confirm_data)};
      }
    }
    return {false, std::move(confirm_data)};
  #else
    confirm_data->ok = event.kind == EventMgr::EventKind::SELECT;
    return {true, std::move(confirm_data)};
  #endif
}

#if 1
  auto MsgViewer::show_progress(const char *title, ...) -> std::pair<PagePtr, ProgressDataPtr> {

    auto page          = Page::Make();
    auto progress_data = ProgressData::Make();

    uint16_t width = Screen::get_width() - 60;

    Page::Format fmt = {
        .indent        = 0,
        .margin_left   = 10,
        .margin_right  = 10,
        .margin_top    = 30, // 70,
        .margin_bottom = 10,
        .screen_left   = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
        .screen_right  = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
        .screen_top    = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
        .screen_bottom = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
        .align         = CSS::Align::CENTER,
    };

    char buff[80];

    va_list args;
    va_start(args, title);
    vsnprintf(buff, 80, title, args);
    va_end(args);

    page->start(fmt);

    page->clear_region(Dim(width, HEIGHT), Pos(fmt.screen_left, fmt.screen_top));

    page->put_rounded(Dim(width - 4, HEIGHT - 4), Pos(fmt.screen_left + 2, fmt.screen_top + 2));

    // Title

    page->set_limits(fmt);
    page->new_paragraph(fmt);
    std::string buffer = buff;
    page->add_text(buffer, fmt);
    page->end_paragraph(fmt);

    // Progress zone

    progress_data->dim = Dim(width - 100, Screen::get_height() / 18);
    progress_data->pos =
        Pos(((Screen::get_width() - width) >> 1) + 50, (Screen::get_height() >> 1) + 20);

    progress_data->previous_width = 0;

    page->put_highlight(progress_data->dim, progress_data->pos);

    progress_data->dim.width -= 10;
    progress_data->dim.height -= 10;
    progress_data->pos.x += 5;
    progress_data->pos.y += 5;

    page->paint(false);

    return {std::move(page), std::move(progress_data)};
  }

  auto MsgViewer::update_progress(PagePtr page, ProgressDataPtr progress_data, uint16_t percent)
      -> std::pair<PagePtr, ProgressDataPtr> {

    if (percent > 100) return {std::move(page), std::move(progress_data)};

    uint16_t width = Screen::get_width() - 60;

    Page::Format fmt = {
        .margin_top    = 30, // 70,
        .screen_left   = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
        .screen_right  = static_cast<uint16_t>((Screen::get_width() - width) >> 1),
        .screen_top    = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
        .screen_bottom = static_cast<uint16_t>((Screen::get_height() - HEIGHT) >> 1),
        .align         = CSS::Align::CENTER,
    };

    page->start(fmt);

    uint16_t progress_width =
        percent >= 100 ? progress_data->dim.width
                       : static_cast<uint16_t>(
                             static_cast<uint32_t>(progress_data->dim.width * percent) / 100);

    if (progress_width > progress_data->previous_width) {
      progress_width -= progress_data->previous_width;
      page->set_region(
          Dim{progress_width, progress_data->dim.height},
          Pos{static_cast<uint16_t>(progress_data->pos.x + progress_data->previous_width),
              static_cast<uint16_t>(progress_data->pos.y)});

      progress_data->previous_width += progress_width;

      page->paint(false, false, true);
    }

    return {std::move(page), std::move(progress_data)};
  }
#endif

/**
 * @brief Last-resort OOM handler.
 *
 * Clears persistent state on embedded builds, forces a full refresh so the
 * alert is visible, displays a restart instruction, and finally deep-sleeps
 * (embedded) or exits (Linux).
 */
void MsgViewer::out_of_memory(const char *raison) {
  #if EPUB_INKPLATE_BUILD
    nvs_handle_t nvs_handle;
    esp_err_t err;

    if ((err = nvs_open("EPUB-InkPlate", NVS_READWRITE, &nvs_handle)) == ESP_OK) {
      if (nvs_erase_all(nvs_handle) == ESP_OK) {
        nvs_commit(nvs_handle);
      }
      nvs_close(nvs_handle);
    }
  #endif

  screen.force_full_update();

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
       raison);

  #undef MSG

  #if EPUB_INKPLATE_BUILD
    inkplate_platform.deep_sleep(INT_PIN, LEVEL); // Never return
  #else
    exit(0);
  #endif
}