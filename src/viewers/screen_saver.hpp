#if EPUB_INKPLATE_BUILD

  #pragma once

  #include "global.hpp"

  #include <vector>

  class ScreenSaver {
  private:
    static constexpr char const *TAG = "ScreenSaver";

    std::vector<std::string> picture_filenames;

    auto setup() -> bool;

  public:
    void show();
  };

  #if __SCREEN_SAVER__
    ScreenSaver screen_saver;
  #else
    extern ScreenSaver screen_saver;
  #endif

#endif