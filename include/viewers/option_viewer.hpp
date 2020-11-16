// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __OPTION_VIEWER_HPP__
#define __OPTION_VIEWER_HPP__

#include "global.hpp"

class OptionViewer
{
  private:
    static constexpr char const * TAG = "OptionViewer";

  public:
    OptionViewer();
};

#if __OPTION_VIEWER__
  OptionViewer * option_viewer;
#else
  extern OptionViewer * option_viewer;
#endif

#endif