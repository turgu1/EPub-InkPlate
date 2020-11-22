#ifndef __BATTERY_VIEWER_HPP__
#define __BATTERY_VIEWER_HPP__

namespace BatteryViewer {

  #if __BATTERY_VIEWER__
    void show();
  #else
    extern void show();
  #endif

}

#endif
