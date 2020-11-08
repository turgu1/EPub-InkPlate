#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

#include <cstdio>

#if __LOGGING__
  void log(const char level, const char * tag, const char * fmt, ...);
#else
  extern void log(const char level, const char * tag, const char * fmt, ...);
#endif

#define LOG_I(fmt, ...) { log('I', TAG, fmt, ##__VA_ARGS__); }
#define LOG_D(fmt, ...) { log('D', TAG, fmt, ##__VA_ARGS__); }
#define LOG_E(fmt, ...) { log('E', TAG, fmt, ##__VA_ARGS__); }

#endif