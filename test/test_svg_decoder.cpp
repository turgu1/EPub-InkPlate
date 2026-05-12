// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "config.hpp"
#include "fonts.hpp"
#include "picture_factory.hpp"
#include "svg_decoder.hpp"
#include "test_stats.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(__linux__)
  #include <unistd.h>
#endif

#if defined(__GLIBC__)
  #include <malloc.h>
#endif

static int checks   = 0;
static int failures = 0;

#define CHECK(cond)                                                                                \
  do {                                                                                             \
    ++checks;                                                                                      \
    if (!(cond)) {                                                                                 \
      ++failures;                                                                                  \
      std::printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond);                              \
    }                                                                                              \
  } while (0)

namespace {

static constexpr const char *kSvgResultsDir = "test/results/svg_decoder/";

auto ensureSvgFontHierarchy() -> bool {
  static bool initialized = false;
  static bool ok          = false;

  if (initialized) return ok;
  initialized = true;

  if (!config.read()) {
    std::printf("  FAIL [%s] unable to read test config from MAIN_FOLDER/config.txt\n", __func__);
    return false;
  }

  ok = appFonts.setup();
  if (!ok) {
    std::printf("  FAIL [%s] unable to setup app fonts from Config/FontsDB\n", __func__);
  }

  return ok;
}

struct DrawSink {
  int expectedW{0};
  int expectedH{0};
  int expectedY0{0};
  int linesSeen{0};
  bool valid{true};
  std::vector<uint8_t> rowSeen{};
};

auto drawCb(SvgDraw *draw) -> int32_t {
  if (draw == nullptr || draw->pPixels == nullptr) return 0;

  auto *sink = static_cast<DrawSink *>(draw->pUser);
  if (sink == nullptr) return 0;

  if (draw->iBpp != 8 || draw->iHeight <= 0 || draw->iWidth != sink->expectedW) {
    sink->valid = false;
    return 0;
  }

  const int rowStart = draw->y - sink->expectedY0;
  const int rowEnd   = rowStart + draw->iHeight;
  if (rowStart < 0 || rowEnd > sink->expectedH) {
    sink->valid = false;
    return 0;
  }

  for (int row = rowStart; row < rowEnd; ++row) {
    if (sink->rowSeen.empty() || sink->rowSeen[static_cast<std::size_t>(row)] != 0U) {
      sink->valid = false;
      return 0;
    }
    sink->rowSeen[static_cast<std::size_t>(row)] = 1U;
  }

  sink->linesSeen += draw->iHeight;
  if (sink->linesSeen > sink->expectedH) {
    sink->valid = false;
    return 0;
  }

  return 1;
}

// 8x8 synthetic SVG with black background and a white inset square.
constexpr const char *kSvg8x8Simple =
    "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8' viewBox='0 0 8 8'>"
    "<rect x='0' y='0' width='8' height='8' fill='black'/>"
    "<rect x='2' y='2' width='4' height='4' fill='white'/>"
    "</svg>";

auto runScaleCase(SvgOptions opts, int expectedW, int expectedH, FontPtr &font) -> void {
  SvgDecoder decoder(font);
  const int openOk = decoder.openRAM(reinterpret_cast<const uint8_t *>(kSvg8x8Simple),
                                     static_cast<int32_t>(std::strlen(kSvg8x8Simple)), drawCb);
  CHECK(openOk == 1);
  CHECK(decoder.getLastError() == SvgError::SUCCESS);
  CHECK(decoder.getWidth() == 8);
  CHECK(decoder.getHeight() == 8);

  DrawSink sink{.expectedW  = expectedW,
                .expectedH  = expectedH,
                .expectedY0 = 0,
                .rowSeen    = std::vector<uint8_t>(static_cast<std::size_t>(expectedH), 0U)};
  decoder.setUserPointer(&sink);

  const int decOk = decoder.decode(0, 0, opts);
  CHECK(decOk == 1);
  CHECK(decoder.getLastError() == SvgError::SUCCESS);
  CHECK(decoder.getOutputWidth() == expectedW);
  CHECK(decoder.getOutputHeight() == expectedH);
  CHECK(sink.valid);
  CHECK(sink.linesSeen == expectedH);
  CHECK(std::all_of(sink.rowSeen.begin(), sink.rowSeen.end(),
                    [](uint8_t seen) { return seen != 0U; }));
}

struct SvgFixture {
  std::string fileName{};
  int width{0};
  int height{0};
  bool hasText{false};
  std::vector<uint8_t> bytes{};
  std::vector<uint8_t> expectedBitmap{};
};

struct BitmapSink {
  int expectedW{0};
  int expectedH{0};
  int expectedY0{0};
  int linesSeen{0};
  bool valid{true};
  std::vector<uint8_t> rowSeen{};
  std::vector<uint8_t> bitmap{};
};

auto trimInPlace(std::string &s) -> void {
  const auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };

  while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
}

auto fileStem(const std::string &name) -> std::string {
  const std::size_t dot = name.find_last_of('.');
  if (dot == std::string::npos) return name;
  return name.substr(0, dot);
}

auto ensureSvgResultsDir() -> bool {
  std::error_code ec;
  std::filesystem::create_directories(kSvgResultsDir, ec);
  return !ec;
}

auto writeGrayBmp(const std::string &path, int width, int height,
                  const std::vector<uint8_t> &pixels) -> bool {
  if (width <= 0 || height <= 0) return false;
  if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    return false;
  }

  const uint32_t rowStride =
      static_cast<uint32_t>(((static_cast<uint32_t>(width) * 8U + 31U) / 32U) * 4U);
  const uint32_t imageSize = rowStride * static_cast<uint32_t>(height);
  const uint32_t dataOff   = 14U + 40U + 1024U;
  const uint32_t fileSize  = dataOff + imageSize;

  std::ofstream ofs(path, std::ios::binary);
  if (!ofs.is_open()) return false;

  const auto writeU16 = [&ofs](uint16_t v) {
    const uint8_t b[2] = {static_cast<uint8_t>(v & 0xffU), static_cast<uint8_t>((v >> 8U) & 0xffU)};
    ofs.write(reinterpret_cast<const char *>(b), 2);
  };

  const auto writeU32 = [&ofs](uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v & 0xffU), static_cast<uint8_t>((v >> 8U) & 0xffU),
                          static_cast<uint8_t>((v >> 16U) & 0xffU),
                          static_cast<uint8_t>((v >> 24U) & 0xffU)};
    ofs.write(reinterpret_cast<const char *>(b), 4);
  };

  ofs.put('B');
  ofs.put('M');
  writeU32(fileSize);
  writeU16(0U);
  writeU16(0U);
  writeU32(dataOff);

  writeU32(40U);
  writeU32(static_cast<uint32_t>(width));
  writeU32(static_cast<uint32_t>(height));
  writeU16(1U);
  writeU16(8U);
  writeU32(0U);
  writeU32(imageSize);
  writeU32(2835U);
  writeU32(2835U);
  writeU32(256U);
  writeU32(256U);

  for (int i = 0; i < 256; ++i) {
    const uint8_t c = static_cast<uint8_t>(i);
    ofs.put(static_cast<char>(c));
    ofs.put(static_cast<char>(c));
    ofs.put(static_cast<char>(c));
    ofs.put('\0');
  }

  const std::vector<uint8_t> pad(
      static_cast<std::size_t>(rowStride) - static_cast<std::size_t>(width), 0U);

  for (int y = height - 1; y >= 0; --y) {
    const uint8_t *row =
        pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    ofs.write(reinterpret_cast<const char *>(row), width);
    if (!pad.empty()) {
      ofs.write(reinterpret_cast<const char *>(pad.data()),
                static_cast<std::streamsize>(pad.size()));
    }
  }

  return ofs.good();
}

auto loadFileBytes(const std::string &path, std::vector<uint8_t> &out) -> bool {
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs.is_open()) return false;

  const auto size = static_cast<std::size_t>(ifs.tellg());
  ifs.seekg(0, std::ios::beg);

  out.assign(size, 0U);
  if (size == 0U) return true;

  ifs.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(size));
  return ifs.good() || ifs.eof();
}

auto readU16(const std::vector<uint8_t> &buf, std::size_t off, uint16_t &v) -> bool {
  if (off + 2 > buf.size()) return false;
  v = static_cast<uint16_t>(buf[off]) |
      static_cast<uint16_t>(static_cast<uint16_t>(buf[off + 1]) << 8U);
  return true;
}

auto readU32(const std::vector<uint8_t> &buf, std::size_t off, uint32_t &v) -> bool {
  if (off + 4 > buf.size()) return false;
  v = static_cast<uint32_t>(buf[off]) | (static_cast<uint32_t>(buf[off + 1]) << 8U) |
      (static_cast<uint32_t>(buf[off + 2]) << 16U) | (static_cast<uint32_t>(buf[off + 3]) << 24U);
  return true;
}

auto readS32(const std::vector<uint8_t> &buf, std::size_t off, int32_t &v) -> bool {
  uint32_t u = 0;
  if (!readU32(buf, off, u)) return false;
  v = static_cast<int32_t>(u);
  return true;
}

auto extractChannel(uint32_t px, uint32_t mask) -> uint8_t {
  if (mask == 0U) return 0U;

  uint32_t shift = 0U;
  while (((mask >> shift) & 1U) == 0U && shift < 32U) ++shift;

  uint32_t bits = 0U;
  while (((mask >> (shift + bits)) & 1U) != 0U && (shift + bits) < 32U) ++bits;

  if (bits == 0U) return 0U;

  const uint32_t value = (px & mask) >> shift;
  const uint32_t maxV  = (bits >= 31U) ? 0xffffffffU : ((1U << bits) - 1U);
  if (maxV == 0U) return 0U;
  return static_cast<uint8_t>((value * 255U + (maxV / 2U)) / maxV);
}

auto loadBmpAsGray(const std::string &path, int expectedW, int expectedH,
                   std::vector<uint8_t> &grayOut) -> bool {
  std::vector<uint8_t> bmp;
  if (!loadFileBytes(path, bmp)) return false;
  if (bmp.size() < 54U) return false;
  if (!(bmp[0] == 'B' && bmp[1] == 'M')) return false;

  uint32_t dataOffset  = 0;
  uint32_t dibSize     = 0;
  int32_t width        = 0;
  int32_t height       = 0;
  uint16_t planes      = 0;
  uint16_t bpp         = 0;
  uint32_t compression = 0;
  uint32_t clrUsed     = 0;

  if (!readU32(bmp, 10U, dataOffset) || !readU32(bmp, 14U, dibSize) || !readS32(bmp, 18U, width) ||
      !readS32(bmp, 22U, height) || !readU16(bmp, 26U, planes) || !readU16(bmp, 28U, bpp) ||
      !readU32(bmp, 30U, compression) || !readU32(bmp, 46U, clrUsed)) {
    return false;
  }

  if (planes != 1U || width <= 0 || height == 0) return false;

  const bool topDown = (height < 0);
  const int32_t absH = (height < 0) ? -height : height;
  if (width != expectedW || absH != expectedH) return false;

  const std::size_t rowStride =
      ((static_cast<std::size_t>(width) * static_cast<std::size_t>(bpp) + 31U) / 32U) * 4U;
  const std::size_t totalPixelBytes = rowStride * static_cast<std::size_t>(absH);
  if (static_cast<std::size_t>(dataOffset) + totalPixelBytes > bmp.size()) return false;

  grayOut.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(absH), 0U);

  std::vector<uint8_t> palette;
  if (bpp == 8U) {
    uint32_t colors = clrUsed;
    if (colors == 0U) colors = 256U;
    const std::size_t paletteStart = 14U + static_cast<std::size_t>(dibSize);
    const std::size_t paletteSize  = static_cast<std::size_t>(colors) * 4U;
    if (paletteStart + paletteSize > bmp.size()) return false;
    palette.assign(bmp.begin() + static_cast<std::ptrdiff_t>(paletteStart),
                   bmp.begin() + static_cast<std::ptrdiff_t>(paletteStart + paletteSize));
  }

  uint32_t redMask   = 0x00ff0000U;
  uint32_t greenMask = 0x0000ff00U;
  uint32_t blueMask  = 0x000000ffU;
  if (bpp == 32U && compression == 3U && dibSize >= 56U) {
    uint32_t r = 0, g = 0, b = 0;
    if (!readU32(bmp, 54U, r) || !readU32(bmp, 58U, g) || !readU32(bmp, 62U, b)) return false;
    if (r != 0U) redMask = r;
    if (g != 0U) greenMask = g;
    if (b != 0U) blueMask = b;
  }

  for (int y = 0; y < absH; ++y) {
    const int srcY = topDown ? y : (absH - 1 - y);
    const std::size_t srcRow =
        static_cast<std::size_t>(dataOffset) + static_cast<std::size_t>(srcY) * rowStride;

    for (int x = 0; x < width; ++x) {
      uint8_t gray = 0xffU;
      if (bpp == 32U && (compression == 0U || compression == 3U)) {
        const std::size_t off = srcRow + static_cast<std::size_t>(x) * 4U;
        if (off + 4U > bmp.size()) return false;
        const uint32_t px = static_cast<uint32_t>(bmp[off]) |
                            (static_cast<uint32_t>(bmp[off + 1]) << 8U) |
                            (static_cast<uint32_t>(bmp[off + 2]) << 16U) |
                            (static_cast<uint32_t>(bmp[off + 3]) << 24U);
        const uint8_t r   = extractChannel(px, redMask);
        const uint8_t g   = extractChannel(px, greenMask);
        const uint8_t b   = extractChannel(px, blueMask);
        gray              = static_cast<uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
      } else if (bpp == 24U && compression == 0U) {
        const std::size_t off = srcRow + static_cast<std::size_t>(x) * 3U;
        if (off + 3U > bmp.size()) return false;
        const uint8_t b = bmp[off + 0U];
        const uint8_t g = bmp[off + 1U];
        const uint8_t r = bmp[off + 2U];
        gray            = static_cast<uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
      } else if (bpp == 8U && compression == 0U) {
        const std::size_t off = srcRow + static_cast<std::size_t>(x);
        if (off >= bmp.size()) return false;
        const uint8_t idx      = bmp[off];
        const std::size_t poff = static_cast<std::size_t>(idx) * 4U;
        if (poff + 3U > palette.size()) return false;
        const uint8_t b = palette[poff + 0U];
        const uint8_t g = palette[poff + 1U];
        const uint8_t r = palette[poff + 2U];
        gray            = static_cast<uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
      } else {
        return false;
      }

      grayOut[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
              static_cast<std::size_t>(x)] = gray;
    }
  }

  return true;
}

auto drawCbCapture(SvgDraw *draw) -> int32_t {
  if (draw == nullptr || draw->pPixels == nullptr) return 0;

  auto *sink = static_cast<BitmapSink *>(draw->pUser);
  if (sink == nullptr) return 0;

  if (draw->iBpp != 8 || draw->iHeight <= 0 || draw->iWidth != sink->expectedW) {
    sink->valid = false;
    return 0;
  }

  const int rowStart = draw->y - sink->expectedY0;
  const int rowEnd   = rowStart + draw->iHeight;
  if (rowStart < 0 || rowEnd > sink->expectedH) {
    sink->valid = false;
    return 0;
  }

  for (int row = rowStart; row < rowEnd; ++row) {
    if (sink->rowSeen.empty() || sink->rowSeen[static_cast<std::size_t>(row)] != 0U) {
      sink->valid = false;
      return 0;
    }

    const uint8_t *src = draw->pPixels + static_cast<std::size_t>(row - rowStart) *
                                             static_cast<std::size_t>(draw->iWidth);
    uint8_t *dst       = sink->bitmap.data() +
                         static_cast<std::size_t>(row) * static_cast<std::size_t>(sink->expectedW);
    std::memcpy(dst, src, static_cast<std::size_t>(draw->iWidth));

    sink->rowSeen[static_cast<std::size_t>(row)] = 1U;
  }

  sink->linesSeen += draw->iHeight;
  if (sink->linesSeen > sink->expectedH) {
    sink->valid = false;
    return 0;
  }

  return 1;
}

auto compareAgainstExpectedBitmap(const SvgFixture &fixture, const std::vector<uint8_t> &actual)
    -> void {
  CHECK(actual.size() == fixture.expectedBitmap.size());
  if (actual.size() != fixture.expectedBitmap.size()) return;

  const bool isRelaxedFixture          = fixture.hasText || fixture.fileName == "chip.std.svg";
  const int kPerPixelTolerance         = isRelaxedFixture ? 22 : 18;
  const double kMaxMeanAbsDiff         = isRelaxedFixture ? 40.0 : 36.0;
  const double kMaxTolMismatchRatio    = isRelaxedFixture ? 0.32 : 0.24;
  const double kMaxBinaryMismatchRatio = isRelaxedFixture ? 0.12 : 0.07;

  std::size_t tolMismatches    = 0;
  std::size_t binaryMismatches = 0;
  uint64_t sumAbsDiff          = 0;

  for (std::size_t i = 0; i < actual.size(); ++i) {
    const int a = static_cast<int>(actual[i]);
    const int e = static_cast<int>(fixture.expectedBitmap[i]);
    const int d = std::abs(a - e);

    sumAbsDiff += static_cast<uint64_t>(d);
    if (d > kPerPixelTolerance) ++tolMismatches;
    if ((a < 128) != (e < 128)) ++binaryMismatches;
  }

  const double count               = static_cast<double>(actual.size());
  const double meanAbsDiff         = static_cast<double>(sumAbsDiff) / count;
  const double tolMismatchRatio    = static_cast<double>(tolMismatches) / count;
  const double binaryMismatchRatio = static_cast<double>(binaryMismatches) / count;

  if (meanAbsDiff > kMaxMeanAbsDiff || tolMismatchRatio > kMaxTolMismatchRatio ||
      binaryMismatchRatio > kMaxBinaryMismatchRatio) {
    std::printf("  NOTE: bmp mismatch %s: mean=%.2f tol_ratio=%.4f bin_ratio=%.4f\n",
                fixture.fileName.c_str(), meanAbsDiff, tolMismatchRatio, binaryMismatchRatio);
  }

  CHECK(meanAbsDiff <= kMaxMeanAbsDiff);
  CHECK(tolMismatchRatio <= kMaxTolMismatchRatio);
  CHECK(binaryMismatchRatio <= kMaxBinaryMismatchRatio);
}

auto parseFixtureLine(const std::string &rawLine, SvgFixture &fixture) -> bool {
  if (rawLine.empty()) return false;
  if (rawLine[0] == '#') return false;

  const std::size_t colon = rawLine.find(':');
  if (colon == std::string::npos) return false;

  std::string fileName = rawLine.substr(0, colon);
  std::string dims     = rawLine.substr(colon + 1);
  trimInPlace(fileName);
  trimInPlace(dims);

  const std::size_t x = dims.find('x');
  if (x == std::string::npos) return false;

  std::string wStr = dims.substr(0, x);
  std::string hStr = dims.substr(x + 1);
  trimInPlace(wStr);
  trimInPlace(hStr);
  if (wStr.empty() || hStr.empty()) return false;

  int w = 0;
  int h = 0;
  try {
    w = std::stoi(wStr);
    h = std::stoi(hStr);
  } catch (...) {
    return false;
  }

  if (w <= 0 || h <= 0) return false;

  fixture.fileName = fileName;
  fixture.width    = w;
  fixture.height   = h;
  return true;
}

auto getFixtureFilter() -> std::string {
  const char *value = std::getenv("TEST_SVG_DECODER_FIXTURE");
  if (value == nullptr) return {};

  std::string filter(value);
  trimInPlace(filter);
  return filter;
}

auto loadSvgFixtures() -> std::vector<SvgFixture> {
  static constexpr const char *kBaseDir  = "test/fixtures/svgs/";
  static constexpr const char *kListFile = "test/fixtures/svgs/list.txt";
  const std::string fixtureFilter        = getFixtureFilter();
  bool matchedFilter                     = fixtureFilter.empty();

  std::vector<SvgFixture> fixtures;
  std::ifstream list(kListFile);
  if (!list.is_open()) {
    std::printf("  FAIL [%s] unable to open fixture list: %s\n", __func__, kListFile);
    ++failures;
    return fixtures;
  }

  std::string line;
  int lineNo = 0;
  while (std::getline(list, line)) {
    ++lineNo;
    trimInPlace(line);
    if (line.empty() || line[0] == '#') continue;

    SvgFixture fixture;
    if (!parseFixtureLine(line, fixture)) {
      std::printf("  FAIL [%s] malformed fixture list line %d: %s\n", __func__, lineNo,
                  line.c_str());
      ++failures;
      continue;
    }

    if (!fixtureFilter.empty() && fixture.fileName != fixtureFilter) continue;

    matchedFilter = true;

    const std::string filePath = std::string(kBaseDir) + fixture.fileName;
    if (!loadFileBytes(filePath, fixture.bytes)) {
      std::printf("  FAIL [%s] unable to read fixture: %s\n", __func__, filePath.c_str());
      ++failures;
      continue;
    }

    if (fixture.bytes.empty()) {
      std::printf("  FAIL [%s] fixture is empty: %s\n", __func__, filePath.c_str());
      ++failures;
      continue;
    }

    {
      std::string svgText(reinterpret_cast<const char *>(fixture.bytes.data()),
                          fixture.bytes.size());
      fixture.hasText = (svgText.find("<text") != std::string::npos ||
                         svgText.find("<tspan") != std::string::npos);
    }

    const std::string baseName = fileStem(fixture.fileName);
    const std::string bmpPath  = std::string(kBaseDir) + baseName + ".bmp";
    if (!loadBmpAsGray(bmpPath, fixture.width, fixture.height, fixture.expectedBitmap)) {
      std::printf("  FAIL [%s] unable to load/parse expected bmp: %s\n", __func__, bmpPath.c_str());
      ++failures;
      continue;
    }

    fixtures.push_back(std::move(fixture));
  }

  if (!fixtureFilter.empty() && !matchedFilter) {
    std::printf("  FAIL [%s] fixture filter did not match: %s\n", __func__, fixtureFilter.c_str());
    ++failures;
  }

  CHECK(!fixtures.empty());
  return fixtures;
}

auto decodeFixtureWithDimensionChecks(const SvgFixture &fixture, FontPtr &font) -> void {
  SvgDecoder decoder(font);

  const int openOk = decoder.openRAM(fixture.bytes.data(),
                                     static_cast<int32_t>(fixture.bytes.size()), drawCbCapture);
  CHECK(openOk == 1);
  CHECK(decoder.getLastError() == SvgError::SUCCESS);
  if (openOk != 1) return;

  CHECK(decoder.getWidth() == fixture.width);
  CHECK(decoder.getHeight() == fixture.height);

  BitmapSink sink{.expectedW  = fixture.width,
                  .expectedH  = fixture.height,
                  .expectedY0 = 0,
                  .rowSeen    = std::vector<uint8_t>(static_cast<std::size_t>(fixture.height), 0U),
                  .bitmap     = std::vector<uint8_t>(static_cast<std::size_t>(fixture.width) *
                                                         static_cast<std::size_t>(fixture.height),
                                                     0xffU)};
  decoder.setUserPointer(&sink);

  const int decOk = decoder.decode(0, 0, SvgOptions{});
  CHECK(decOk == 1);
  CHECK(decoder.getLastError() == SvgError::SUCCESS);
  CHECK(decoder.getOutputWidth() == fixture.width);
  CHECK(decoder.getOutputHeight() == fixture.height);
  CHECK(sink.valid);
  CHECK(sink.linesSeen == fixture.height);
  CHECK(std::all_of(sink.rowSeen.begin(), sink.rowSeen.end(),
                    [](uint8_t seen) { return seen != 0U; }));

  const std::string outPath = std::string(kSvgResultsDir) + fileStem(fixture.fileName) + ".bmp";
  const bool wroteBmp       = writeGrayBmp(outPath, fixture.width, fixture.height, sink.bitmap);
  CHECK(wroteBmp);
  if (!wroteBmp) {
    std::printf("  FAIL [%s] unable to write decoded bmp: %s\n", __func__, outPath.c_str());
  }

  compareAgainstExpectedBitmap(fixture, sink.bitmap);

  decoder.close();
}

auto drawCbNoop(SvgDraw *draw) -> int32_t {
  if (draw == nullptr || draw->pPixels == nullptr) return 0;
  if (draw->iWidth <= 0 || draw->iHeight <= 0 || draw->iBpp != 8) return 0;
  return 1;
}

#if defined(__linux__)
  auto readRssBytes(std::size_t &rssBytes) -> bool {
    std::ifstream statm("/proc/self/statm");
    if (!statm.is_open()) return false;

    long pagesTotal = 0;
    long pagesRss   = 0;
    statm >> pagesTotal >> pagesRss;
    if (!statm.good()) return false;
    if (pagesTotal < 0 || pagesRss < 0) return false;

    const long pageSize = ::sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;

    rssBytes = static_cast<std::size_t>(pagesRss) * static_cast<std::size_t>(pageSize);
    return true;
  }
#endif

auto runMemoryChurnCheck(const std::vector<SvgFixture> &fixtures, FontPtr &font) -> void {
  static constexpr int kRounds = 12;

  #if defined(__linux__)
    std::size_t rssStart = 0;
    std::size_t rssEnd   = 0;
    std::size_t rssPeak  = 0;
    const bool hasRss    = readRssBytes(rssStart);
    rssPeak              = rssStart;
  #else
    const bool hasRss = false;
  #endif

  #if defined(__GLIBC__)
    const auto miStart = ::mallinfo2();
  #endif

  for (int round = 0; round < kRounds; ++round) {
    for (const auto &fixture : fixtures) {
      SvgDecoder decoder(font);
      const int openOk = decoder.openRAM(fixture.bytes.data(),
                                         static_cast<int32_t>(fixture.bytes.size()), drawCbNoop);
      CHECK(openOk == 1);
      if (openOk != 1) continue;

      const int decOk = decoder.decode(0, 0, SvgOptions{});
      CHECK(decOk == 1);
      CHECK(decoder.getOutputWidth() == fixture.width);
      CHECK(decoder.getOutputHeight() == fixture.height);
      decoder.close();
    }

    #if defined(__linux__)
      if (hasRss) {
        std::size_t rssNow = 0;
        if (readRssBytes(rssNow)) rssPeak = std::max(rssPeak, rssNow);
      }
    #endif
  }

  #if defined(__linux__)
    if (hasRss) {
      if (!readRssBytes(rssEnd)) rssEnd = rssPeak;

      static constexpr std::size_t kPeakGrowthLimitBytes = 32U * 1024U * 1024U;
      static constexpr std::size_t kEndGrowthLimitBytes  = 16U * 1024U * 1024U;

      const std::size_t peakGrowth = (rssPeak >= rssStart) ? (rssPeak - rssStart) : 0U;
      const std::size_t endGrowth  = (rssEnd >= rssStart) ? (rssEnd - rssStart) : 0U;

      CHECK(peakGrowth <= kPeakGrowthLimitBytes);
      CHECK(endGrowth <= kEndGrowthLimitBytes);
    } else {
      std::printf("  NOTE: skipped RSS growth checks (unable to read /proc/self/statm).\n");
    }
  #else
    std::printf("  NOTE: skipped RSS growth checks (non-Linux build).\n");
  #endif

  #if defined(__GLIBC__)
    const auto miEnd = ::mallinfo2();

    static constexpr std::size_t kUsedGrowthLimitBytes = 8U * 1024U * 1024U;
    static constexpr std::size_t kFreeGrowthLimitBytes = 32U * 1024U * 1024U;

    const std::size_t usedStart = static_cast<std::size_t>(miStart.uordblks);
    const std::size_t usedEnd   = static_cast<std::size_t>(miEnd.uordblks);
    const std::size_t freeStart = static_cast<std::size_t>(miStart.fordblks);
    const std::size_t freeEnd   = static_cast<std::size_t>(miEnd.fordblks);

    const std::size_t usedGrowth = (usedEnd >= usedStart) ? (usedEnd - usedStart) : 0U;
    const std::size_t freeGrowth = (freeEnd >= freeStart) ? (freeEnd - freeStart) : 0U;

    CHECK(usedGrowth <= kUsedGrowthLimitBytes);
    CHECK(freeGrowth <= kFreeGrowthLimitBytes);
  #else
    std::printf("  NOTE: skipped mallinfo2 checks (non-glibc allocator).\n");
  #endif
}

auto runPictureFactorySmokeCheck(const SvgFixture &fixture, FontPtr &font) -> void {
  HimemString filePath = "test/fixtures/svgs/";
  filePath.append(fixture.fileName.c_str());

  auto pict =
      PictureFactory::create(filePath, Dim(fixture.width, fixture.height), true, font, true);
  CHECK(pict != nullptr);
  if (pict == nullptr) return;

  CHECK(pict->getDim().width == fixture.width);
  CHECK(pict->getDim().height == fixture.height);

  const auto *bmp = pict->getBitmap();
  CHECK(bmp != nullptr);
}

} // namespace

auto testSvgDecoder() -> TestStats {
  std::printf("[testSvgDecoder]\n");
  checks   = 0;
  failures = 0;

  const std::string fixtureFilter = getFixtureFilter();
  if (!fixtureFilter.empty()) {
    std::printf("[testSvgDecoder] fixture filter: %s\n", fixtureFilter.c_str());
  }

  CHECK(ensureSvgFontHierarchy());
  if (!ensureSvgFontHierarchy()) {
    std::printf("[testSvgDecoder] checks=%d failures=%d\n", checks, failures);
    return TestStats{checks - failures, failures};
  }

  FontPtr &font = appFonts.getFont(SYSTEM_REGULAR_FONT_INDEX);

  CHECK(ensureSvgResultsDir());

  runScaleCase(SvgOptions{}, 8, 8, font);

  SvgOptions half{};
  half.set(SVG_OPTION(SCALE_HALF));
  runScaleCase(half, 4, 4, font);

  SvgOptions quarter{};
  quarter.set(SVG_OPTION(SCALE_QUARTER));
  runScaleCase(quarter, 2, 2, font);

  SvgOptions eighth{};
  eighth.set(SVG_OPTION(SCALE_EIGHTH));
  runScaleCase(eighth, 1, 1, font);

  const std::vector<SvgFixture> fixtures = loadSvgFixtures();
  for (const auto &fixture : fixtures) {
    decodeFixtureWithDimensionChecks(fixture, font);
  }

  if (!fixtures.empty()) {
    runPictureFactorySmokeCheck(fixtures.front(), font);
  }

  if (!fixtures.empty()) {
    runMemoryChurnCheck(fixtures, font);
  }

  std::printf("[testSvgDecoder] checks=%d failures=%d\n", checks, failures);
  return TestStats{checks - failures, failures};
}
