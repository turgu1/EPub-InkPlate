#ifndef __EINK_HPP__
#define __EINK_HPP__

class Bitmap 
{
  protected:
    const int32_t data_size;
    const int16_t width, height, line_size;
    const uint8_t init_value;

  public:
    Bitmap(int16_t w, int16_t h, int32_t s, uint8_t i) : 
      data_size(s), width(w), height(h), line_size(s / h), init_value(i) {}

    inline int16_t       get_width() { return width;      }
    inline int16_t      get_height() { return height;     }
    inline int16_t   get_data_size() { return data_size;  }
    inline int16_t   get_line_size() { return line_size;  }
    inline uint8_t  get_init_value() { return init_value; }
    
    void              clear() { memset(get_data(), get_init_value(), get_data_size()); } 

    virtual uint8_t * get_data() = 0;
};

class Bitmap1Bit : public Bitmap 
{
  public:
    Bitmap1Bit(int16_t w, int16_t h, int32_t s) : Bitmap(w, h, s, 0) {}
};

class Bitmap3Bit : public Bitmap 
{
  public:
    Bitmap3Bit(int16_t w, int16_t h, int32_t s) : Bitmap(w, h, s, (uint8_t) 0x77) {}
};

class EInk
{
  public:

    enum class PanelState  { OFF, ON };

    inline PanelState get_panel_state() { return panel_state; }
    inline bool        is_initialized() { return initialized; }

    virtual inline Bitmap1Bit * new_bitmap1bit() = 0;
    virtual inline Bitmap3Bit * new_bitmap3bit() = 0;

    // All the following methods are protecting the I2C device trough
    // the Wire::enter() and Wire::leave() methods. These are implementing a
    // Mutex semaphore access control.
    //
    // If you ever add public methods, you *MUST* consider adding calls to Wire::enter()
    // and Wire::leave() and insure no deadlock will happen... or modifu the mutex to use
    // a recursive mutex.

    virtual bool setup() = 0;

    inline void update(Bitmap1Bit & bitmap) { update_1bit(bitmap); }
    inline void update(Bitmap3Bit & bitmap) { update_3bit(bitmap); }

    virtual void partial_update(Bitmap1Bit & bitmap) = 0;

    virtual void clean() = 0;

    int8_t read_temperature();

  protected:                     
    
    EInk() : 
      panel_state(PanelState::OFF), 
      initialized(false),
      partial_allowed(false) {}

    PanelState panel_state;
    bool       initialized;
    bool       partial_allowed;

    virtual void update_1bit(Bitmap1Bit & bitmap) = 0;
    virtual void update_3bit(Bitmap3Bit & bitmap) = 0;
};

#endif