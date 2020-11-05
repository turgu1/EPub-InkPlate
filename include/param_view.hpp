#ifndef __PARAM_VIEW_HPP__
#define __PARAM_VIEW_HPP__

#include "global.hpp"

class ParamView
{
  public:
    ParamView();
};

#if __PARAM_VIEW__
  ParamView param_view;
#else
  extern ParamView param_view;
#endif

#endif