// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include <cinttypes>
#include "esp.hpp"

void * allocate(size_t size) 
{
  return ESP::ps_malloc(size);  
}