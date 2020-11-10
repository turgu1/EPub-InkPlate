#include <cinttypes>
#include "esp.hpp"

void * allocate(size_t size) 
{
  return ESP::ps_malloc(size);  
}