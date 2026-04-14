#if EPUB_INKPLATE_BUILD

  #pragma once

  #include "global.hpp"
  #include "himem.hpp"

  #include "viewers/page.hpp"

  #include <vector>

  using ScreenSaverPtr = himemUniquePtr<class ScreenSaver>;
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
    friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

    static inline auto Make() { return makeUniqueHimem<ScreenSaver>(); }

    auto show() -> void;
  };

#endif