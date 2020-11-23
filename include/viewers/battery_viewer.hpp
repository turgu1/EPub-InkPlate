#ifndef __BATTERY_VIEWER_HPP__
#define __BATTERY_VIEWER_HPP__

#include "global.hpp"

#if EPUB_INKPLATE6_BUILD
  namespace BatteryViewer {

    #if __BATTERY_VIEWER__
      void show();
    #else
      extern void show();
    #endif

  }
#endif
#endif
