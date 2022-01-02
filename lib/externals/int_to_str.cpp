char * 
int_to_str(int v, char * buffer, char size)
{
  char * str  = &buffer[size-1];
  bool   sign = v < 0;

  if (sign) v = -v;
  *--str = 0;

  do {
    *--str  = '0' + (v % 10);
     v     /= 10;
  } while ((v != 0) && (str > buffer));
  
  if (sign && (str > buffer)) *--str = '-';

  if (str != buffer) {
    char * str2 = buffer;
    do *str2++ = *str++; while (*str);
    *str2 = 0;
  }

  return buffer;
}
