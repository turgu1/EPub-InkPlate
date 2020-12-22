#ifndef __BATTERY_HPP__
#define __BATTERY_HPP__

#include "noncopyable.hpp"
#include "mcp.hpp"

class Battery : NonCopyable
{
  public:
    static inline Battery & get_singleton() noexcept { return singleton; }

    bool setup();

    double read_level();

  private:
    static constexpr char const * TAG = "Battery";
    static Battery singleton;
    Battery() {}

    const MCP::Pin BATTERY_SWITCH = MCP::Pin::IOPIN_9;
};

#ifdef __BATTERY__
  Battery & battery = Battery::get_singleton();
#else
  extern Battery & battery;
#endif

#endif