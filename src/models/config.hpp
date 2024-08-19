// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "config_template.hpp"

enum class ConfigIdent { 
  VERSION, SSID, PWD, PORT, BATTERY, FONT_SIZE, TIMEOUT, ORIENTATION, 
  USE_FONTS_IN_BOOKS, DEFAULT_FONT, SHOW_IMAGES, PIXEL_RESOLUTION, SHOW_HEAP, 
  SHOW_TITLE, FRONT_LIGHT, DIR_VIEW,
  #if DATE_TIME_RTC
    SHOW_RTC,
    NTP_SERVER,
    TIME_ZONE,
  #endif
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
    CALIB_A, CALIB_B, CALIB_C, CALIB_D, CALIB_E, CALIB_F, CALIB_DIVIDER
  #endif
};

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
  #if DATE_TIME_RTC
    typedef ConfigBase<ConfigIdent, 26> Config;
  #else
    typedef ConfigBase<ConfigIdent, 23> Config;
  #endif
#else
  #if DATE_TIME_RTC
    typedef ConfigBase<ConfigIdent, 19> Config;
  #else
    typedef ConfigBase<ConfigIdent, 16> Config;
  #endif
#endif

#if __CONFIG__
  #include <string>

  static const std::string CONFIG_FILE = MAIN_FOLDER "/config.txt";

  static int8_t   version;
  static char     ssid[32];
  static char     pwd[32];
  static int32_t  port;
  static int8_t   battery;
  static int8_t   orientation;
  static int8_t   timeout;
  static int8_t   font_size;
  static int8_t   use_fonts_in_books;
  static int8_t   default_font;
  static int8_t   show_images;
  static int8_t   resolution;
  static int8_t   show_heap;
  static int8_t   show_title;
  static int8_t   front_light;
  static int8_t   dir_view;

  #if DATE_TIME_RTC
    static int8_t show_rtc;
    static char   ntp_server[32];
    static char   time_zone[32];
  #endif
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
    static int64_t calib_a, calib_b, calib_c, calib_d, calib_e, calib_f, calib_divider;
    static const int64_t default_calib             =  0;
  #endif

  static const int32_t  default_port               = 80;
  static const int8_t   default_battery            =  2;  // 0 = NONE, 1 = PERCENT, 2 = VOLTAGE, 3 = ICON
  static const int8_t   default_orientation        =  1;  // 0 = LEFT, 1 = RIGHT, 2 = BOTTOM
  static const int8_t   default_font_size          = 12;  // 8, 10, 12, 15 pts
  static const int8_t   default_timeout            = 15;  // 5, 15, 30 minutes
  static const int8_t   default_show_images        =  0;  // 0 = NO, 1 = YES
  static const int8_t   default_use_fonts_in_books =  1;  // 0 = NO, 1 = YES
  static const int8_t   default_default_font       =  1;  // 0 = CALADEA, 1 = CRIMSON, 2 = RED HAT, 3 = ASAP
  static const int8_t   default_resolution         =  0;  // 0 = 1bit, 1 = 3bits
  static const int8_t   default_show_heap          =  0;  // 0 = NO, 1 = YES
  static const int8_t   default_show_title         =  1;
  static const int8_t   default_front_light        = 15;  // value between 0 and 63
  static const int8_t   default_dir_view           =  0;  // 0 = linear view, 1 = matrix view
  static const int8_t   the_version                =  1;

  static const int8_t   default_show_rtc           =  0;

  // static Config::CfgType conf = {{

  template <>
  Config::CfgType Config::cfg = {{
    { Config::Ident::VERSION,            Config::EntryType::BYTE,   "version",            &version,            &the_version,                0 },
    { Config::Ident::SSID,               Config::EntryType::STRING, "wifi_ssid",          ssid,                "NONE",                     32 },
    { Config::Ident::PWD,                Config::EntryType::STRING, "wifi_pwd",           pwd,                 "NONE",                     32 },
    { Config::Ident::PORT,               Config::EntryType::INT,    "http_port",          &port,               &default_port,               0 },
    { Config::Ident::BATTERY,            Config::EntryType::BYTE,   "battery",            &battery,            &default_battery,            0 },
    { Config::Ident::TIMEOUT,            Config::EntryType::BYTE,   "timeout",            &timeout,            &default_timeout,            0 },
    { Config::Ident::FONT_SIZE,          Config::EntryType::BYTE,   "font_size",          &font_size,          &default_font_size,          0 },
    { Config::Ident::DEFAULT_FONT,       Config::EntryType::BYTE,   "default_font",       &default_font,       &default_default_font,       0 },
    { Config::Ident::USE_FONTS_IN_BOOKS, Config::EntryType::BYTE,   "use_fonts_in_books", &use_fonts_in_books, &default_use_fonts_in_books, 0 },
    { Config::Ident::SHOW_IMAGES,        Config::EntryType::BYTE,   "show_images",        &show_images,        &default_show_images,        0 },
    { Config::Ident::ORIENTATION,        Config::EntryType::BYTE,   "orientation",        &orientation,        &default_orientation,        0 },
    { Config::Ident::PIXEL_RESOLUTION,   Config::EntryType::BYTE,   "resolution",         &resolution,         &default_resolution,         0 },
    { Config::Ident::SHOW_HEAP,          Config::EntryType::BYTE,   "show_heap",          &show_heap,          &default_show_heap,          0 },
    { Config::Ident::SHOW_TITLE,         Config::EntryType::BYTE,   "show_title",         &show_title,         &default_show_title,         0 },
    { Config::Ident::FRONT_LIGHT,        Config::EntryType::BYTE,   "front_light",        &front_light,        &default_front_light,        0 },
    { Config::Ident::DIR_VIEW,           Config::EntryType::BYTE,   "dir_view",           &dir_view,           &default_dir_view,           0 },

    #if DATE_TIME_RTC
    { Config::Ident::SHOW_RTC,           Config::EntryType::BYTE,   "show_rtc",           &show_rtc,           &default_show_rtc,           0 },
    { Config::Ident::NTP_SERVER,         Config::EntryType::STRING, "ntp_server",         ntp_server,          "pool.ntp.org",             32 },
    { Config::Ident::TIME_ZONE,          Config::EntryType::STRING, "tz",                 time_zone,           "",                         32 },
    #endif

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2
    { Config::Ident::CALIB_A,            Config::EntryType::INT64,   "calib_a",           &calib_a,            &default_calib,              0 },
    { Config::Ident::CALIB_B,            Config::EntryType::INT64,   "calib_b",           &calib_b,            &default_calib,              0 },
    { Config::Ident::CALIB_C,            Config::EntryType::INT64,   "calib_c",           &calib_c,            &default_calib,              0 },
    { Config::Ident::CALIB_D,            Config::EntryType::INT64,   "calib_d",           &calib_d,            &default_calib,              0 },
    { Config::Ident::CALIB_E,            Config::EntryType::INT64,   "calib_e",           &calib_e,            &default_calib,              0 },
    { Config::Ident::CALIB_F,            Config::EntryType::INT64,   "calib_f",           &calib_f,            &default_calib,              0 },
    { Config::Ident::CALIB_DIVIDER,      Config::EntryType::INT64,   "calib_divider",     &calib_divider,      &default_calib,              0 },
    #endif
  }};

  // Config config(conf, CONFIG_FILE);
  Config config(CONFIG_FILE, true);
#else
  extern Config config;
#endif
