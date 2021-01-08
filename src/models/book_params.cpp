#define __BOOK_PARAMS__ 1
#include "models/book_params.hpp"

bool 
BookParams::read(const std::string fname)
{
  filename = fname;
  
}

bool
BookParams::save()
{
  if (modified) {

  }
}