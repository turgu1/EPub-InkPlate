#ifndef __OPTION_VIEW_HPP__
#define __OPTION_VIEW_HPP__

#include "global.hpp"

class OptionView
{
  public:
    OptionView();
};

#if __OPTION_VIEW__
  OptionView * option_view;
#else
  extern OptionView * option_view;
#endif

#endif