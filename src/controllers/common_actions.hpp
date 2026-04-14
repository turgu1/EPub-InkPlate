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
  static void returnToLast();
  static void refreshBooksDir();
  static void showLastBook();
  static void about();
  static void powerItOff();
};

#undef PUBLIC