// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/event_mgr.hpp"

class OptionController
{
  private:
    static constexpr char const * TAG = "OptionController";

    bool main_form_is_shown;
    bool font_form_is_shown;
    bool books_refresh_needed;

    #if INKPLATE_6PLUS
      bool calibration_is_shown;
    #endif

    bool wait_for_key_after_wifi;

  public:
    OptionController() : main_form_is_shown(false), 
                         font_form_is_shown(false),
                         books_refresh_needed(false), 
                         #if INKPLATE_6PLUS
                           calibration_is_shown(false),
                         #endif
                         wait_for_key_after_wifi(false) { };
                         
    void    input_event(const EventMgr::Event & event);
    void          enter();
    void          leave(bool going_to_deep_sleep = false);
    void set_font_count(uint8_t count);
     
    inline void set_main_form_is_shown() { main_form_is_shown = true; }
    inline void set_font_form_is_shown() { font_form_is_shown = true; }

    #if INKPLATE_6PLUS
      inline void set_calibration_is_shown() { calibration_is_shown = true; }
    #endif

    inline void set_wait_for_key_after_wifi() { 
      wait_for_key_after_wifi = true; 
      main_form_is_shown      = false;
      font_form_is_shown      = false;
      #if INKPLATE_6PLUS
        calibration_is_shown  = false;
      #endif
    }
};

#if __OPTION_CONTROLLER__
  OptionController option_controller;
#else
  extern OptionController option_controller;
#endif
