// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"

#include "bmp_decoder.hpp"
#include "himem.hpp"
#include "picture.hpp"

using BmpPicturePtr = HimemUniquePtr<class BmpPicture>;

class BmpPicture : public Picture {

  private:
    static constexpr char const *TAG = "BmpPicture";

    BmpPicture(const HimemString &filename, Dim max, bool loadBitmap, bool fromFile = false);

    auto loadFromZip(const HimemString &filename, Dim max, bool loadBitmap) -> void;
    auto loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void;
    auto decodeFromBuffer(const uint8_t *data, uint32_t dataSize, Dim max, bool loadBitmap) -> void;

  public:
    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend HimemUniquePtr<T> makeUniqueHimem(Args &&... args);

    static inline auto Make(const HimemString &filename, Dim max, bool loadBitmap,
                            bool fromFile = false) {
      return makeUniqueHimem<BmpPicture>(filename, max, loadBitmap, fromFile);
    }

    ~BmpPicture() override = default;
};