#pragma once

#if BLE_KEYPAD

  #include "ble_keypad.hpp"
  #include "viewers/page.hpp"

  class ShowBtState {
    private:
      static const constexpr char * TAG = "ShowBtState";

      static constexpr char32_t BT_ICON = 'V';
      static constexpr uint16_t BT_ICON_SIZE = 10;

    public:
      ShowBtState() = default;
      ~ShowBtState() = default;

      auto setup() -> void;
      void show(bool paired, bool updateScr = false);
      void show(PagePtr &page, bool paired);
  };

  #if __SHOW_BT_STATE__
    ShowBtState showBtState;
  #else
    extern ShowBtState showBtState;
  #endif

#endif
