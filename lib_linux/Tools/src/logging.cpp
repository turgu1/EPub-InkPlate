#define __LOGGING__ 1
#include "logging.hpp"

#include <cstdio>
#include <cstdarg>

void log(const char level, const char * tag, const char * fmt, ...) 
{ 
  va_list args;
  va_start(args, fmt);
  
  fprintf(stderr, "%c (%s) ", level, tag); 
  vfprintf(stderr, fmt, args); 
  fputc('\n', stderr);
}