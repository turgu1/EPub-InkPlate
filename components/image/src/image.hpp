// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "himem.hpp"
#include "unzip.hpp"

// Class Image
//
// This is a virtual class. It allows for the retrieval of
// images that will have a maximum size as close as possible to the
// screen width and height or smaller. Images, when loaded, are converted
// to 3 bits gray scale pixels.
//
// The ImageFactory class will instanciate the proper class (PngImage or JPegImage)
// depending on the filename extension.

class Image {

protected:
  Dim dim{0, 0};
  FileContentPtr bitmap{nullptr};
  bool size_retrieved{false};
  Dim orig_dim{0, 0};
  uint32_t file_size{0};

public:
  Image() = default;
  Image(Dim d, FileContentPtr b) : dim(d), bitmap(std::move(b)) {}
  Image(Dim d, uint8_t *b, uint32_t size) : dim(d) {
    bitmap = make_unique_himem<uint8_t[]>(size);
    memcpy(bitmap.get(), b, size);
    file_size = size;
  }
  virtual ~Image() = default;

  inline const auto get_orig_dim() const -> Dim { return orig_dim; }
  inline const auto get_dim() const -> Dim { return dim; }
  inline const auto get_bitmap() const -> uint8_t * { return bitmap.get(); }

  void resize(Dim new_dim);

private:
  static constexpr char const *TAG = "Image";
};

using ImagePtr = himem_unique_ptr<Image>;
