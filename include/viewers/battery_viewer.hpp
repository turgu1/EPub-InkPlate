// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#if EPUB_INKPLATE_BUILD
  namespace BatteryViewer {

    #if __BATTERY_VIEWER__
      void show();
    #else
      extern void show();
    #endif

  }
#endif

