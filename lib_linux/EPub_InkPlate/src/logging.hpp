// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <format>

#if __LOGGING__
  void log(const char level, const char *tag, const char *fmt, ...);
#else
  extern void log(const char level, const char *tag, const char *fmt, ...);
#endif

#if DEBUGGING
  #define LOG_I(fmt, ...) { log('I', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
  #define LOG_D(fmt, ...) { log('D', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
  #define LOG_W(fmt, ...) { log('W', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
  #define LOG_V(fmt, ...) { log('V', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
#else
  #define LOG_I(fmt, ...) { log('I', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
  #define LOG_D(fmt, ...)
  #define LOG_W(fmt, ...) { log('W', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }
  #define LOG_V(fmt, ...)
#endif

#define LOG_E(fmt, ...) { log('E', TAG, std::format(fmt, ## __VA_ARGS__).c_str()); }