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

