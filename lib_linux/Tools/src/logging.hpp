#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

#include <cstdio>

#if __LOGGING__
  void log(const char level, const char * tag, const char * fmt, ...);
#else
  extern void log(const char level, const char * tag, const char * fmt, ...);
#endif

#define LOG_I(tag, fmt, ...) { log('I', tag, fmt, ##__VA_ARGS__); }
#define LOG_D(tag, fmt, ...) { log('D', tag, fmt, ##__VA_ARGS__); }
#define LOG_E(tag, fmt, ...) { log('E', tag, fmt, ##__VA_ARGS__); }

#endif