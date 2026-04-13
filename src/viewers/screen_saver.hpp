#if EPUB_INKPLATE_BUILD

  #pragma once

  #include "global.hpp"
  #include "himem.hpp"

  #include "viewers/page.hpp"

  #include <vector>

  using ScreenSaverPtr = himem_unique_ptr<class ScreenSaver>;
  class ScreenSaver {
  private:
    static constexpr char const *TAG = "ScreenSaver";

    std::vector<std::string> picture_filenames;

    auto setup() -> bool;

    PagePtr page{Page::Make()};

  public:
    ScreenSaver()  = default;
    ~ScreenSaver() = default;

    template <typename T, typename... Args>
      requires(!std::is_array_v<T>)
    friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

    static inline auto Make() { return make_unique_himem<ScreenSaver>(); }

    void show();
  };

#endif