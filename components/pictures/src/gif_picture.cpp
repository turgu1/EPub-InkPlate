// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "gif_picture.hpp"

#include "unzip.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

auto chooseOptions(uint32_t srcW, uint32_t srcH, Dim max) -> GifOptions {
  int32_t scale = 1;
  while (scale < 8 &&
         (((srcW + scale - 1) / scale) > max.width || ((srcH + scale - 1) / scale) > max.height)) {
    scale <<= 1;
  }

  GifOptions options{};
  if (scale >= 8) {
    options.set(GIF_OPTION(SCALE_EIGHTH));
  } else if (scale >= 4) {
    options.set(GIF_OPTION(SCALE_QUARTER));
  } else if (scale >= 2) {
    options.set(GIF_OPTION(SCALE_HALF));
  }

  return options;
}

auto onGifDraw(GifDraw *draw) -> int32_t {
  auto *data = static_cast<GifPicture::PictureData *>(draw->pUser);
  if (data == nullptr || data->bitmap == nullptr || draw->pPixels == nullptr) return 0;
  if (draw->y < 0 || draw->y >= data->dim.height) return 0;
  if (draw->x < 0 || draw->x >= data->dim.width) return 0;
  if (draw->x + draw->iWidth > data->dim.width) return 0;

  auto *dst = data->bitmap + (draw->y * data->dim.width + draw->x);
  memcpy(dst, draw->pPixels, draw->iWidth);
  return 1;
}

} // namespace

GifPicture::GifPicture(const HimemString &filename, Dim max, bool loadBitmap, bool fromFile)
    : Picture() {
  LOG_D("Loading GIF picture file %s", filename.c_str());

  if (fromFile) {
    loadFromFile(filename, max, loadBitmap);
  } else {
    loadFromZip(filename, max, loadBitmap);
  }
}

auto GifPicture::loadFromZip(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  uint32_t encodedSize = 0;
  if (!unzip.openStreamFile(filename.c_str(), encodedSize)) return;

  auto encoded = makeUniqueHimem<uint8_t[]>(encodedSize);
  if (encoded == nullptr) {
    LOG_E("Unable to allocate GIF input buffer (%" PRIu32 " bytes)", encodedSize);
    unzip.closeStreamFile();
    return;
  }

  uint32_t total = 0;
  while (total < encodedSize) {
    uint32_t toRead = encodedSize - total;
    if (toRead > 4096) toRead = 4096;

    const uint32_t read =
        unzip.getStreamData(reinterpret_cast<char *>(encoded.get() + total), toRead);
    if (read == 0) break;
    total += read;
  }

  unzip.closeStreamFile();

  if (total != encodedSize) {
    LOG_E("Unable to fully read GIF content from zip (%" PRIu32 "/%" PRIu32 " bytes)", total,
          encodedSize);
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto GifPicture::loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  FILE *f = fopen(filename.c_str(), "rb");
  if (f == nullptr) {
    LOG_E("Unable to open GIF file: %s", filename.c_str());
    return;
  }

  fseek(f, 0L, SEEK_END);
  const long encodedSizeLong = ftell(f);
  fseek(f, 0L, SEEK_SET);

  if (encodedSizeLong <= 0) {
    fclose(f);
    return;
  }

  const uint32_t encodedSize = static_cast<uint32_t>(encodedSizeLong);
  auto encoded               = makeUniqueHimem<uint8_t[]>(encodedSize);
  if (encoded == nullptr) {
    LOG_E("Unable to allocate GIF input buffer (%" PRIu32 " bytes)", encodedSize);
    fclose(f);
    return;
  }

  const size_t read = fread(encoded.get(), 1, encodedSize, f);
  fclose(f);

  if (read != encodedSize) {
    LOG_E("Unable to fully read GIF file: %s", filename.c_str());
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto GifPicture::decodeFromBuffer(const uint8_t *data, uint32_t dataSize, Dim max, bool loadBitmap)
    -> void {
  GifDecoder decoder;
  if (!decoder.openRAM(data, static_cast<int32_t>(dataSize), onGifDraw)) {
    LOG_E("Unable to open GIF decoder. Error: %d", static_cast<int>(decoder.getLastError()));
    return;
  }

  origDim = Dim(decoder.getWidth(), decoder.getHeight());

  const GifOptions options = chooseOptions(decoder.getWidth(), decoder.getHeight(), max);

  if (options.test(GIF_OPTION(SCALE_EIGHTH))) {
    dim = Dim((decoder.getWidth() + 7) / 8, (decoder.getHeight() + 7) / 8);
  } else if (options.test(GIF_OPTION(SCALE_QUARTER))) {
    dim = Dim((decoder.getWidth() + 3) / 4, (decoder.getHeight() + 3) / 4);
  } else if (options.test(GIF_OPTION(SCALE_HALF))) {
    dim = Dim((decoder.getWidth() + 1) / 2, (decoder.getHeight() + 1) / 2);
  } else {
    dim = Dim(decoder.getWidth(), decoder.getHeight());
  }

  if (!loadBitmap) return;

  bitmap = makeUniqueHimem<uint8_t[]>(dim.width * dim.height);
  if (bitmap == nullptr) {
    LOG_E("Unable to allocate GIF bitmap (%d bytes)", dim.width * dim.height);
    return;
  }

  pictureData = {.dim = dim, .bitmap = bitmap.get()};
  decoder.setUserPointer(&pictureData);

  if (!decoder.decode(0, 0, options)) {
    LOG_E("Unable to decode GIF. Error: %d", static_cast<int>(decoder.getLastError()));
    bitmap.reset();
    dim = Dim(0, 0);
  }
}
