// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"

#include "viewers/page.hpp"

#if EPUB_INKPLATE_BUILD
  class BatteryViewer {
  private:
    static constexpr char const *TAG = "BatteryViewer";

  public:
    static auto show(PagePtr &page) -> void;
  };
#endif
