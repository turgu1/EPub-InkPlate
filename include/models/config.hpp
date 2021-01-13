#pragma once

#include "global.hpp"
#include "config_template.hpp"

enum class ConfigIdent { 
  VERSION, SSID, PWD, PORT, BATTERY, FONT_SIZE, TIMEOUT, ORIENTATION, 
  USE_FONTS_IN_BOOKS, DEFAULT_FONT, SHOW_IMAGES, PIXEL_RESOLUTION, SHOW_HEAP 
};

typedef ConfigBase<ConfigIdent, 13> Config;

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

  static int32_t  default_port               = 80;
  static int8_t   default_battery            =  2;  // 0 = NONE, 1 = PERCENT, 2 = VOLTAGE, 3 = ICON
  static int8_t   default_orientation        =  1;  // 0 = LEFT, 1 = RIGHT, 2 = BOTTOM
  static int8_t   default_font_size          = 12;  // 8, 10, 12, 15 pts
  static int8_t   default_timeout            = 15;  // 5, 15, 30 minutes
  static int8_t   default_show_images        =  0;  // 0 = NO, 1 = YES
  static int8_t   default_use_fonts_in_books =  1;  // 0 = NO, 1 = YES
  static int8_t   default_default_font       =  1;  // 0 = CALADEA, 1 = CRIMSON, 2 = RED HAT, 3 = ASAP
  static int8_t   default_resolution         =  0;  // 0 = 1bit, 1 = 3bits
  static int8_t   default_show_heap          =  0;  // 0 = NO, 1 = YES
  static int8_t   the_version                =  1;

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
    { Config::Ident::SHOW_HEAP,          Config::EntryType::BYTE,   "show_heap",          &show_heap,          &default_show_heap,          0 }
  }};

  // Config config(conf, CONFIG_FILE);
  Config config(CONFIG_FILE, true);
#else
  extern Config config;
#endif
