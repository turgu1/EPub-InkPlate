#define __LOGGING__ 1
#include "logging.hpp"

#include <cstdio>
#include <cstdarg>

void log(char level, char * tag, char * fmt, ...) 
{ 
  va_list args;
  va_start(args, fmt);
  
  fprintf(stderr, "%c (%s) ", level, tag); 
  vfprintf(stderr, fmt, args); 
  fputc('\n', stderr);
}