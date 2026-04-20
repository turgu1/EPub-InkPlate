#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "picture.hpp"

using BitmapPicturePtr = HimemUniquePtr<class BitmapPicture>;

class BitmapPicture : public Picture {
private:
  BitmapPicture(Dim d, const uint8_t *b, uint32_t size) : Picture(d, b, size) {}

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(Dim d, const uint8_t *b, uint32_t size) {
    return makeUniqueHimem<BitmapPicture>(d, b, size);
  }
  ~BitmapPicture() override = default;
};
