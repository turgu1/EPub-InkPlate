// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "svg_picture.hpp"

#include "unzip.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

auto chooseOptions(uint32_t srcW, uint32_t srcH, Dim max) -> SvgOptions {
  int32_t scale = 1;
  while (scale < 8 &&
         (((srcW + scale - 1) / scale) > max.width || ((srcH + scale - 1) / scale) > max.height)) {
    scale <<= 1;
  }

  SvgOptions options{};
  if (scale >= 8) {
    options.set(SVG_OPTION(SCALE_EIGHTH));
  } else if (scale >= 4) {
    options.set(SVG_OPTION(SCALE_QUARTER));
  } else if (scale >= 2) {
    options.set(SVG_OPTION(SCALE_HALF));
  }

  return options;
}

auto onSvgDraw(SvgDraw *draw) -> int32_t {
  auto *data = static_cast<SvgPicture::PictureData *>(draw->pUser);
  if (data == nullptr || data->bitmap == nullptr || draw->pPixels == nullptr) return 0;
  if (draw->iHeight <= 0 || draw->iWidth <= 0 || draw->iBpp != 8) return 0;

  if (draw->y < 0 || draw->y + draw->iHeight > data->dim.height) return 0;
  if (draw->x < 0 || draw->x + draw->iWidth > data->dim.width) return 0;

  for (int32_t row = 0; row < draw->iHeight; ++row) {
    auto *dst = data->bitmap + ((draw->y + row) * data->dim.width + draw->x);
    auto *src = draw->pPixels + (row * draw->iWidth);
    memcpy(dst, src, static_cast<std::size_t>(draw->iWidth));
  }

  return 1;
}

} // namespace

SvgPicture::SvgPicture(const HimemString &filename, Dim max, bool loadBitmap, FontPtr &font,
                       bool fromFile)
    : Picture(), font(font) {
  LOG_D("Loading SVG picture file %s", filename.c_str());

  if (fromFile) {
    loadFromFile(filename, max, loadBitmap);
  } else {
    loadFromZip(filename, max, loadBitmap);
  }
}

auto SvgPicture::loadFromZip(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  uint32_t encodedSize = 0;
  if (!unzip.openStreamFile(filename.c_str(), encodedSize)) return;

  auto encoded = makeUniqueHimem<uint8_t[]>(encodedSize);
  if (encoded == nullptr) {
    LOG_E("Unable to allocate SVG input buffer (%" PRIu32 " bytes)", encodedSize);
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
    LOG_E("Unable to fully read SVG content from zip (%" PRIu32 "/%" PRIu32 " bytes)", total,
          encodedSize);
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto SvgPicture::loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  FILE *f = fopen(filename.c_str(), "rb");
  if (f == nullptr) {
    LOG_E("Unable to open SVG file: %s", filename.c_str());
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
    LOG_E("Unable to allocate SVG input buffer (%" PRIu32 " bytes)", encodedSize);
    fclose(f);
    return;
  }

  const size_t read = fread(encoded.get(), 1, encodedSize, f);
  fclose(f);

  if (read != encodedSize) {
    LOG_E("Unable to fully read SVG file: %s", filename.c_str());
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto SvgPicture::decodeFromBuffer(const uint8_t *data, uint32_t dataSize, Dim max, bool loadBitmap)
    -> void {
  SvgDecoder decoder(font);
  if (!decoder.openRAM(data, static_cast<int32_t>(dataSize), onSvgDraw)) {
    LOG_E("Unable to open SVG decoder. Error: %d", static_cast<int>(decoder.getLastError()));
    return;
  }

  origDim = Dim(decoder.getWidth(), decoder.getHeight());

  const SvgOptions options = chooseOptions(decoder.getWidth(), decoder.getHeight(), max);

  if (options.test(SVG_OPTION(SCALE_EIGHTH))) {
    dim = Dim((decoder.getWidth() + 7) / 8, (decoder.getHeight() + 7) / 8);
  } else if (options.test(SVG_OPTION(SCALE_QUARTER))) {
    dim = Dim((decoder.getWidth() + 3) / 4, (decoder.getHeight() + 3) / 4);
  } else if (options.test(SVG_OPTION(SCALE_HALF))) {
    dim = Dim((decoder.getWidth() + 1) / 2, (decoder.getHeight() + 1) / 2);
  } else {
    dim = Dim(decoder.getWidth(), decoder.getHeight());
  }

  if (!loadBitmap) return;

  bitmap = makeUniqueHimem<uint8_t[]>(dim.width * dim.height);
  if (bitmap == nullptr) {
    LOG_E("Unable to allocate SVG bitmap (%d bytes)", dim.width * dim.height);
    return;
  }

  pictureData = {.dim = dim, .bitmap = bitmap.get()};
  decoder.setUserPointer(&pictureData);

  if (!decoder.decode(0, 0, options)) {
    LOG_E("Unable to decode SVG. Error: %d", static_cast<int>(decoder.getLastError()));
    bitmap.reset();
    dim = Dim(0, 0);
    return;
  }

  if (decoder.getOutputWidth() != dim.width || decoder.getOutputHeight() != dim.height) {
    LOG_E("SVG decoder output dimensions mismatch (%d,%d) expected (%d,%d)",
          decoder.getOutputWidth(), decoder.getOutputHeight(), dim.width, dim.height);
    bitmap.reset();
    dim = Dim(0, 0);
  }
}
