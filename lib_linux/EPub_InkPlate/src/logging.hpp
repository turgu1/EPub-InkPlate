// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#if __LOGGING__
  void log(const char level, const char *tag, const char *fmt, ...);
#else
  extern void log(const char level, const char *tag, const char *fmt, ...);
#endif

// clang-format off
#if DEBUGGING
  #define LOG_I(fmt, ...) { log('I', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_D(fmt, ...) { log('D', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_W(fmt, ...) { log('W', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_V(fmt, ...) { log('V', TAG, fmt, ##__VA_ARGS__); }
#else
  #define LOG_I(fmt, ...) { log('I', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_D(fmt, ...)
  #define LOG_W(fmt, ...) { log('W', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_V(fmt, ...)
#endif

#define LOG_E(fmt, ...) { log('E', TAG, fmt, ##__VA_ARGS__); }
// clang-format on