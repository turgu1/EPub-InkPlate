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

using PicturePtr = himem_unique_ptr<class Picture>;

class Picture {

public:
  Picture() = default;
  Picture(Dim d, FileContentPtr b) : dim(d), bitmap(std::move(b)) {}
  Picture(Dim d, const uint8_t *b, uint32_t size) : dim(d) {
    bitmap = make_unique_himem<uint8_t[]>(size);
    memcpy(bitmap.get(), b, size);
    file_size = size;
  }

protected:
  Dim dim{0, 0};
  FileContentPtr bitmap{nullptr};
  bool size_retrieved{false};
  Dim orig_dim{0, 0};
  uint32_t file_size{0};

private:
  static constexpr char const *TAG = "Picture";

public:
  static inline auto Make() { return make_unique_himem<Picture>(); }
  static inline auto Make(Dim d, FileContentPtr b) {
    return make_unique_himem<Picture>(d, std::move(b));
  }
  static inline auto Make(Dim d, const uint8_t *b, uint32_t size) {
    return make_unique_himem<Picture>(d, b, size);
  }

  virtual ~Picture() = default;

  inline const auto get_orig_dim() const -> Dim { return orig_dim; }
  inline const auto get_dim() const -> Dim { return dim; }
  inline const auto get_bitmap() const -> uint8_t * { return bitmap.get(); }

  void resize(Dim new_dim);
};
