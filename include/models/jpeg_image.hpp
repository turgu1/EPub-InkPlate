#pragma once

#include "global.hpp"
#include "image.hpp"

class JPegImage : public Image
{
  public:
    JPegImage(std::string filename, Dim max);

  private:
    static constexpr char const * TAG = "JPegImage";
    const uint16_t WORK_SIZE = 20 * 1024;
};