// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#if __LOGGING__
  void log(const char level, const char * tag, const char * fmt, ...);
#else
  extern void log(const char level, const char * tag, const char * fmt, ...);
#endif

#if DEBUGGING
  #define LOG_I(fmt, ...) { log('I', TAG, fmt, ##__VA_ARGS__); }
  #define LOG_D(fmt, ...) { log('D', TAG, fmt, ##__VA_ARGS__); }
#else
  #define LOG_I(fmt, ...)
  #define LOG_D(fmt, ...)
#endif

#define LOG_E(fmt, ...) { log('E', TAG, fmt, ##__VA_ARGS__); }
