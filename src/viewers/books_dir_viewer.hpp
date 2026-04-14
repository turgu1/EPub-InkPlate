// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

using BooksDirViewerPtr = himemUniquePtr<class BooksDirViewer>;
class BooksDirViewer {
public:
  virtual void setup() = 0;

  virtual int16_t showPageAndHighlight(int16_t bookIdx) = 0;
  virtual void highlightBook(int16_t bookIdx)           = 0;
  virtual void clearHighlight()                         = 0;

  virtual int16_t nextPage()   = 0;
  virtual int16_t prevPage()   = 0;
  virtual int16_t nextItem()   = 0;
  virtual int16_t prevItem()   = 0;
  virtual int16_t nextColumn() = 0;
  virtual int16_t prevColumn() = 0;

  virtual int16_t getIndexAt(uint16_t x, uint16_t y) = 0;
};
