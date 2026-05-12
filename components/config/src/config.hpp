// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "config_template.hpp"

// clang-format off
enum class ConfigIdent {
    VERSION,     SSID,               PWD,          PORT,          BATTERY,     FONT_SIZE,        TIMEOUT,
    ORIENTATION, USE_FONTS_IN_BOOKS, DEFAULT_FONT, SHOW_PICTURES, LINE_HEIGHT, PIXEL_RESOLUTION, SHOW_HEAP,
    SHOW_TITLE,  FRONT_LIGHT,        DIR_VIEW,     COVER_SIZE,    DNS_NAME,    AP_SSID,          AP_PWD,
  #if DATE_TIME_RTC
    SHOW_RTC,    NTP_SERVER,         TIME_ZONE,
  #endif
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    CALIB_A, CALIB_B, CALIB_C, CALIB_D, CALIB_E, CALIB_F, CALIB_DIVIDER,
  #endif
  FONTS_DB, BATTERY_TRIM
};
// clang-format on

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
  #if DATE_TIME_RTC
    using Config = ConfigBase<ConfigIdent, 33>;
  #else
    using Config = ConfigBase<ConfigIdent, 30>;
  #endif
#else
  #if DATE_TIME_RTC
    using Config = ConfigBase<ConfigIdent, 26>;
  #else
    using Config = ConfigBase<ConfigIdent, 23>;
  #endif
#endif

#if __CONFIG__
  #include <string>

  static const std::string CONFIG_FILE = MAIN_FOLDER "/config.txt";

  static int8_t version;
  static char ssid[32];
  static char pwd[32];
  static char dnsName[32];
  static char apSsid[32];
  static char apPwd[32];
  static int32_t port;
  static int8_t battery;
  static int8_t orientation;
  static int8_t timeout;
  static int8_t fontSize;
  static int8_t useFontsInBooks;
  static int8_t defaultFont;
  static int8_t showPictures;
  static int8_t resolution;
  static int8_t showHeap;
  static int8_t showTitle;
  static int8_t frontLight;
  static int8_t dirView;
  static int8_t coverSize;
  static double batteryTrim;
  static int8_t lineHeight;

  #if DATE_TIME_RTC
    static int8_t showRtc;
    static char ntpServer[32];
    static char timeZone[32];
  #endif
  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    static int64_t calibA, calibB, calibC, calibD, calibE, calibF, calibDivider;
    static const int64_t defaultCalib = 0;
  #endif

  static FontsDB fontsDB;

  // clang-format off
  static const int32_t defaultPort            = 80;
  static const int8_t  defaultBattery         =  2; // 0 = NONE, 1 = PERCENT, 2 = VOLTAGE, 3 = ICON
  static const int8_t  defaultOrientation     =  1; // 0 = LEFT, 1 = RIGHT, 2 = BOTTOM
  static const int8_t  defaultFontSize        = 12; // 8, 10, 12, 15 pts
  static const int8_t  defaultTimeout         = 15; // 5, 15, 30 minutes
  static const int8_t  defaultShowPictures    =  0; // 0 = NO, 1 = YES
  static const int8_t  defaultUseFontsInBooks =  1; // 0 = NO, 1 = YES
  static const int8_t  defaultDefaultFont     =  1; // index from the user fonts list
  static const int8_t  defaultResolution      =  0; // 0 = 1bit, 1 = 3bits
  static const int8_t  defaultShowHeap        =  0; // 0 = NO, 1 = YES
  static const int8_t  defaultShowTitle       =  1;
  static const int8_t  defaultFrontLight      = 15; // value between 0 and 63
  static const int8_t  defaultDirView         =  0; // 0 = linear view, 1 = matrix view
  static const int8_t  defaultCoverSize       =  0; // 0 = SMALL, 1 = LARGE
  static const int8_t  theVersion             =  1;
  static const int8_t  defaultShowRtc         =  0;
  static const double  defaultBatteryTrim     =  4.086 / 4.26;
  static const int8_t  defaultLineHeight      =  1; // 0 = TIGHT, 1 = MEDIUM, 2 = LARGE

  template <>
  Config::CfgType Config::cfg = {{
      {Config::Ident::VERSION,            Config::EntryType::BYTE,   "version",       &version,               &theVersion,             0},
      {Config::Ident::SSID,               Config::EntryType::STRING, "wifi_ssid",     ssid,                   "NONE",                 32},
      {Config::Ident::PWD,                Config::EntryType::STRING, "wifi_pwd",      pwd,                    "NONE",                 32},
      {Config::Ident::DNS_NAME,           Config::EntryType::STRING, "dns_name",      dnsName,                "inkplate",             32},
      {Config::Ident::AP_SSID,            Config::EntryType::STRING, "ap_wifi_ssid",  apSsid,                 "inkplate",             32},
      {Config::Ident::AP_PWD,             Config::EntryType::STRING, "ap_wifi_pwd",   apPwd,                  "qwerty1234",           32},
      {Config::Ident::PORT,               Config::EntryType::INT,    "http_port",     &port,                  &defaultPort,            0},
      {Config::Ident::BATTERY,            Config::EntryType::BYTE,   "battery",       &battery,               &defaultBattery,         0},
      {Config::Ident::TIMEOUT,            Config::EntryType::BYTE,   "timeout",       &timeout,               &defaultTimeout,         0},
      {Config::Ident::FONT_SIZE,          Config::EntryType::BYTE,   "font_size",     &fontSize,              &defaultFontSize,        0},
      {Config::Ident::DEFAULT_FONT,       Config::EntryType::BYTE,   "default_font",  &defaultFont,           &defaultDefaultFont,     0},
      {Config::Ident::USE_FONTS_IN_BOOKS, Config::EntryType::BYTE,   "use_fonts_in_books", &useFontsInBooks, &defaultUseFontsInBooks,  0},
      {Config::Ident::SHOW_PICTURES,      Config::EntryType::BYTE,   "show_images",   &showPictures,          &defaultShowPictures,    0},
      {Config::Ident::ORIENTATION,        Config::EntryType::BYTE,   "orientation",   &orientation,           &defaultOrientation,     0},
      {Config::Ident::PIXEL_RESOLUTION,   Config::EntryType::BYTE,   "resolution",    &resolution,            &defaultResolution,      0},
      {Config::Ident::SHOW_HEAP,          Config::EntryType::BYTE,   "show_heap",     &showHeap,              &defaultShowHeap,        0},
      {Config::Ident::SHOW_TITLE,         Config::EntryType::BYTE,   "show_title",    &showTitle,             &defaultShowTitle,       0},
      {Config::Ident::FRONT_LIGHT,        Config::EntryType::BYTE,   "front_light",   &frontLight,            &defaultFrontLight,      0},
      {Config::Ident::DIR_VIEW,           Config::EntryType::BYTE,   "dir_view",      &dirView,               &defaultDirView,         0},
      {Config::Ident::COVER_SIZE,         Config::EntryType::BYTE,   "cover_size",    &coverSize,             &defaultCoverSize,       0},

  #if DATE_TIME_RTC
      {Config::Ident::SHOW_RTC,           Config::EntryType::BYTE,   "show_rtc",       &showRtc,              &defaultShowRtc,         0},
      {Config::Ident::NTP_SERVER,         Config::EntryType::STRING, "ntp_server",     ntpServer,             "pool.ntp.org",         32},
      {Config::Ident::TIME_ZONE,          Config::EntryType::STRING, "tz",             timeZone,              "",                     32},
  #endif

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      {Config::Ident::CALIB_A,            Config::EntryType::INT64,  "calib_a",        &calibA,               &defaultCalib,           0},
      {Config::Ident::CALIB_B,            Config::EntryType::INT64,  "calib_b",        &calibB,               &defaultCalib,           0},
      {Config::Ident::CALIB_C,            Config::EntryType::INT64,  "calib_c",        &calibC,               &defaultCalib,           0},
      {Config::Ident::CALIB_D,            Config::EntryType::INT64,  "calib_d",        &calibD,               &defaultCalib,           0},
      {Config::Ident::CALIB_E,            Config::EntryType::INT64,  "calib_e",        &calibE,               &defaultCalib,           0},
      {Config::Ident::CALIB_F,            Config::EntryType::INT64,  "calib_f",        &calibF,               &defaultCalib,           0},
      {Config::Ident::CALIB_DIVIDER,      Config::EntryType::INT64,  "calib_divider",  &calibDivider,         &defaultCalib,           0},
  #endif
      {Config::Ident::FONTS_DB,           Config::EntryType::FONTS_DB, "",             &fontsDB,              &defaultFont,            0},
      {Config::Ident::BATTERY_TRIM,       Config::EntryType::DOUBLE,   "battery_trim", &batteryTrim,          &defaultBatteryTrim,     0},
      {Config::Ident::LINE_HEIGHT,        Config::EntryType::BYTE,     "line_height",  &lineHeight,           &defaultLineHeight,      0},
  }};

  // clang-format on

  // Config config(conf, CONFIG_FILE);
  Config config(HimemString(CONFIG_FILE), true);
#else
  extern Config config;
#endif
