// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __PARAM_VIEWER_HPP__
#define __PARAM_VIEWER_HPP__

#include "global.hpp"

class ParamViewer
{
  private:
    static constexpr char const * TAG = "ParamViewer";

  public:
    ParamViewer();
};

#if __PARAM_VIEWER__
  ParamViewer param_viewer;
#else
  extern ParamViewer param_viewer;
#endif

#endif