// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FORM_VIEWER__ 1
#include "viewers/form_viewer.hpp"

MemoryPool<FormChoiceField::Item> FormChoiceField::item_pool;
uint8_t FormChoiceField::font_choices_count = 0;

FormChoice FormChoiceField::font_choices[8] = {
  { nullptr, 0 },
  { nullptr, 1 },
  { nullptr, 2 },
  { nullptr, 3 },
  { nullptr, 4 },
  { nullptr, 5 },
  { nullptr, 6 },
  { nullptr, 7 }
};

bool 
FormDone::event(const EventMgr::Event & event) 
{ 
  form_viewer.set_completed(true); 
  return false; 
}