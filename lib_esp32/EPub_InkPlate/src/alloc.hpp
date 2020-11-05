#include <cinttypes>
#include "esp.hpp"

inline void * allocate(uint32_t size) 
{
  return ESP::ps_malloc(size);  
}