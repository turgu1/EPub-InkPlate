#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include "global.hpp"

#include <string>
#include <array>

class Config
{
  public:
    enum class Ident { 
      SSID, PWD, PORT, BATTERY, FONT_SIZE, TIMEOUT, ORIENTATION, VERSION,
      USE_FONTS_IN_BOOKS, DEFAULT_FONT, SHOW_IMAGES, PIXEL_RESOLUTION 
    };

  private:
    static constexpr char const * TAG         = "Config"; 
    static constexpr char const * CONFIG_FILE = MAIN_FOLDER "/config.txt";
    enum class EntryType { STRING, INT, BYTE };

    struct ConfigDescr {
      Ident        ident;
      EntryType    type;
      const char * caption;
      void       * value;
      const void * default_value;
      uint8_t      max_size;
    };

    FILE *f;
    static std::array<Config::ConfigDescr, 12> cfg;
    bool modified;

    bool parse_line(
      char *  buff,    uint16_t max_size, 
      char ** caption, char  ** value);

  public:
    Config() : f(nullptr), modified(false) {};

    bool get(Ident id, int32_t     * val);
    bool get(Ident id, int8_t      * val);
    bool get(Ident id, std::string & val);
    void put(Ident id, int32_t       val);
    void put(Ident id, int8_t        val);
    void put(Ident id, std::string & val);

    bool read();
    bool save(bool force = false);
    void show();
};

#if __CONFIG__
  Config config;
#else
  extern Config config;
#endif

#endif