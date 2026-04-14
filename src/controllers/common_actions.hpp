// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#if __COMMON_ACTIONS__
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

class CommonActions {
public:
  static auto returnToLast() -> void;
  static auto refreshBooksDir() -> void;
  static auto showLastBook() -> void;
  static auto about() -> void;
  static auto powerItOff() -> void;
};

#undef PUBLIC