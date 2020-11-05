#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

#include <cstdio>

#if __LOGGING__
  void log(char level, char * tag, char * fmt, ...);
#else
  extern void log(char level, char * tag, char * fmt, ...);
#endif

#define LOG_I(tag, fmt, ...) { log('I', tag, fmt, ##__VA_ARGS__); }
#define LOG_D(tag, fmt, ...) { log('D', tag, fmt, ##__VA_ARGS__); }
#define LOG_E(tag, fmt, ...) { log('E', tag, fmt, ##__VA_ARGS__); }

#endif