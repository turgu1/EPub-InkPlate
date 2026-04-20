// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "unzip.hpp"

// Class Picture
//
// This is a virtual class. It allows for the retrieval of
// pictures that will have a maximum size as close as possible to the
// screen width and height or smaller. Pictures, when loaded, are converted
// to 3 bits gray scale pixels.
//
// The PictureFactory class will instanciate the proper class (PngPicture or JPegPicture)
// depending on the filename extension.

using PicturePtr = HimemUniquePtr<class Picture>;

class Picture {

public:
  Picture() = default;
  Picture(Dim d, FileContentPtr b) : dim(d), bitmap(std::move(b)) {}
  Picture(Dim d, const uint8_t *b, uint32_t size) : dim(d) {
    bitmap = makeUniqueHimem<uint8_t[]>(size);
    memcpy(bitmap.get(), b, size);
    fileSize = size;
  }

protected:
  Dim dim{0, 0};
  FileContentPtr bitmap{nullptr};
  bool sizeRetrieved{false};
  Dim origDim{0, 0};
  uint32_t fileSize{0};

private:
  static constexpr char const *TAG = "Picture";

public:
  static inline auto Make() { return makeUniqueHimem<Picture>(); }
  static inline auto Make(Dim d, FileContentPtr b) {
    return makeUniqueHimem<Picture>(d, std::move(b));
  }
  static inline auto Make(Dim d, const uint8_t *b, uint32_t size) {
    return makeUniqueHimem<Picture>(d, b, size);
  }

  virtual ~Picture() = default;

 [[nodiscard]] inline auto getOrigDim() const -> const Dim { return origDim; }
 [[nodiscard]] inline auto getDim() const -> const Dim { return dim; }
 [[nodiscard]] inline auto getBitmap() const -> uint8_t * { return bitmap.get(); }

  auto resize(Dim newDim) -> void;
};
