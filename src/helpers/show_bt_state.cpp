#if BLE_KEYPAD

  #define __SHOW_BT_STATE__ 1
  #include "helpers/show_bt_state.hpp"

  #include "fonts.hpp"
  #include "screen.hpp"

  auto ShowBtState::setup() -> void {
  }

  extern auto appIsInitialized() -> bool;

  // Used by the event mgr for BLE to show the Bluetooth icon when the BLE device was paired or not.
  void ShowBtState::show(bool paired, bool updateScr) {

    LOG_I("updateScreen()... paired = {}", paired ? "Yes" : "No");

    if (!appIsInitialized()) { return; }

    FontPtr &iconFont = appFonts.getFont(0);

    Glyph *  glyph    = iconFont->getGlyph(BT_ICON, BT_ICON_SIZE);

    if ((glyph == nullptr) || (glyph->buffer == nullptr) || (glyph->dim.width == 0) ||
        (glyph->dim.height == 0)) {
      return;
    }


    Pos pos = { 10, (uint16_t)(Screen::getHeight() - glyph->dim.height + iconFont->getDescenderHeight(BT_ICON_SIZE) - 2) };

    LOG_W("Glyph Dim: [{}, {}], Pos: [{}, {}]", glyph->dim.width, glyph->dim.height, pos.x, pos.y);

    if (paired) {
      screen.drawGlyph(glyph->buffer, glyph->dim, pos, glyph->pitch);
    } else {
      screen.colorizeRegion(glyph->dim, pos, Screen::Color::WHITE);
    }

    if (updateScr) { screen.update(); }
  }

  // Used by the ScreenBottom class
  auto ShowBtState::show(PagePtr &page, bool paired) -> void {

    LOG_I("show()... paired = {}", paired ? "Yes" : "No");

    FontPtr &iconFont = appFonts.getFont(0);
    Glyph *  glyph    = iconFont->getGlyph(BT_ICON, BT_ICON_SIZE);

    if ((glyph == nullptr) || (glyph->buffer == nullptr) || (glyph->dim.width == 0) ||
        (glyph->dim.height == 0)) {
      return;
    }

    Pos pos = { 10, (uint16_t)(Screen::getHeight() + iconFont->getDescenderHeight(BT_ICON_SIZE) - 2) };

    LOG_W("Glyph Dim: [{}, {}], Pos: [{}, {}]", glyph->dim.width, glyph->dim.height, pos.x, pos.y);

    Page::Format fmt = {
      .fontIndex = 0,
      .fontSize  = BT_ICON_SIZE,
      .align     = HAlign::LEFT,
    };

    if (paired) {
      page->putCharAt(BT_ICON, pos, fmt);
    } else {
      page->clearRegion(glyph->dim, pos);
    }
  }
#endif