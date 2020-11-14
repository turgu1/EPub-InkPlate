// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef __PARAM_VIEW_HPP__
#define __PARAM_VIEW_HPP__

#include "global.hpp"

class ParamView
{
  private:
    static constexpr char const * TAG = "ParamView";

  public:
    ParamView();
};

#if __PARAM_VIEW__
  ParamView param_view;
#else
  extern ParamView param_view;
#endif

#endif