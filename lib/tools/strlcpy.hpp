#pragma once

#include <cstring>

#ifndef HAVE_STRLCAT

  /**
   * @brief Safely concatenate two strings.
   * 
   * @param dst Destination string
   * @param src Source string
   * @param size Size of destination string buffer
   * @return size_t Length of string
   */
  extern size_t strlcat(char * dst, const char * src, size_t size);

#endif

#ifndef HAVE_STRLCPY

  /**
   * @brief Safely copy two strings.
   * 
   * @param dst Destination string
   * @param src Source string
   * @param size Size of destination string buffer
   * @return size_t Length of string
   */
  size_t strlcpy(char * dst, const char *src, size_t size);

#endif
