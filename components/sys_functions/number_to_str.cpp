#include <cinttypes>
#include <cstring>

char *int_to_str(int v, char *buffer, int8_t size) {
  char *str = &buffer[size];
  bool sign = v < 0;

  if (sign) v = -v;
  *--str = 0;

  do {
    *--str = '0' + (v % 10);
    v /= 10;
  } while ((v != 0) && (str >= buffer));

  if (sign && (str > buffer)) *--str = '-';

  if (str != buffer) {
    char *str2 = buffer;
    while ((*str2++ = *str++));
  }

  return buffer;
}

char *float_to_str(double v, char *buffer, int8_t size, uint8_t precision) {
  int32_t intPart = static_cast<int32_t>(v);
  double fracPart = v - intPart;

  char *str = int_to_str(intPart, buffer, size);

  if (precision > 0) {
    char *dotPos = str + strlen(str);
    *dotPos++    = '.';

    for (uint8_t i = 0; i < precision; ++i) {
      fracPart *= 10;
      int digit = static_cast<int>(fracPart);
      *dotPos++ = '0' + digit;
      fracPart -= digit;
    }
    *dotPos = 0;
  }

  return str;
}
