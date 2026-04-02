#pragma once

#include "global.hpp"
#include "himem.hpp"
#include "picture.hpp"

using BitmapPicturePtr = himem_unique_ptr<class BitmapPicture>;

class BitmapPicture : public Picture {
private:
  BitmapPicture(Dim d, const uint8_t *b, uint32_t size) : Picture(d, b, size) {}

public:
  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make(Dim d, const uint8_t *b, uint32_t size) {
    return make_unique_himem<BitmapPicture>(d, b, size);
  }
  ~BitmapPicture() override = default;
};
