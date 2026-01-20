#include <cstring>

#ifndef HAVE_STRLCAT

size_t
strlcat(char * dst, const char * src, size_t size)
{
  size_t srclen, dstlen;

  dstlen = strlen(dst);
  size   -= dstlen + 1;

  if (!size) return dstlen;

  srclen = strlen(src);

  if (srclen > size) srclen = size;

  memcpy(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';

  return dstlen + srclen;
}

#endif /* !HAVE_STRLCAT */

#ifndef HAVE_STRLCPY

size_t
strlcpy(char * dst, const char * src, size_t size)
{
  size_t srclen;

  size --;

  srclen = strlen(src);

  if (srclen > size) srclen = size;

  memcpy(dst, src, srclen);
  dst[srclen] = '\0';

  return srclen;
}

#endif
