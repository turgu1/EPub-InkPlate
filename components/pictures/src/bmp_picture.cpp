#include "bmp_picture.hpp"

BmpPicture::BmpPicture(const HimemString &filename, Dim max, bool loadBitmap, bool fromFile) {
  LOG_D("Loading BMP picture file {}", filename);

  if (fromFile) {
    loadFromFile(filename, max, loadBitmap);
  } else {
    loadFromZip(filename, max, loadBitmap);
  }
}

auto BmpPicture::loadFromZip(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  uint32_t encodedSize = 0;
  if (!unzip.openStreamFile(filename.c_str(), encodedSize)) { return; }

  auto encoded = makeUniqueHimem<uint8_t[]>(encodedSize);
  if (encoded == nullptr) {
    LOG_E("Unable to allocate BMP input buffer ({} bytes)", encodedSize);
    unzip.closeStreamFile();
    return;
  }

  uint32_t total = 0;
  while (total < encodedSize) {
    uint32_t toRead = encodedSize - total;
    if (toRead > 4096) { toRead = 4096; }

    const uint32_t read =
      unzip.getStreamData(reinterpret_cast<char *>(encoded.get() + total), toRead);
    if (read == 0) { break; }
    total += read;
  }

  unzip.closeStreamFile();

  if (total != encodedSize) {
    LOG_E("Unable to fully read BMP content from zip ({}/{} bytes)", total,
          encodedSize);
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto BmpPicture::loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  FILE *f = fopen(filename.c_str(), "rb");
  if (f == nullptr) {
    LOG_E("Unable to open BMP file: {}", filename);
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
  auto           encoded               = makeUniqueHimem<uint8_t[]>(encodedSize);
  if (encoded == nullptr) {
    LOG_E("Unable to allocate BMP input buffer ({} bytes)", encodedSize);
    fclose(f);
    return;
  }

  const size_t read = fread(encoded.get(), 1, encodedSize, f);
  fclose(f);

  if (read != encodedSize) {
    LOG_E("Unable to fully read BMP file: {}", filename);
    return;
  }

  decodeFromBuffer(encoded.get(), encodedSize, max, loadBitmap);
}

auto BmpPicture::decodeFromBuffer(const uint8_t *data, uint32_t dataSize, Dim max, bool loadBitmap)
-> void {
  BMPDecoder decoder;

  dim = decoder.getDim(data, dataSize);
  bitsPerPixel = 4;

  if (!loadBitmap) { return; }

  auto bitmapSize = (((dim.width + 1) & 0xFFFE) * dim.height) >> 1;

  bitmap = makeUniqueHimem<uint8_t[]>(bitmapSize);
  if (bitmap == nullptr) {
    LOG_E("Unable to allocate BMP bitmap ({} bytes)", bitmapSize);
    return;
  }

  if (!decoder.decode(data, dataSize, bitmap.get())) {
    LOG_E("Unable to decode BMP.");
    bitmap.reset();
    dim = Dim(0, 0);
  }
}
