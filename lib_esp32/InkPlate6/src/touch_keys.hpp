#ifndef __TOUCH_KEYS_HPP__
#define __TOUCH_KEYS_HPP__

#include "noncopyable.hpp"
#include "mcp.hpp"

class TouchKeys : NonCopyable 
{
  public:
    enum class Key : uint8_t { KEY_0, KEY_1, KEY_2 };

    static inline TouchKeys & get_singleton() noexcept { return singleton; }

    bool setup();

    /**
     * @brief Read a specific touchkey
     * 
     * Read one of the three touchkeys.
     * 
     * @param key touchkey number (0, 1 or 2)
     * @return uint8_t 1 if touched, 0 if not
     */
    uint8_t read_key(Key key);

    /**
     * @brief Read all keys
     * 
     * @return uint8_t keys bitmap (bit = 1 if touched) keys 0, 1 and 2 are mapped to bits 0, 1 and 2.
     */
    uint8_t read_all_keys();

  private:
    static constexpr char const * TAG = "TouchKeys";
    static TouchKeys singleton;
    TouchKeys();

    const MCP::Pin TOUCH_0 = MCP::Pin::IOPIN_10;
    const MCP::Pin TOUCH_1 = MCP::Pin::IOPIN_11;
    const MCP::Pin TOUCH_2 = MCP::Pin::IOPIN_12;

};

#ifdef __TOUCH_KEYS__
  TouchKeys & touch_keys = TouchKeys::get_singleton();
#else
  extern TouchKeys & touch_keys;
#endif

#endif