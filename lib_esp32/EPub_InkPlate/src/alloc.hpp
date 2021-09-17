// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "esp.hpp"

inline void * allocate(size_t size) { return ESP::ps_malloc(size); }

