#include <cinttypes>

char * 
int_to_str(int v, char * buffer, int8_t size)
{
  char * str  = &buffer[size];
  bool   sign = v < 0;

  if (sign) v = -v;
  *--str = 0;

  do {
    *--str  = '0' + (v % 10);
    v /= 10;
  } while ((v != 0) && (str >= buffer));
  
  if (sign && (str > buffer)) *--str = '-';

  if (str != buffer) {
    char * str2 = buffer;
    while ((*str2++ = *str++)) ;
  }

  return buffer;
}
