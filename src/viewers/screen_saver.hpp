#if EPUB_INKPLATE_BUILD

  #pragma once

  #include "global.hpp"
  #include "himem.hpp"

  #include "viewers/page.hpp"

  using ScreenSaverPtr = HimemUniquePtr<class ScreenSaver>;
  class ScreenSaver {
  private:
    static constexpr char const *TAG = "ScreenSaver";

    HimemVector<HimemString> picture_filenames;

    auto setup() -> bool;

    PagePtr page{Page::Make(appFonts)};

  public:
    ScreenSaver()  = default;
    ~ScreenSaver() = default;

    template <typename T, typename... Args>
      requires(!std::is_array_v<T>)
    friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

    static inline auto Make() { return makeUniqueHimem<ScreenSaver>(); }

    auto show() -> void;
  };

#endif