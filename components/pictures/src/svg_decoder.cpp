#include "svg_decoder.hpp"

#include "fonts_db.hpp"
#include "jpeg_decoder.hpp"
#include "mypngle.hpp"
#include "ttf2.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct RasterImage {
  int32_t width{0};
  int32_t height{0};
  std::vector<uint8_t> pixels{};
};

struct JpegDecodeContext {
  RasterImage *image{nullptr};
};

auto onJpegDraw(JPegDraw *draw) -> int32_t {
  if (draw == nullptr || draw->pUser == nullptr || draw->pPixels == nullptr) return 0;
  auto *ctx = static_cast<JpegDecodeContext *>(draw->pUser);
  if (ctx->image == nullptr) return 0;
  if (draw->iBpp != 8) return 0;

  const int32_t w = draw->iWidthUsed > 0 ? draw->iWidthUsed : draw->iWidth;
  if (w <= 0 || draw->iHeight <= 0) return 1;

  const auto *src = reinterpret_cast<const uint8_t *>(draw->pPixels);
  for (int32_t row = 0; row < draw->iHeight; ++row) {
    const int32_t dy = draw->y + row;
    if (dy < 0 || dy >= ctx->image->height) continue;
    const int32_t dx = draw->x;
    if (dx < 0 || dx + w > ctx->image->width) continue;

    auto *dst =
        &ctx->image
             ->pixels[static_cast<std::size_t>(dy) * static_cast<std::size_t>(ctx->image->width) +
                      static_cast<std::size_t>(dx)];
    std::memcpy(dst, src + static_cast<std::size_t>(row) * static_cast<std::size_t>(draw->iWidth),
                static_cast<std::size_t>(w));
  }
  return 1;
}

struct PngDecodeContext {
  RasterImage *image{nullptr};
};

auto onPngInit(pngle_t *pngle, uint32_t w, uint32_t h) -> void {
  if (pngle == nullptr) return;
  auto *ctx = static_cast<PngDecodeContext *>(mypngle_get_user_data(pngle));
  if (ctx == nullptr || ctx->image == nullptr) return;
  if (w == 0 || h == 0) return;

  ctx->image->width  = static_cast<int32_t>(w);
  ctx->image->height = static_cast<int32_t>(h);
  ctx->image->pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0xff);
}

#ifdef PNGLE_GRAYSCALE_OUTPUT
auto onPngDraw(pngle_t *pngle, uint32_t x, uint32_t y, uint8_t pix, uint8_t alpha) -> void {
  if (pngle == nullptr) return;
  auto *ctx = static_cast<PngDecodeContext *>(mypngle_get_user_data(pngle));
  if (ctx == nullptr || ctx->image == nullptr) return;
  if (x >= static_cast<uint32_t>(ctx->image->width) ||
      y >= static_cast<uint32_t>(ctx->image->height))
    return;

  const uint8_t out = static_cast<uint8_t>(
      (static_cast<uint32_t>(alpha) * pix + static_cast<uint32_t>(255U - alpha) * 255U) / 255U);
  ctx->image->pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(ctx->image->width) +
                     static_cast<std::size_t>(x)] = out;
}
#else
auto onPngDraw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
    -> void {
  if (pngle == nullptr) return;
  auto *ctx = static_cast<PngDecodeContext *>(mypngle_get_user_data(pngle));
  if (ctx == nullptr || ctx->image == nullptr || rgba == nullptr) return;

  const uint8_t alpha   = rgba[3];
  const uint8_t srcGray = static_cast<uint8_t>((77U * static_cast<uint32_t>(rgba[0]) +
                                                150U * static_cast<uint32_t>(rgba[1]) +
                                                29U * static_cast<uint32_t>(rgba[2])) >>
                                               8U);
  const uint8_t out     = static_cast<uint8_t>(
      (static_cast<uint32_t>(alpha) * srcGray + static_cast<uint32_t>(255U - alpha) * 255U) / 255U);

  for (uint32_t yy = y; yy < y + h; ++yy) {
    if (yy >= static_cast<uint32_t>(ctx->image->height)) continue;
    for (uint32_t xx = x; xx < x + w; ++xx) {
      if (xx >= static_cast<uint32_t>(ctx->image->width)) continue;
      ctx->image
          ->pixels[static_cast<std::size_t>(yy) * static_cast<std::size_t>(ctx->image->width) +
                   static_cast<std::size_t>(xx)] = out;
    }
  }
}
#endif

auto decodeBase64(std::string_view text, std::vector<uint8_t> &out) -> bool {
  static constexpr std::array<int8_t, 256> table = [] {
    std::array<int8_t, 256> t{};
    t.fill(-1);
    for (int i = 0; i < 26; ++i) {
      t[static_cast<std::size_t>('A' + i)] = static_cast<int8_t>(i);
      t[static_cast<std::size_t>('a' + i)] = static_cast<int8_t>(26 + i);
    }
    for (int i = 0; i < 10; ++i) {
      t[static_cast<std::size_t>('0' + i)] = static_cast<int8_t>(52 + i);
    }
    t[static_cast<std::size_t>('+')] = 62;
    t[static_cast<std::size_t>('/')] = 63;
    return t;
  }();

  out.clear();
  out.reserve((text.size() * 3) / 4 + 3);

  uint32_t acc = 0;
  int32_t bits = 0;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) continue;
    if (ch == '=') break;
    const int8_t value = table[static_cast<uint8_t>(ch)];
    if (value < 0) return false;

    acc = (acc << 6U) | static_cast<uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xffU));
    }
  }

  return !out.empty();
}

auto decodeDataUriImage(std::string_view href, std::string &mime, std::vector<uint8_t> &bytes)
    -> bool {
  const auto trimLocal = [](std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    return sv;
  };

  href = trimLocal(href);
  if (!href.starts_with("data:")) return false;

  const std::size_t comma = href.find(',');
  if (comma == std::string_view::npos) return false;

  std::string_view meta = href.substr(5, comma - 5);
  std::string_view data = href.substr(comma + 1);
  if (meta.empty() || data.empty()) return false;

  std::size_t semi = meta.find(';');
  mime             = std::string(semi == std::string_view::npos ? meta : meta.substr(0, semi));

  bool isBase64 = false;
  while (semi != std::string_view::npos) {
    meta.remove_prefix(semi + 1);
    const std::size_t nextSemi = meta.find(';');
    const std::string_view token =
        (nextSemi == std::string_view::npos) ? meta : meta.substr(0, nextSemi);
    if (token == "base64") {
      isBase64 = true;
      break;
    }
    if (nextSemi == std::string_view::npos) break;
    semi = nextSemi;
  }

  if (!isBase64) return false;
  return decodeBase64(data, bytes);
}

auto decodeJpegGray(const std::vector<uint8_t> &bytes, RasterImage &out) -> bool {
  if (bytes.empty()) return false;

  JPegDecoder decoder;
  decoder.setPixelType(JPegPixelType::EIGHT_BIT_GRAYSCALE);
  if (decoder.openRAM(const_cast<uint8_t *>(bytes.data()), static_cast<int32_t>(bytes.size()),
                      onJpegDraw) == 0)
    return false;

  out.width  = decoder.getWidth();
  out.height = decoder.getHeight();
  if (out.width <= 0 || out.height <= 0) {
    decoder.close();
    return false;
  }
  out.pixels.assign(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height),
                    0xff);

  JpegDecodeContext ctx{&out};
  decoder.setUserPointer(&ctx);
  const int ok = decoder.decode(0, 0, {});
  decoder.close();
  return ok != 0;
}

auto decodePngGray(const std::vector<uint8_t> &bytes, RasterImage &out) -> bool {
  if (bytes.empty()) return false;
  pngle_t *pngle = mypngle_new();
  if (pngle == nullptr) return false;

  PngDecodeContext ctx{&out};
  mypngle_set_user_data(pngle, &ctx);
  mypngle_set_init_callback(pngle, onPngInit);
  mypngle_set_draw_callback(pngle, onPngDraw);

  const int res = mypngle_feed(pngle, bytes.data(), bytes.size());
  const bool ok = (res >= 0 && out.width > 0 && out.height > 0 && !out.pixels.empty());
  mypngle_destroy(pngle);
  return ok;
}

// auto resolveSvgTextFontPath() -> std::string {
//   static constexpr const char *kDefaultFontPath = "SDCard/fonts/FiraSans-Regular.otf";
//   static constexpr const char *kFontsListPath   = "SDCard/fonts_list.xml";

//   pugi::xml_document fontsDoc;
//   pugi::xml_parse_result parsed = fontsDoc.load_file(kFontsListPath);
//   if (!parsed) return std::string(kDefaultFontPath);

//   pugi::xml_node root = fontsDoc.child("fonts");
//   if (!root) return std::string(kDefaultFontPath);

//   // Look for SYSTEM group and TEXT font
//   for (pugi::xml_node group = root.child("group"); group; group = group.next_sibling("group")) {
//     const char *groupName = group.attribute("name").as_string();
//     if (groupName == nullptr || std::strcmp(groupName, "SYSTEM") != 0) continue;

//     // Find TEXT font within SYSTEM group
//     for (pugi::xml_node font = group.child("font"); font; font = font.next_sibling("font")) {
//       const char *fontName = font.attribute("name").as_string();
//       if (fontName == nullptr || std::strcmp(fontName, "TEXT") != 0) continue;

//       // Get the normal variant
//       pugi::xml_node normalNode = font.child("normal");
//       if (!normalNode) break;

//       const char *filename = normalNode.attribute("filename").as_string();
//       if (filename == nullptr || *filename == '\0') break;

//       std::string path = "SDCard/fonts/";
//       path += filename;
//       return path;
//     }
//     break; // Only one SYSTEM group
//   }

//   return std::string(kDefaultFontPath);
// }

// auto getSvgRobotoFont() -> Font * {
//   static bool attempted = false;
//   static FT_Library library{nullptr};
//   static FontFaceDescriptorPtr descriptor{nullptr};
//   static FontPtr font{nullptr};

//   if (attempted) return font.get();
//   attempted = true;

//   const int ftInit = FT_Init_FreeType(&library);
//   if (ftInit != 0 || library == nullptr) return nullptr;

//   const std::string fontPath = resolveSvgTextFontPath();

//   std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
//   if (!file.is_open()) return nullptr;

//   const std::size_t size = static_cast<std::size_t>(file.tellg());
//   file.seekg(0, std::ios::beg);
//   if (size == 0) return nullptr;

//   descriptor = FontFaceDescriptor::Make();
//   if (!descriptor) return nullptr;

//   auto bytes = makeUniqueHimem<uint8_t[]>(size + 1U);
//   if (!bytes) return nullptr;

//   file.read(reinterpret_cast<char *>(bytes.get()), static_cast<std::streamsize>(size));
//   if (file.gcount() != static_cast<std::streamsize>(size)) return nullptr;
//   bytes[size] = 0;

//   descriptor->name         = "SVG-TEXT";
//   descriptor->style        = FaceStyle::NORMAL;
//   descriptor->filename     = fontPath;
//   descriptor->fontData     = std::move(bytes);
//   descriptor->fontDataSize = size;

//   font = TTF::Make(descriptor, library);
//   if (!font || !font->isReady()) {
//     font.reset();
//     return nullptr;
//   }
//   font->setPreferAntialiasing(true);

//   return font.get();
// }

struct LengthValue {
  float value{0.0};
  bool valid{false};
  bool isPercent{false};
};

struct PathSubpath {
  std::vector<std::pair<float, float>> points{};
  bool closed{false};
};

template <typename Transform>
auto applyTransform(const Transform &m, float x, float y) -> std::pair<float, float> {
  return {m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f};
}

template <typename Transform>
auto multiplyTransform(const Transform &lhs, const Transform &rhs) -> Transform {
  return {lhs.a * rhs.a + lhs.c * rhs.b,         lhs.b * rhs.a + lhs.d * rhs.b,
          lhs.a * rhs.c + lhs.c * rhs.d,         lhs.b * rhs.c + lhs.d * rhs.d,
          lhs.a * rhs.e + lhs.c * rhs.f + lhs.e, lhs.b * rhs.e + lhs.d * rhs.f + lhs.f};
}

auto parseTransformNumbers(std::string_view text, std::vector<float> &numbers) -> bool {
  numbers.clear();
  const char *p         = text.data();
  const char *const end = text.data() + text.size();

  while (p < end) {
    while (p < end && (std::isspace(static_cast<unsigned char>(*p)) || *p == ',')) ++p;
    if (p >= end) break;

    char *out         = nullptr;
    const float value = std::strtod(p, &out);
    if (out == p) return false;
    if (!std::isfinite(value)) return false;

    numbers.push_back(value);
    p = out;
  }

  return !numbers.empty();
}

template <typename Transform>
auto parseTransform(std::string_view text, Transform &out) -> bool {
  auto localTrim = [](std::string_view sv) -> std::string_view {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    return sv;
  };

  text = localTrim(text);
  if (text.empty()) return true;

  Transform result{};
  std::vector<float> numbers;

  while (!text.empty()) {
    text = localTrim(text);
    if (text.empty()) break;

    const std::size_t open  = text.find('(');
    const std::size_t close = text.find(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open) {
      return false;
    }

    std::string_view name = localTrim(text.substr(0, open));
    std::string_view args = text.substr(open + 1, close - open - 1);
    if (!parseTransformNumbers(args, numbers)) return false;

    Transform step{};
    if (name == "matrix" && numbers.size() == 6) {
      step.a = numbers[0];
      step.b = numbers[1];
      step.c = numbers[2];
      step.d = numbers[3];
      step.e = numbers[4];
      step.f = numbers[5];
    } else if (name == "translate") {
      const float tx = numbers[0];
      const float ty = (numbers.size() > 1) ? numbers[1] : 0.0;
      step.e         = tx;
      step.f         = ty;
    } else if (name == "scale") {
      const float sx = numbers[0];
      const float sy = (numbers.size() > 1) ? numbers[1] : sx;
      step.a         = sx;
      step.d         = sy;
    } else if (name == "rotate") {
      const float deg = numbers[0];
      const float rad = deg * (std::acos(-1.0) / 180.0);
      const float cs  = std::cos(rad);
      const float sn  = std::sin(rad);
      step.a          = cs;
      step.b          = sn;
      step.c          = -sn;
      step.d          = cs;
      if (numbers.size() >= 3) {
        const float cx = numbers[1];
        const float cy = numbers[2];
        step.e         = cx - cs * cx + sn * cy;
        step.f         = cy - sn * cx - cs * cy;
      }
    } else if (name == "skewx" && numbers.size() == 1) {
      const float rad = numbers[0] * (std::acos(-1.0) / 180.0);
      step.c          = std::tan(rad);
    } else if (name == "skewy" && numbers.size() == 1) {
      const float rad = numbers[0] * (std::acos(-1.0) / 180.0);
      step.b          = std::tan(rad);
    } else {
      return false;
    }

    result = multiplyTransform(result, step);

    text.remove_prefix(close + 1);
  }

  out = result;
  return true;
}

auto isMarkerLike(std::string_view value) -> bool {
  if (value.empty()) return false;

  std::string lowered(value);
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  if (lowered.find("dot") != std::string::npos) return true;
  if (lowered.find("circle") != std::string::npos) return true;
  if (lowered.find("disc") != std::string::npos) return true;
  return false;
}

using GlyphRows = std::array<const char *, 7>;

// Decode a UTF-8 byte sequence into a Unicode code point (char32_t)
// Returns the Unicode code point and advances the pointer to the next character
// end parameter prevents buffer over-reads
auto decodeUtf8Char(const uint8_t *&p, const uint8_t *end) -> char32_t {
  if (p == nullptr || p >= end || *p == 0) return ' ';

  char32_t charcode = ' ';

  // 1-byte ASCII (0xxxxxxx)
  if ((*p & 0x80) == 0x00) {
    charcode = static_cast<char32_t>(*p);
    ++p;
  }
  // 4-byte UTF-8 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
  else if (((*p & 0xF8) == 0xF0) && (p + 4 <= end) && ((p[1] & 0xC0) == 0x80) &&
           ((p[2] & 0xC0) == 0x80) && ((p[3] & 0xC0) == 0x80)) {
    charcode = (static_cast<char32_t>(p[0]) & 0x07);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[1]) & 0x3F);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[2]) & 0x3F);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[3]) & 0x3F);
    p += 4;
  }
  // 3-byte UTF-8 (1110xxxx 10xxxxxx 10xxxxxx)
  else if (((*p & 0xF0) == 0xE0) && (p + 3 <= end) && ((p[1] & 0xC0) == 0x80) &&
           ((p[2] & 0xC0) == 0x80)) {
    charcode = (static_cast<char32_t>(p[0]) & 0x0F);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[1]) & 0x3F);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[2]) & 0x3F);
    p += 3;
  }
  // 2-byte UTF-8 (110xxxxx 10xxxxxx)
  else if (((*p & 0xE0) == 0xC0) && (p + 2 <= end) && ((p[1] & 0xC0) == 0x80)) {
    charcode = (static_cast<char32_t>(p[0]) & 0x1F);
    charcode = (charcode << 6) + (static_cast<char32_t>(p[1]) & 0x3F);
    p += 2;
  }
  // Invalid or malformed sequence: consume one byte and return space
  else {
    ++p;
  }

  return charcode;
}

auto decodeEntityChar(const uint8_t *&p, const uint8_t *end) -> char32_t {
  if (p == nullptr || p >= end || *p != '&') return decodeUtf8Char(p, end);

  const uint8_t *name      = p + 1;
  const uint8_t *entityEnd = name;
  std::size_t len          = 0;
  while (len < 16U && *entityEnd != 0 && *entityEnd != ';') {
    ++entityEnd;
    ++len;
  }

  if (*entityEnd != ';' || len == 0U) {
    ++p;
    return '&';
  }

  std::string_view entity(reinterpret_cast<const char *>(name), len);
  p = entityEnd + 1;

  if (entity == "amp") return '&';
  if (entity == "lt") return '<';
  if (entity == "gt") return '>';
  if (entity == "quot") return '"';
  if (entity == "apos") return '\'';
  if (entity == "nbsp") return 0x00A0;

  if (!entity.empty() && entity.front() == '#') {
    uint32_t value          = 0;
    const bool hex          = (entity.size() > 2U) && (entity[1] == 'x' || entity[1] == 'X');
    const std::size_t start = hex ? 2U : 1U;
    if (start >= entity.size()) return '&';

    for (std::size_t i = start; i < entity.size(); ++i) {
      const unsigned char c = static_cast<unsigned char>(entity[i]);
      if (hex) {
        if (c >= '0' && c <= '9')
          value = (value << 4U) + static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
          value = (value << 4U) + static_cast<uint32_t>(10 + c - 'a');
        else if (c >= 'A' && c <= 'F')
          value = (value << 4U) + static_cast<uint32_t>(10 + c - 'A');
        else
          return '&';
      } else {
        if (c < '0' || c > '9') return '&';
        value = value * 10U + static_cast<uint32_t>(c - '0');
      }
      if (value > 0x10FFFFU) return '&';
    }
    return static_cast<char32_t>(value);
  }

  return '&';
}

auto countUtf8Codepoints(std::string_view text) -> std::size_t {
  const uint8_t *p         = reinterpret_cast<const uint8_t *>(text.data());
  const uint8_t *const end = p + text.size();

  std::size_t count = 0;
  while (p < end) {
    const uint8_t *cursor = p;
    const char32_t ch     = decodeEntityChar(cursor, end);
    if (cursor <= p) {
      ++p;
      continue;
    }
    p = cursor;
    (void)ch;
    ++count;
  }

  return count;
}

auto glyphForChar(char c) -> const GlyphRows & {
  static const GlyphRows kUnknown = {" ### ", "#   #", "   # ", "  #  ", "  #  ", "     ", "  #  "};
  static const GlyphRows kSpace   = {"     ", "     ", "     ", "     ", "     ", "     ", "     "};
  static const GlyphRows kDot     = {"     ", "     ", "     ", "     ", "     ", " ### ", " ### "};
  static const GlyphRows kComma   = {"     ", "     ", "     ", "     ", " ### ", " ### ", "  #  "};
  static const GlyphRows kDash    = {"     ", "     ", "     ", " ### ", "     ", "     ", "     "};
  static const GlyphRows kSlash   = {"    #", "   # ", "   # ", "  #  ", " #   ", " #   ", "#    "};
  static const GlyphRows kColon   = {"     ", " ### ", " ### ", "     ", " ### ", " ### ", "     "};
  static const GlyphRows kLParen  = {"   # ", "  #  ", " #   ", " #   ", " #   ", "  #  ", "   # "};
  static const GlyphRows kRParen  = {" #   ", "  #  ", "   # ", "   # ", "   # ", "  #  ", " #   "};

  static const GlyphRows k0 = {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "};
  static const GlyphRows k1 = {"  #  ", " ##  ", "# #  ", "  #  ", "  #  ", "  #  ", "#####"};
  static const GlyphRows k2 = {" ### ", "#   #", "    #", "  ## ", " #   ", "#    ", "#####"};
  static const GlyphRows k3 = {" ### ", "#   #", "    #", " ### ", "    #", "#   #", " ### "};
  static const GlyphRows k4 = {"   ##", "  # #", " #  #", "#   #", "#####", "    #", "    #"};
  static const GlyphRows k5 = {"#####", "#    ", "#    ", "#### ", "    #", "#   #", " ### "};
  static const GlyphRows k6 = {" ### ", "#   #", "#    ", "#### ", "#   #", "#   #", " ### "};
  static const GlyphRows k7 = {"#####", "    #", "   # ", "  #  ", "  #  ", "  #  ", "  #  "};
  static const GlyphRows k8 = {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "};
  static const GlyphRows k9 = {" ### ", "#   #", "#   #", " ####", "    #", "#   #", " ### "};

  static const GlyphRows kA = {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"};
  static const GlyphRows kB = {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "};
  static const GlyphRows kC = {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "};
  static const GlyphRows kD = {"#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "};
  static const GlyphRows kE = {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"};
  static const GlyphRows kF = {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "};
  static const GlyphRows kG = {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "};
  static const GlyphRows kH = {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"};
  static const GlyphRows kI = {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"};
  static const GlyphRows kJ = {"#####", "    #", "    #", "    #", "    #", "#   #", " ### "};
  static const GlyphRows kK = {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"};
  static const GlyphRows kL = {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"};
  static const GlyphRows kM = {"#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"};
  static const GlyphRows kN = {"#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"};
  static const GlyphRows kO = {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "};
  static const GlyphRows kP = {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "};
  static const GlyphRows kQ = {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"};
  static const GlyphRows kR = {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"};
  static const GlyphRows kS = {" ### ", "#   #", "#    ", " ### ", "    #", "#   #", " ### "};
  static const GlyphRows kT = {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "};
  static const GlyphRows kU = {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "};
  static const GlyphRows kV = {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "};
  static const GlyphRows kW = {"#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #"};
  static const GlyphRows kX = {"#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"};
  static const GlyphRows kY = {"#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "};
  static const GlyphRows kZ = {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"};

  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
  switch (c) {
  case ' ':
    return kSpace;
  case '.':
    return kDot;
  case ',':
    return kComma;
  case '-':
    return kDash;
  case '/':
    return kSlash;
  case ':':
    return kColon;
  case '(':
    return kLParen;
  case ')':
    return kRParen;
  case '0':
    return k0;
  case '1':
    return k1;
  case '2':
    return k2;
  case '3':
    return k3;
  case '4':
    return k4;
  case '5':
    return k5;
  case '6':
    return k6;
  case '7':
    return k7;
  case '8':
    return k8;
  case '9':
    return k9;
  case 'A':
    return kA;
  case 'B':
    return kB;
  case 'C':
    return kC;
  case 'D':
    return kD;
  case 'E':
    return kE;
  case 'F':
    return kF;
  case 'G':
    return kG;
  case 'H':
    return kH;
  case 'I':
    return kI;
  case 'J':
    return kJ;
  case 'K':
    return kK;
  case 'L':
    return kL;
  case 'M':
    return kM;
  case 'N':
    return kN;
  case 'O':
    return kO;
  case 'P':
    return kP;
  case 'Q':
    return kQ;
  case 'R':
    return kR;
  case 'S':
    return kS;
  case 'T':
    return kT;
  case 'U':
    return kU;
  case 'V':
    return kV;
  case 'W':
    return kW;
  case 'X':
    return kX;
  case 'Y':
    return kY;
  case 'Z':
    return kZ;
  default:
    return kUnknown;
  }
}

auto chooseScale(const SvgOptions &options) -> int32_t {
  if (options.test(SVG_OPTION(SCALE_EIGHTH))) return 8;
  if (options.test(SVG_OPTION(SCALE_QUARTER))) return 4;
  if (options.test(SVG_OPTION(SCALE_HALF))) return 2;
  return 1;
}

auto trim(std::string_view sv) -> std::string_view {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
  return sv;
}

auto appendMixedText(const pugi::xml_node &node, std::string &out) -> void {
  for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
    if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
      out += child.value();
      continue;
    }

    if (child.type() == pugi::node_element) appendMixedText(child, out);
  }
}

auto parseFloat(const std::string_view text, float &value) -> bool {
  const auto t = trim(text);
  if (t.empty()) return false;

  char *end = nullptr;
  value     = std::strtod(t.data(), &end);
  if (end == t.data()) return false;
  while (*end != '\0') {
    if (!std::isspace(static_cast<unsigned char>(*end))) return false;
    ++end;
  }
  return std::isfinite(value);
}

auto parseLength(std::string_view text) -> LengthValue {
  LengthValue out{};

  text = trim(text);
  if (text.empty()) return out;

  char *end     = nullptr;
  const float v = std::strtod(text.data(), &end);
  if (end == text.data()) return out;
  if (!std::isfinite(v)) return out;

  out.value = v;
  out.valid = true;

  std::string_view suffix(end, text.data() + text.size() - end);
  suffix = trim(suffix);
  if (!suffix.empty() && suffix.front() == '%') {
    out.isPercent = true;
  }

  return out;
}

auto parseViewBox(const char *value, float &vx, float &vy, float &vw, float &vh) -> bool {
  if (value == nullptr || *value == '\0') return false;

  std::vector<float> nums;
  const char *p = value;
  while (*p != '\0') {
    while (*p != '\0' && (std::isspace(static_cast<unsigned char>(*p)) || *p == ',')) ++p;
    if (*p == '\0') break;

    char *end     = nullptr;
    const float v = std::strtod(p, &end);
    if (end == p) return false;
    if (!std::isfinite(v)) return false;

    nums.push_back(v);
    p = end;
  }

  if (nums.size() != 4) return false;

  vx = nums[0];
  vy = nums[1];
  vw = nums[2];
  vh = nums[3];
  return vw > 0.0 && vh > 0.0;
}

struct PreserveAspectRatio {
  bool alignNone{false};
  bool slice{false};
  int alignX{1};
  int alignY{1};
};

auto parsePreserveAspectRatio(const char *value, PreserveAspectRatio &out) -> bool {
  out = PreserveAspectRatio{};
  if (value == nullptr || *value == '\0') return true;

  std::string_view text = trim(value);
  if (text.empty()) return true;

  std::array<std::string_view, 3> tokens{};
  std::size_t tokenCount = 0;
  while (!text.empty() && tokenCount < tokens.size()) {
    std::size_t split = 0;
    while (split < text.size() && !std::isspace(static_cast<unsigned char>(text[split]))) ++split;
    tokens[tokenCount++] = text.substr(0, split);
    text.remove_prefix(split);
    text = trim(text);
  }

  std::size_t idx = 0;
  if (idx < tokenCount && tokens[idx] == "defer") ++idx;
  if (idx >= tokenCount) return true;

  const std::string_view align = tokens[idx++];
  if (align == "none") {
    out.alignNone = true;
  } else {
    if (align.rfind("xMin", 0) == 0) {
      out.alignX = 0;
    } else if (align.rfind("xMid", 0) == 0) {
      out.alignX = 1;
    } else if (align.rfind("xMax", 0) == 0) {
      out.alignX = 2;
    } else {
      return false;
    }

    const std::size_t yPos = align.find("Y");
    if (yPos == std::string_view::npos) return false;
    const std::string_view yPart = align.substr(yPos);
    if (yPart == "YMin") {
      out.alignY = 0;
    } else if (yPart == "YMid") {
      out.alignY = 1;
    } else if (yPart == "YMax") {
      out.alignY = 2;
    } else {
      return false;
    }
  }

  if (idx < tokenCount) {
    const std::string_view mode = tokens[idx];
    if (mode == "slice") {
      out.slice = true;
    } else if (mode != "meet") {
      return false;
    }
  }
  return true;
}

auto parseColorValue(std::string_view text, uint8_t &gray, bool &enabled) -> bool {
  text = trim(text);
  if (text.empty()) return false;

  if (text == "none") {
    enabled = false;
    return true;
  }

  enabled = true;

  if (text == "black") {
    gray = 0x00;
    return true;
  }
  if (text == "white") {
    gray = 0xff;
    return true;
  }

  if (text.size() == 4 && text.front() == '#') {
    auto hexTo = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + c - 'a';
      if (c >= 'A' && c <= 'F') return 10 + c - 'A';
      return -1;
    };

    const int r = hexTo(text[1]);
    const int g = hexTo(text[2]);
    const int b = hexTo(text[3]);
    if (r < 0 || g < 0 || b < 0) return false;

    const uint8_t rr = static_cast<uint8_t>(r * 17);
    const uint8_t gg = static_cast<uint8_t>(g * 17);
    const uint8_t bb = static_cast<uint8_t>(b * 17);
    gray             = static_cast<uint8_t>((77U * rr + 150U * gg + 29U * bb) >> 8U);
    return true;
  }

  if (text.size() == 7 && text.front() == '#') {
    unsigned int rgb = 0;
    if (std::sscanf(std::string(text).c_str(), "#%06x", &rgb) != 1) return false;

    const uint8_t r = static_cast<uint8_t>((rgb >> 16U) & 0xffU);
    const uint8_t g = static_cast<uint8_t>((rgb >> 8U) & 0xffU);
    const uint8_t b = static_cast<uint8_t>(rgb & 0xffU);
    gray            = static_cast<uint8_t>((77U * r + 150U * g + 29U * b) >> 8U);
    return true;
  }

  if (text.rfind("rgb(", 0) == 0 && text.back() == ')') {
    std::string inside(text.substr(4, text.size() - 5));
    std::replace(inside.begin(), inside.end(), ',', ' ');

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (std::sscanf(inside.c_str(), "%f %f %f", &r, &g, &b) != 3) return false;

    r = std::clamp(r, 0.0f, 255.0f);
    g = std::clamp(g, 0.0f, 255.0f);
    b = std::clamp(b, 0.0f, 255.0f);

    gray = static_cast<uint8_t>((77U * static_cast<uint8_t>(r) + 150U * static_cast<uint8_t>(g) +
                                 29U * static_cast<uint8_t>(b)) >>
                                8U);
    return true;
  }

  return false;
}

auto parseOpacity(std::string_view text, uint8_t &alpha) -> bool {
  float v = 0.0;
  if (!parseFloat(text, v)) return false;

  if (v <= 0.0) {
    alpha = 0;
    return true;
  }
  if (v >= 1.0) {
    alpha = 0xff;
    return true;
  }

  alpha = static_cast<uint8_t>(std::lround(v * 255.0));
  return true;
}

auto parsePoints(std::string_view text, std::vector<std::pair<float, float>> &points) -> bool {
  points.clear();

  std::vector<float> values;
  const char *p         = text.data();
  const char *const end = text.data() + text.size();

  while (p < end) {
    while (p < end && (std::isspace(static_cast<unsigned char>(*p)) || *p == ',')) ++p;
    if (p >= end) break;

    char *out     = nullptr;
    const float v = std::strtod(p, &out);
    if (out == p) return false;
    if (!std::isfinite(v)) return false;

    values.push_back(v);
    p = out;
  }

  if (values.size() < 4 || (values.size() % 2) != 0) return false;

  points.reserve(values.size() / 2);
  for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
    points.emplace_back(values[i], values[i + 1]);
  }

  return true;
}

auto skipPathSeparators(const char *&p, const char *end) -> void {
  while (p < end && (std::isspace(static_cast<unsigned char>(*p)) || *p == ',')) ++p;
}

auto parsePathNumber(const char *&p, const char *end, float &value) -> bool {
  skipPathSeparators(p, end);
  if (p >= end) return false;

  if (std::isalpha(static_cast<unsigned char>(*p))) return false;

  char *out = nullptr;
  value     = std::strtod(p, &out);
  if (out == p || out > end || !std::isfinite(value)) return false;
  p = out;
  return true;
}

auto isPathCommand(char c) -> bool {
  switch (static_cast<char>(std::toupper(static_cast<unsigned char>(c)))) {
  case 'M':
  case 'L':
  case 'H':
  case 'V':
  case 'C':
  case 'S':
  case 'Q':
  case 'T':
  case 'A':
  case 'Z':
    return true;
  default:
    return false;
  }
}

auto flattenQuadratic(std::vector<std::pair<float, float>> &out, float x0, float y0, float cx,
                      float cy, float x1, float y1) -> void {
  const float len = std::hypot(cx - x0, cy - y0) + std::hypot(x1 - cx, y1 - cy);
  const int steps = std::clamp(static_cast<int>(std::lround(len / 8.0)), 8, 48);

  for (int i = 1; i <= steps; ++i) {
    const float t  = static_cast<float>(i) / static_cast<float>(steps);
    const float mt = 1.0 - t;
    const float x  = mt * mt * x0 + 2.0 * mt * t * cx + t * t * x1;
    const float y  = mt * mt * y0 + 2.0 * mt * t * cy + t * t * y1;
    out.emplace_back(x, y);
  }
}

auto flattenCubic(std::vector<std::pair<float, float>> &out, float x0, float y0, float c1x,
                  float c1y, float c2x, float c2y, float x1, float y1) -> void {
  const float len = std::hypot(c1x - x0, c1y - y0) + std::hypot(c2x - c1x, c2y - c1y) +
                    std::hypot(x1 - c2x, y1 - c2y);
  const int steps = std::clamp(static_cast<int>(std::lround(len / 8.0)), 12, 64);

  for (int i = 1; i <= steps; ++i) {
    const float t  = static_cast<float>(i) / static_cast<float>(steps);
    const float mt = 1.0 - t;
    const float x =
        mt * mt * mt * x0 + 3.0 * mt * mt * t * c1x + 3.0 * mt * t * t * c2x + t * t * t * x1;
    const float y =
        mt * mt * mt * y0 + 3.0 * mt * mt * t * c1y + 3.0 * mt * t * t * c2y + t * t * t * y1;
    out.emplace_back(x, y);
  }
}

auto blendGray(uint8_t dst, uint8_t src, uint8_t alpha) -> uint8_t {
  if (alpha == 0) return dst;
  if (alpha == 0xff) return src;

  const uint32_t s = static_cast<uint32_t>(src) * static_cast<uint32_t>(alpha);
  const uint32_t d = static_cast<uint32_t>(dst) * static_cast<uint32_t>(0xffU - alpha);
  return static_cast<uint8_t>((s + d + 127U) / 255U);
}

auto collectNumbers(std::string_view text, std::vector<float> &numbers) -> void {
  numbers.clear();
  const char *p         = text.data();
  const char *const end = text.data() + text.size();

  while (p < end) {
    while (p < end &&
           !(std::isdigit(static_cast<unsigned char>(*p)) || *p == '-' || *p == '+' || *p == '.')) {
      ++p;
    }
    if (p >= end) break;

    char *out         = nullptr;
    const float value = std::strtod(p, &out);
    if (out == p) {
      ++p;
      continue;
    }
    if (std::isfinite(value)) numbers.push_back(value);
    p = out;
  }
}

auto updateBounds(float x, float y, float &minX, float &minY, float &maxX, float &maxY, bool &seen)
    -> void {
  if (!std::isfinite(x) || !std::isfinite(y)) return;
  if (!seen) {
    minX = maxX = x;
    minY = maxY = y;
    seen        = true;
    return;
  }
  minX = std::min(minX, x);
  minY = std::min(minY, y);
  maxX = std::max(maxX, x);
  maxY = std::max(maxY, y);
}

auto inferBoundsFromNode(const pugi::xml_node &node, float &minX, float &minY, float &maxX,
                         float &maxY, bool &seen) -> void {
  std::vector<float> nums;

  auto name               = std::string_view(node.name());
  const std::size_t colon = name.find(':');
  if (colon != std::string_view::npos) name.remove_prefix(colon + 1);

  if (name == "path") {
    const auto d = node.attribute("d").as_string();
    collectNumbers(d, nums);
    for (std::size_t i = 0; i + 1 < nums.size(); i += 2) {
      updateBounds(nums[i], nums[i + 1], minX, minY, maxX, maxY, seen);
    }
  } else if (name == "polyline" || name == "polygon") {
    const auto points = node.attribute("points").as_string();
    collectNumbers(points, nums);
    for (std::size_t i = 0; i + 1 < nums.size(); i += 2) {
      updateBounds(nums[i], nums[i + 1], minX, minY, maxX, maxY, seen);
    }
  } else if (name == "line") {
    const auto x1 = parseLength(node.attribute("x1").as_string());
    const auto y1 = parseLength(node.attribute("y1").as_string());
    const auto x2 = parseLength(node.attribute("x2").as_string());
    const auto y2 = parseLength(node.attribute("y2").as_string());
    if (x1.valid && y1.valid && x2.valid && y2.valid) {
      updateBounds(x1.value, y1.value, minX, minY, maxX, maxY, seen);
      updateBounds(x2.value, y2.value, minX, minY, maxX, maxY, seen);
    }
  } else if (name == "rect") {
    const auto x = parseLength(node.attribute("x").as_string("0"));
    const auto y = parseLength(node.attribute("y").as_string("0"));
    const auto w = parseLength(node.attribute("width").as_string());
    const auto h = parseLength(node.attribute("height").as_string());
    if (w.valid && h.valid && w.value > 0.0 && h.value > 0.0) {
      const float rx = x.valid ? x.value : 0.0;
      const float ry = y.valid ? y.value : 0.0;
      updateBounds(rx, ry, minX, minY, maxX, maxY, seen);
      updateBounds(rx + w.value, ry + h.value, minX, minY, maxX, maxY, seen);
    }
  } else if (name == "circle") {
    const auto cx = parseLength(node.attribute("cx").as_string());
    const auto cy = parseLength(node.attribute("cy").as_string());
    const auto r  = parseLength(node.attribute("r").as_string());
    if (cx.valid && cy.valid && r.valid && r.value > 0.0) {
      updateBounds(cx.value - r.value, cy.value - r.value, minX, minY, maxX, maxY, seen);
      updateBounds(cx.value + r.value, cy.value + r.value, minX, minY, maxX, maxY, seen);
    }
  } else if (name == "ellipse") {
    const auto cx = parseLength(node.attribute("cx").as_string());
    const auto cy = parseLength(node.attribute("cy").as_string());
    const auto rx = parseLength(node.attribute("rx").as_string());
    const auto ry = parseLength(node.attribute("ry").as_string());
    if (cx.valid && cy.valid && rx.valid && ry.valid && rx.value > 0.0 && ry.value > 0.0) {
      updateBounds(cx.value - rx.value, cy.value - ry.value, minX, minY, maxX, maxY, seen);
      updateBounds(cx.value + rx.value, cy.value + ry.value, minX, minY, maxX, maxY, seen);
    }
  } else if (name == "text") {
    const auto lx             = parseLength(node.attribute("x").as_string("0"));
    const auto ly             = parseLength(node.attribute("y").as_string("0"));
    const auto ls             = parseLength(node.attribute("font-size").as_string("10"));
    const float x             = lx.valid ? lx.value : 0.0;
    const float y             = ly.valid ? ly.value : 0.0;
    const float fs            = (ls.valid && ls.value > 0.0) ? ls.value : 10.0;
    const std::size_t textLen = std::strlen(node.child_value());
    const float w             = std::max(1.0f, static_cast<float>(textLen) * fs * 0.6f);
    const float h             = fs;
    updateBounds(x, y - h, minX, minY, maxX, maxY, seen);
    updateBounds(x + w, y, minX, minY, maxX, maxY, seen);
  }

  for (const auto &child : node.children()) {
    inferBoundsFromNode(child, minX, minY, maxX, maxY, seen);
  }
}

} // namespace

auto SvgDecoder::openRAM(const uint8_t *pData, int32_t iDataSize, SvgDrawCallback pfnDraw)
    -> int32_t {
  reset();

  if (pData == nullptr || iDataSize <= 0 || pfnDraw == nullptr) {
    lastError_ = SvgError::INVALID_PARAM;
    return 0;
  }

  data_   = pData;
  size_   = iDataSize;
  drawCb_ = pfnDraw;

  const auto res = doc_.load_buffer(data_, static_cast<std::size_t>(size_));
  if (res.status != pugi::status_ok) {
    lastError_ = SvgError::PARSE_ERROR;
    return 0;
  }

  root_ = doc_.document_element();
  if (root_.empty()) {
    lastError_ = SvgError::INVALID_FILE;
    return 0;
  }

  std::string_view rootName = root_.name();
  const std::size_t colon   = rootName.find(':');
  if (colon != std::string_view::npos) rootName.remove_prefix(colon + 1);
  if (rootName != "svg") {
    lastError_ = SvgError::INVALID_FILE;
    return 0;
  }

  parseEmbeddedStyles();

  if (!parseDimensions()) return 0;

  lastError_ = SvgError::SUCCESS;
  return 1;
}

auto SvgDecoder::close() -> void { reset(); }

auto SvgDecoder::decode(int32_t x, int32_t y, SvgOptions iOptions) -> int32_t {
  if (root_.empty() || drawCb_ == nullptr || width_ <= 0 || height_ <= 0) {
    lastError_ = SvgError::INVALID_PARAM;
    return 0;
  }

  const int32_t scale = chooseScale(iOptions);
  outputWidth_        = (width_ + scale - 1) / scale;
  outputHeight_       = (height_ + scale - 1) / scale;

  if (outputWidth_ <= 0 || outputHeight_ <= 0) {
    lastError_ = SvgError::DECODE_ERROR;
    return 0;
  }

  const std::size_t pixelCount =
      static_cast<std::size_t>(outputWidth_) * static_cast<std::size_t>(outputHeight_);

  bitmap_ = makeUniqueHimem<uint8_t[]>(pixelCount);
  if (bitmap_ == nullptr) {
    lastError_ = SvgError::DECODE_ERROR;
    return 0;
  }
  std::memset(bitmap_.get(), 0xff, pixelCount);

  if (!renderDocument(scale)) {
    lastError_ = SvgError::DECODE_ERROR;
    bitmap_.reset();
    return 0;
  }

  if (!flushChunks(x, y)) {
    lastError_ = SvgError::DECODE_ERROR;
    bitmap_.reset();
    return 0;
  }

  lastError_ = SvgError::SUCCESS;
  return 1;
}

auto SvgDecoder::setUserPointer(void *p) -> void { user_ = p; }

auto SvgDecoder::getLastError() const -> SvgError { return lastError_; }

auto SvgDecoder::getWidth() const -> int32_t { return width_; }

auto SvgDecoder::getHeight() const -> int32_t { return height_; }

auto SvgDecoder::getOutputWidth() const -> int32_t { return outputWidth_; }

auto SvgDecoder::getOutputHeight() const -> int32_t { return outputHeight_; }

auto SvgDecoder::reset() -> void {
  data_            = nullptr;
  size_            = 0;
  drawCb_          = nullptr;
  user_            = nullptr;
  width_           = 0;
  height_          = 0;
  outputWidth_     = 0;
  outputHeight_    = 0;
  hasViewBox_      = false;
  viewX_           = 0.0;
  viewY_           = 0.0;
  viewW_           = 0.0;
  viewH_           = 0.0;
  viewportScaleX_  = 1.0;
  viewportScaleY_  = 1.0;
  viewportOffsetX_ = 0.0;
  viewportOffsetY_ = 0.0;
  bitmap_.reset();
  doc_.reset();
  root_ = pugi::xml_node();
  classStyles_.clear();
  lastError_ = SvgError::SUCCESS;
}

auto SvgDecoder::parseEmbeddedStyles() -> void { classStyles_.clear(); }

auto SvgDecoder::getStyleProperty(const pugi::xml_node &node, std::string_view attrName,
                                  bool includeClass) const -> std::string {
  const std::string style = node.attribute("style").as_string();
  if (!style.empty()) {
    const std::string key = std::string(attrName) + ":";
    const std::size_t pos = style.find(key);
    if (pos != std::string::npos) {
      std::size_t start = pos + key.size();
      while (start < style.size() && std::isspace(static_cast<unsigned char>(style[start]))) {
        ++start;
      }
      std::size_t end = style.find(';', start);
      if (end == std::string::npos) end = style.size();
      while (end > start && std::isspace(static_cast<unsigned char>(style[end - 1]))) --end;
      if (end > start) return style.substr(start, end - start);
    }
  }

  // Check for direct attribute fallback
  if (const auto attr = node.attribute(attrName.data())) {
    const std::string value = attr.as_string();
    if (!value.empty()) return value;
  }

  // Include class-based styles if requested
  if (!includeClass) return {};

  const std::string classList = node.attribute("class").as_string();
  if (classList.empty() || classStyles_.empty()) return {};

  std::string resolved;
  std::size_t pos = 0;
  while (pos < classList.size()) {
    while (pos < classList.size() && std::isspace(static_cast<unsigned char>(classList[pos]))) {
      ++pos;
    }
    if (pos >= classList.size()) break;

    std::size_t end = pos;
    while (end < classList.size() && !std::isspace(static_cast<unsigned char>(classList[end]))) {
      ++end;
    }
    const std::string cls = classList.substr(pos, end - pos);
    if (!cls.empty()) {
      const auto it = classStyles_.find(cls);
      if (it != classStyles_.end()) {
        const auto propIt = it->second.find(std::string(attrName));
        if (propIt != it->second.end()) resolved = propIt->second;
      }
    }

    pos = end;
  }

  return resolved;
}

auto SvgDecoder::parseDimensions() -> bool {
  const auto widthAttr  = parseLength(root_.attribute("width").as_string());
  const auto heightAttr = parseLength(root_.attribute("height").as_string());

  float viewX = 0.0;
  float viewY = 0.0;
  float viewW = 0.0;
  float viewH = 0.0;
  const bool hasViewBox =
      parseViewBox(root_.attribute("viewBox").as_string(), viewX, viewY, viewW, viewH);

  float w = 0.0;
  float h = 0.0;

  if (widthAttr.valid && !widthAttr.isPercent && heightAttr.valid && !heightAttr.isPercent) {
    w           = widthAttr.value;
    h           = heightAttr.value;
    hasViewBox_ = hasViewBox;
    if (hasViewBox_) {
      viewX_ = viewX;
      viewY_ = viewY;
      viewW_ = viewW;
      viewH_ = viewH;
    }
  } else if (hasViewBox) {
    w           = viewW;
    h           = viewH;
    hasViewBox_ = true;
    viewX_      = viewX;
    viewY_      = viewY;
    viewW_      = viewW;
    viewH_      = viewH;
  } else {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    bool seen  = false;
    inferBoundsFromNode(root_, minX, minY, maxX, maxY, seen);
    if (seen && maxX > minX && maxY > minY) {
      w = maxX - minX;
      h = maxY - minY;
    }
  }

  if (!(w > 0.0 && h > 0.0)) {
    lastError_ = SvgError::INVALID_FILE;
    return false;
  }

  width_  = std::max(static_cast<int32_t>(1), static_cast<int32_t>(std::lround(w)));
  height_ = std::max(static_cast<int32_t>(1), static_cast<int32_t>(std::lround(h)));

  if (!hasViewBox_) {
    viewX_ = 0.0;
    viewY_ = 0.0;
    viewW_ = static_cast<float>(width_);
    viewH_ = static_cast<float>(height_);
  }

  if (viewW_ <= 0.0 || viewH_ <= 0.0) {
    viewportScaleX_  = 1.0;
    viewportScaleY_  = 1.0;
    viewportOffsetX_ = 0.0;
    viewportOffsetY_ = 0.0;
    return true;
  }

  const float sx = static_cast<float>(width_) / viewW_;
  const float sy = static_cast<float>(height_) / viewH_;

  PreserveAspectRatio par{};
  parsePreserveAspectRatio(root_.attribute("preserveAspectRatio").as_string(), par);

  if (par.alignNone) {
    viewportScaleX_  = sx;
    viewportScaleY_  = sy;
    viewportOffsetX_ = -viewX_ * viewportScaleX_;
    viewportOffsetY_ = -viewY_ * viewportScaleY_;
    return true;
  }

  const float s   = par.slice ? std::max(sx, sy) : std::min(sx, sy);
  viewportScaleX_ = s;
  viewportScaleY_ = s;

  const float mappedW = viewW_ * s;
  const float mappedH = viewH_ * s;

  const float extraX = static_cast<float>(width_) - mappedW;
  const float extraY = static_cast<float>(height_) - mappedH;

  const float alignX = (par.alignX == 0) ? 0.0 : (par.alignX == 1 ? extraX * 0.5 : extraX);
  const float alignY = (par.alignY == 0) ? 0.0 : (par.alignY == 1 ? extraY * 0.5 : extraY);

  viewportOffsetX_ = alignX - viewX_ * s;
  viewportOffsetY_ = alignY - viewY_ * s;
  return true;
}

auto SvgDecoder::renderDocument(int32_t scale) -> bool {
  PaintState base{};
  return renderNode(root_, base, scale);
}

auto SvgDecoder::renderNode(const pugi::xml_node &node, const PaintState &parent, int32_t scale)
    -> bool {
  PaintState state = parent;

  const auto applyColorAttr = [&](const char *name, bool &enabled, uint8_t &gray) {
    const std::string attr = getStyleProperty(node, name);
    if (!attr.empty()) {
      bool tmpEnabled = enabled;
      uint8_t tmpGray = gray;
      if (parseColorValue(attr, tmpGray, tmpEnabled)) {
        enabled = tmpEnabled;
        gray    = tmpGray;
      }
    }
  };

  applyColorAttr("fill", state.fillEnabled, state.fillGray);
  applyColorAttr("stroke", state.strokeEnabled, state.strokeGray);

  {
    const std::string attr = getStyleProperty(node, "fill-opacity");
    if (!attr.empty()) {
      uint8_t alpha = state.fillAlpha;
      if (parseOpacity(attr, alpha)) state.fillAlpha = alpha;
    }
  }

  {
    const std::string attr = getStyleProperty(node, "transform");
    if (!attr.empty()) {
      PaintState::Transform local{};
      if (parseTransform(attr, local)) {
        state.transform = multiplyTransform(state.transform, local);
      }
    }
  }
  {
    const std::string attr = getStyleProperty(node, "stroke-opacity");
    if (!attr.empty()) {
      uint8_t alpha = state.strokeAlpha;
      if (parseOpacity(attr, alpha)) state.strokeAlpha = alpha;
    }
  }
  {
    const std::string attr = getStyleProperty(node, "opacity");
    if (!attr.empty()) {
      uint8_t alpha = 0xff;
      if (parseOpacity(attr, alpha)) {
        state.fillAlpha =
            static_cast<uint8_t>((static_cast<uint32_t>(state.fillAlpha) * alpha) / 255U);
        state.strokeAlpha =
            static_cast<uint8_t>((static_cast<uint32_t>(state.strokeAlpha) * alpha) / 255U);
      }
    }
  }
  {
    const std::string attr = getStyleProperty(node, "stroke-width");
    if (!attr.empty()) {
      const auto lv = parseLength(attr);
      if (lv.valid && lv.value > 0.0) state.strokeWidth = lv.value;
    }
  }
  {
    const std::string attr = getStyleProperty(node, "stroke-linecap");
    if (!attr.empty()) {
      std::string v = attr;
      for (char &ch : v) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      state.strokeLineCapRound = (trim(v) == "round");
    }
  }

  if (getStyleProperty(node, "display") == "none") {
    return true;
  }

  std::string_view name   = node.name();
  const std::size_t colon = name.find(':');
  if (colon != std::string_view::npos) name.remove_prefix(colon + 1);

  if (name == "defs" || name == "marker" || name == "symbol" || name == "clipPath" ||
      name == "mask") {
    return true;
  }

  if (name == "rect") {
    renderRect(node, state, scale);
  } else if (name == "line") {
    renderLine(node, state, scale);
  } else if (name == "circle") {
    renderCircle(node, state, scale);
  } else if (name == "ellipse") {
    renderEllipse(node, state, scale);
  } else if (name == "image") {
    renderImage(node, state, scale);
  } else if (name == "text") {
    renderText(node, state, scale);
  } else if (name == "path") {
    renderPath(node, state, scale);
  } else if (name == "polyline") {
    renderPolyline(node, state, scale, false);
  } else if (name == "polygon") {
    renderPolyline(node, state, scale, true);
  }

  for (auto child : node.children()) {
    if (!renderNode(child, state, scale)) return false;
  }

  return true;
}

auto SvgDecoder::flushChunks(int32_t x, int32_t y) -> bool {
  if (bitmap_ == nullptr || outputWidth_ <= 0 || outputHeight_ <= 0) return false;

  static constexpr int32_t kChunkRows = 16;
  SvgDraw draw{};
  draw.x      = x;
  draw.iWidth = outputWidth_;
  draw.iBpp   = 8;
  draw.pUser  = user_;

  for (int32_t row = 0; row < outputHeight_; row += kChunkRows) {
    const int32_t rows = std::min(kChunkRows, outputHeight_ - row);

    draw.y       = y + row;
    draw.iHeight = rows;
    draw.pPixels =
        bitmap_.get() + static_cast<std::size_t>(row) * static_cast<std::size_t>(outputWidth_);

    if (drawCb_(&draw) == 0) return false;
  }

  return true;
}

auto SvgDecoder::mapCoordX(float x, float y, const PaintState &state, int32_t scale) const
    -> float {
  const auto [tx, ty] = applyTransform(state.transform, x, y);
  (void)ty;
  const float v = tx * viewportScaleX_ + viewportOffsetX_;
  return v / static_cast<float>(std::max(static_cast<int32_t>(1), scale));
}

auto SvgDecoder::mapCoordY(float x, float y, const PaintState &state, int32_t scale) const
    -> float {
  const auto [tx, ty] = applyTransform(state.transform, x, y);
  (void)tx;
  const float v = ty * viewportScaleY_ + viewportOffsetY_;
  return v / static_cast<float>(std::max(static_cast<int32_t>(1), scale));
}

auto SvgDecoder::mapLenX(float len, const PaintState &state, int32_t scale) const -> float {
  const float dx = state.transform.a * len;
  const float dy = state.transform.b * len;
  const float v  = std::hypot(dx * viewportScaleX_, dy * viewportScaleY_);
  return v / static_cast<float>(std::max(static_cast<int32_t>(1), scale));
}

auto SvgDecoder::mapLenY(float len, const PaintState &state, int32_t scale) const -> float {
  const float dx = state.transform.c * len;
  const float dy = state.transform.d * len;
  const float v  = std::hypot(dx * viewportScaleX_, dy * viewportScaleY_);
  return v / static_cast<float>(std::max(static_cast<int32_t>(1), scale));
}

auto SvgDecoder::mapStrokeWidth(float strokeWidth, const PaintState &state, int32_t scale) const
    -> int32_t {
  const float avg = (std::abs(mapLenX(strokeWidth, state, scale)) +
                     std::abs(mapLenY(strokeWidth, state, scale))) *
                    0.5;
  return std::max(static_cast<int32_t>(1), static_cast<int32_t>(std::lround(avg)));
}

auto SvgDecoder::plot(int32_t x, int32_t y, uint8_t gray, uint8_t alpha) -> void {
  if (x < 0 || y < 0 || x >= outputWidth_ || y >= outputHeight_) return;
  auto *dst = bitmap_.get() + static_cast<std::size_t>(y) * static_cast<std::size_t>(outputWidth_) +
              static_cast<std::size_t>(x);
  *dst = blendGray(*dst, gray, alpha);
}

auto SvgDecoder::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t gray,
                          uint8_t alpha, int32_t thickness, bool roundCaps) -> void {
  const int32_t origX0 = x0;
  const int32_t origY0 = y0;
  const int32_t origX1 = x1;
  const int32_t origY1 = y1;

  const int32_t half = std::max(static_cast<int32_t>(0), thickness / 2);

  const auto drawRoundCap = [&](int32_t cx, int32_t cy) {
    const float radius  = std::max(0.5, static_cast<float>(thickness) * 0.5);
    const int32_t reach = static_cast<int32_t>(std::ceil(radius + 1.0));

    for (int32_t oy = -reach; oy <= reach; ++oy) {
      for (int32_t ox = -reach; ox <= reach; ++ox) {
        const float dist       = std::hypot(static_cast<float>(ox), static_cast<float>(oy));
        const float signedDist = radius - dist;
        float coverage         = 0.0;
        if (signedDist >= 1.0) {
          coverage = 1.0;
        } else if (signedDist > -1.0) {
          coverage = (signedDist + 1.0) * 0.5;
        }

        if (coverage <= 0.0) continue;
        const uint8_t capAlpha =
            static_cast<uint8_t>(std::lround(static_cast<float>(alpha) * coverage));
        if (capAlpha > 0) plot(cx + ox, cy + oy, gray, capAlpha);
      }
    }
  };

  if (roundCaps && x0 == x1 && y0 == y1) {
    drawRoundCap(x0, y0);
    return;
  }

  const bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);

  // For thick lines or axis-aligned lines, use Bresenham (no AA overhead)
  if (half > 0 || (!steep && y0 == y1) || (steep && x0 == x1)) {
    int32_t dx  = std::abs(x1 - x0);
    int32_t sx  = (x0 < x1) ? 1 : -1;
    int32_t dy  = -std::abs(y1 - y0);
    int32_t sy  = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    while (true) {
      for (int32_t oy = -half; oy <= half; ++oy) {
        for (int32_t ox = -half; ox <= half; ++ox) {
          plot(x0 + ox, y0 + oy, gray, alpha);
        }
      }
      if (x0 == x1 && y0 == y1) break;
      const int32_t e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }

    if (roundCaps) {
      drawRoundCap(origX0, origY0);
      drawRoundCap(origX1, origY1);
    }
    return;
  }

  // Xiaolin Wu antialiased line for thin diagonal lines
  auto plot_aa = [&](int32_t px, int32_t py, float brightness) {
    const uint8_t a = static_cast<uint8_t>(std::lround(alpha * brightness));
    plot(px, py, gray, a);
  };

  auto fpart  = [](float x) { return x - std::floor(x); };
  auto rfpart = [&fpart](float x) { return 1.0 - fpart(x); };

  float fx0 = static_cast<float>(x0);
  float fy0 = static_cast<float>(y0);
  float fx1 = static_cast<float>(x1);
  float fy1 = static_cast<float>(y1);

  if (steep) {
    std::swap(fx0, fy0);
    std::swap(fx1, fy1);
  }
  if (fx0 > fx1) {
    std::swap(fx0, fx1);
    std::swap(fy0, fy1);
  }

  const float fdx  = fx1 - fx0;
  const float fdy  = fy1 - fy0;
  const float grad = (fdx == 0.0) ? 1.0 : fdy / fdx;

  // First endpoint
  float xend          = std::round(fx0);
  float yend          = fy0 + grad * (xend - fx0);
  float xgap          = rfpart(fx0 + 0.5);
  const int32_t xpxl1 = static_cast<int32_t>(xend);
  const int32_t ypxl1 = static_cast<int32_t>(std::floor(yend));
  if (steep) {
    plot_aa(ypxl1, xpxl1, rfpart(yend) * xgap);
    plot_aa(ypxl1 + 1, xpxl1, fpart(yend) * xgap);
  } else {
    plot_aa(xpxl1, ypxl1, rfpart(yend) * xgap);
    plot_aa(xpxl1, ypxl1 + 1, fpart(yend) * xgap);
  }
  float intery = yend + grad;

  // Second endpoint
  xend                = std::round(fx1);
  yend                = fy1 + grad * (xend - fx1);
  xgap                = fpart(fx1 + 0.5);
  const int32_t xpxl2 = static_cast<int32_t>(xend);
  const int32_t ypxl2 = static_cast<int32_t>(std::floor(yend));
  if (steep) {
    plot_aa(ypxl2, xpxl2, rfpart(yend) * xgap);
    plot_aa(ypxl2 + 1, xpxl2, fpart(yend) * xgap);
  } else {
    plot_aa(xpxl2, ypxl2, rfpart(yend) * xgap);
    plot_aa(xpxl2, ypxl2 + 1, fpart(yend) * xgap);
  }

  // Main loop
  if (steep) {
    for (int32_t x = xpxl1 + 1; x < xpxl2; ++x) {
      plot_aa(static_cast<int32_t>(std::floor(intery)), x, rfpart(intery));
      plot_aa(static_cast<int32_t>(std::floor(intery)) + 1, x, fpart(intery));
      intery += grad;
    }
  } else {
    for (int32_t x = xpxl1 + 1; x < xpxl2; ++x) {
      plot_aa(x, static_cast<int32_t>(std::floor(intery)), rfpart(intery));
      plot_aa(x, static_cast<int32_t>(std::floor(intery)) + 1, fpart(intery));
      intery += grad;
    }
  }

  if (roundCaps) {
    drawRoundCap(origX0, origY0);
    drawRoundCap(origX1, origY1);
  }
}

auto SvgDecoder::renderRect(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  const auto lx  = parseLength(node.attribute("x").as_string("0"));
  const auto ly  = parseLength(node.attribute("y").as_string("0"));
  const auto lw  = parseLength(node.attribute("width").as_string());
  const auto lh  = parseLength(node.attribute("height").as_string());
  const auto lrx = parseLength(node.attribute("rx").as_string());
  const auto lry = parseLength(node.attribute("ry").as_string());

  if (!lw.valid || !lh.valid || lw.value <= 0.0 || lh.value <= 0.0) return;

  const float x = lx.valid ? lx.value : 0.0;
  const float y = ly.valid ? ly.value : 0.0;
  const float w = lw.value;
  const float h = lh.value;

  float rx = (lrx.valid && lrx.value > 0.0) ? lrx.value : 0.0;
  float ry = (lry.valid && lry.value > 0.0) ? lry.value : 0.0;
  if (rx > 0.0 && ry <= 0.0) ry = rx;
  if (ry > 0.0 && rx <= 0.0) rx = ry;
  rx = std::clamp(rx, 0.0f, w * 0.5f);
  ry = std::clamp(ry, 0.0f, h * 0.5f);

  const int32_t x0 = static_cast<int32_t>(std::floor(mapCoordX(x, y, state, scale)));
  const int32_t y0 = static_cast<int32_t>(std::floor(mapCoordY(x, y, state, scale)));
  const int32_t x1 = static_cast<int32_t>(std::ceil(mapCoordX(x + w, y + h, state, scale))) - 1;
  const int32_t y1 = static_cast<int32_t>(std::ceil(mapCoordY(x + w, y + h, state, scale))) - 1;

  const float rpx = std::max(0.0f, mapLenX(rx, state, scale));
  const float rpy = std::max(0.0f, mapLenY(ry, state, scale));

  const auto inRoundedRect = [](float px, float py, float left, float top, float right,
                                float bottom, float rrX, float rrY) {
    if (px < left || py < top || px > right || py > bottom) return false;
    if (rrX <= 0.0 || rrY <= 0.0) return true;

    const float rxClamped = std::min(rrX, (right - left + 1.0f) * 0.5f);
    const float ryClamped = std::min(rrY, (bottom - top + 1.0f) * 0.5f);

    const float il = left + rxClamped;
    const float ir = right - rxClamped;
    const float it = top + ryClamped;
    const float ib = bottom - ryClamped;
    if ((px >= il && px <= ir) || (py >= it && py <= ib)) return true;

    const float cx = (px < il) ? il : ir;
    const float cy = (py < it) ? it : ib;
    const float dx = (px - cx) / std::max(1e-6f, rxClamped);
    const float dy = (py - cy) / std::max(1e-6f, ryClamped);
    return (dx * dx + dy * dy) <= 1.0f;
  };

  const auto roundedRectCoverage = [&](int32_t px, int32_t py, int32_t left, int32_t top,
                                       int32_t right, int32_t bottom, float rrX,
                                       float rrY) -> float {
    // 4x4 SSAA for stable antialiasing across different figure scales.
    static constexpr int kAaGrid = 4;
    int insideCount              = 0;
    for (int sy = 0; sy < kAaGrid; ++sy) {
      for (int sx = 0; sx < kAaGrid; ++sx) {
        const float sampleX =
            static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / static_cast<float>(kAaGrid);
        const float sampleY =
            static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / static_cast<float>(kAaGrid);
        if (inRoundedRect(sampleX, sampleY, static_cast<float>(left), static_cast<float>(top),
                          static_cast<float>(right), static_cast<float>(bottom), rrX, rrY)) {
          ++insideCount;
        }
      }
    }
    return static_cast<float>(insideCount) / static_cast<float>(kAaGrid * kAaGrid);
  };

  if (state.fillEnabled && state.fillAlpha > 0) {
    for (int32_t yy = y0; yy <= y1; ++yy) {
      for (int32_t xx = x0; xx <= x1; ++xx) {
        const float coverage = roundedRectCoverage(xx, yy, x0, y0, x1, y1, rpx, rpy);
        if (coverage <= 0.0) continue;
        const uint8_t alpha =
            static_cast<uint8_t>(std::lround(static_cast<float>(state.fillAlpha) * coverage));
        if (alpha > 0) plot(xx, yy, state.fillGray, alpha);
      }
    }
  }

  if (state.strokeEnabled && state.strokeAlpha > 0) {
    const int32_t sw = mapStrokeWidth(state.strokeWidth, state, scale);
    if (rpx <= 0.0 || rpy <= 0.0) {
      drawLine(x0, y0, x1, y0, state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
      drawLine(x1, y0, x1, y1, state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
      drawLine(x1, y1, x0, y1, state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
      drawLine(x0, y1, x0, y0, state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
    } else {
      const int32_t ix0 = x0 + sw;
      const int32_t iy0 = y0 + sw;
      const int32_t ix1 = x1 - sw;
      const int32_t iy1 = y1 - sw;
      const float irx   = std::max(0.0f, rpx - sw);
      const float iry   = std::max(0.0f, rpy - sw);
      for (int32_t yy = y0; yy <= y1; ++yy) {
        for (int32_t xx = x0; xx <= x1; ++xx) {
          const float outerCoverage = roundedRectCoverage(xx, yy, x0, y0, x1, y1, rpx, rpy);
          if (outerCoverage <= 0.0) continue;

          float innerCoverage = 0.0;
          if (ix0 <= ix1 && iy0 <= iy1) {
            innerCoverage = roundedRectCoverage(xx, yy, ix0, iy0, ix1, iy1, irx, iry);
          }

          const float strokeCoverage = outerCoverage * (1.0 - innerCoverage);
          if (strokeCoverage <= 0.0) continue;

          const uint8_t alpha = static_cast<uint8_t>(
              std::lround(static_cast<float>(state.strokeAlpha) * strokeCoverage));
          if (alpha > 0) plot(xx, yy, state.strokeGray, alpha);
        }
      }
    }
  }
}

auto SvgDecoder::renderLine(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  if (!state.strokeEnabled || state.strokeAlpha == 0) return;

  const auto lx1 = parseLength(node.attribute("x1").as_string());
  const auto ly1 = parseLength(node.attribute("y1").as_string());
  const auto lx2 = parseLength(node.attribute("x2").as_string());
  const auto ly2 = parseLength(node.attribute("y2").as_string());

  if (!lx1.valid || !ly1.valid || !lx2.valid || !ly2.valid) return;

  const int32_t x1 =
      static_cast<int32_t>(std::lround(mapCoordX(lx1.value, ly1.value, state, scale)));
  const int32_t y1 =
      static_cast<int32_t>(std::lround(mapCoordY(lx1.value, ly1.value, state, scale)));
  const int32_t x2 =
      static_cast<int32_t>(std::lround(mapCoordX(lx2.value, ly2.value, state, scale)));
  const int32_t y2 =
      static_cast<int32_t>(std::lround(mapCoordY(lx2.value, ly2.value, state, scale)));

  const int32_t sw = mapStrokeWidth(state.strokeWidth, state, scale);
  drawLine(x1, y1, x2, y2, state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);

  const std::string markerStartValue = getStyleProperty(node, "marker-start");
  const std::string markerEndValue   = getStyleProperty(node, "marker-end");
  const auto markerStart             = std::string_view(markerStartValue);
  const auto markerEnd               = std::string_view(markerEndValue);
  const auto drawMarker              = [&](int32_t px, int32_t py, int32_t dx, int32_t dy,
                              std::string_view marker) {
    if (marker.empty() || marker == "none") return;
    if (dx == 0 && dy == 0) {
      dx = 1;
    }

    const float length = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
    const float ux     = static_cast<float>(dx) / length;
    const float uy     = static_cast<float>(dy) / length;
    const float pxn    = -uy;
    const float pyn    = ux;

    if (isMarkerLike(marker)) {
      const int32_t radius      = std::max(static_cast<int32_t>(1), sw);
      const auto circleCoverage = [](float dist, float r) -> float {
        const float edge       = 1.0;
        const float signedDist = r - dist;
        if (signedDist >= edge) return 1.0;
        if (signedDist <= -edge) return 0.0;
        return (signedDist + edge) * 0.5;
      };

      for (int32_t oy = -radius - 1; oy <= radius + 1; ++oy) {
        for (int32_t ox = -radius - 1; ox <= radius + 1; ++ox) {
          const float dist = std::hypot(static_cast<float>(ox), static_cast<float>(oy));
          const float cov  = circleCoverage(dist, static_cast<float>(radius));
          if (cov <= 0.0) continue;
          const uint8_t alpha =
              static_cast<uint8_t>(std::lround(static_cast<float>(state.strokeAlpha) * cov));
          if (alpha > 0) plot(px + ox, py + oy, state.strokeGray, alpha);
        }
      }
      return;
    }

    const float size   = std::max(9.0, static_cast<float>(sw) * 6.3);
    const float backX  = static_cast<float>(px) - ux * size;
    const float backY  = static_cast<float>(py) - uy * size;
    const float leftX  = backX + pxn * (size * 0.55);
    const float leftY  = backY + pyn * (size * 0.55);
    const float rightX = backX - pxn * (size * 0.55);
    const float rightY = backY - pyn * (size * 0.55);

    std::vector<std::pair<int32_t, int32_t>> triangle = {
        {px, py},
        {static_cast<int32_t>(std::lround(leftX)), static_cast<int32_t>(std::lround(leftY))},
        {static_cast<int32_t>(std::lround(rightX)), static_cast<int32_t>(std::lround(rightY))},
    };
    fillPolygon(triangle, state.strokeGray, state.strokeAlpha);
  };

  drawMarker(x1, y1, x1 - x2, y1 - y2, markerStart);
  drawMarker(x2, y2, x2 - x1, y2 - y1, markerEnd);
}

auto SvgDecoder::renderCircle(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  const auto lcx = parseLength(node.attribute("cx").as_string());
  const auto lcy = parseLength(node.attribute("cy").as_string());
  const auto lr  = parseLength(node.attribute("r").as_string());
  if (!lcx.valid || !lcy.valid || !lr.valid || lr.value <= 0.0) return;

  const float cx = mapCoordX(lcx.value, lcy.value, state, scale);
  const float cy = mapCoordY(lcx.value, lcy.value, state, scale);
  const float r =
      (std::abs(mapLenX(lr.value, state, scale)) + std::abs(mapLenY(lr.value, state, scale))) * 0.5;

  const int32_t x0 = static_cast<int32_t>(std::floor(cx - r - 1.0));
  const int32_t x1 = static_cast<int32_t>(std::ceil(cx + r + 1.0));
  const int32_t y0 = static_cast<int32_t>(std::floor(cy - r - 1.0));
  const int32_t y1 = static_cast<int32_t>(std::ceil(cy + r + 1.0));

  // Helper function to compute antialiased coverage (0.0-1.0) based on distance
  auto computeCoverage = [](float distance, float radius) -> float {
    // Soft edge: 1 pixel of antialiasing
    const float edge       = 1.0;
    const float signedDist = radius - distance;
    if (signedDist >= edge) return 1.0;  // Fully inside
    if (signedDist <= -edge) return 0.0; // Fully outside
    return (signedDist + edge) * 0.5;    // Partially inside (linear interpolation)
  };

  if (state.fillEnabled && state.fillAlpha > 0) {
    for (int32_t y = y0; y <= y1; ++y) {
      for (int32_t x = x0; x <= x1; ++x) {
        const float dx       = x - cx;
        const float dy       = y - cy;
        const float dist     = std::sqrt(dx * dx + dy * dy);
        const float coverage = computeCoverage(dist, r);
        if (coverage > 0.0) {
          const uint8_t blendedAlpha =
              static_cast<uint8_t>(std::lround(state.fillAlpha * coverage));
          plot(x, y, state.fillGray, blendedAlpha);
        }
      }
    }
  }

  if (state.strokeEnabled && state.strokeAlpha > 0) {
    const int32_t sw  = mapStrokeWidth(state.strokeWidth, state, scale);
    const float outer = (r + sw * 0.5);
    const float inner = std::max(0.0f, r - sw * 0.5f);

    for (int32_t y = y0; y <= y1; ++y) {
      for (int32_t x = x0; x <= x1; ++x) {
        const float dx   = x - cx;
        const float dy   = y - cy;
        const float dist = std::sqrt(dx * dx + dy * dy);

        // Compute coverage for outer and inner edges with antialiasing
        const float outerCoverage  = computeCoverage(dist, outer);
        const float innerCoverage  = computeCoverage(dist, inner);
        const float strokeCoverage = outerCoverage * (1.0 - innerCoverage);

        if (strokeCoverage > 0.0) {
          const uint8_t blendedAlpha =
              static_cast<uint8_t>(std::lround(state.strokeAlpha * strokeCoverage));
          plot(x, y, state.strokeGray, blendedAlpha);
        }
      }
    }
  }
}

auto SvgDecoder::renderEllipse(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  const auto lcx = parseLength(node.attribute("cx").as_string());
  const auto lcy = parseLength(node.attribute("cy").as_string());
  const auto lrx = parseLength(node.attribute("rx").as_string());
  const auto lry = parseLength(node.attribute("ry").as_string());
  if (!lcx.valid || !lcy.valid || !lrx.valid || !lry.valid || lrx.value <= 0.0 ||
      lry.value <= 0.0) {
    return;
  }

  const float cx = mapCoordX(lcx.value, lcy.value, state, scale);
  const float cy = mapCoordY(lcx.value, lcy.value, state, scale);
  const float rx = std::abs(mapLenX(lrx.value, state, scale));
  const float ry = std::abs(mapLenY(lry.value, state, scale));

  const int32_t x0 = static_cast<int32_t>(std::floor(cx - rx - 1.0));
  const int32_t x1 = static_cast<int32_t>(std::ceil(cx + rx + 1.0));
  const int32_t y0 = static_cast<int32_t>(std::floor(cy - ry - 1.0));
  const int32_t y1 = static_cast<int32_t>(std::ceil(cy + ry + 1.0));

  // Helper function to compute antialiased coverage for ellipse
  auto computeEllipseCoverage = [](float nx, float ny, float radius, bool isOuter) -> float {
    const float d2 = nx * nx + ny * ny;
    const float d  = std::sqrt(d2);
    const float edge =
        1.0f / std::min(100.0f, std::max(1.0f, std::max(std::abs(nx), std::abs(ny))));
    if (isOuter) {
      // Outer edge: coverage decreases from 1 to 0 as we go from inside to outside
      const float signedDist = 1.0f - d;
      if (signedDist >= edge) return 1.0f;
      if (signedDist <= -edge) return 0.0f;
      return (signedDist + edge) * 0.5f;
    } else {
      // Inner edge: coverage increases from 0 to 1 as we go from inside to outside
      const float signedDist = d - 1.0f;
      if (signedDist <= -edge) return 0.0f;
      if (signedDist >= edge) return 1.0f;
      return (signedDist + edge) * 0.5f;
    }
  };

  if (state.fillEnabled && state.fillAlpha > 0) {
    for (int32_t y = y0; y <= y1; ++y) {
      for (int32_t x = x0; x <= x1; ++x) {
        const float nx = (x - cx) / rx;
        const float ny = (y - cy) / ry;
        const float d2 = nx * nx + ny * ny;

        // Compute antialiased coverage
        const float coverage = computeEllipseCoverage(nx, ny, 1.0, true);
        if (coverage > 0.0 && d2 <= 2.0) { // Only process nearby pixels
          const uint8_t blendedAlpha =
              static_cast<uint8_t>(std::lround(state.fillAlpha * coverage));
          plot(x, y, state.fillGray, blendedAlpha);
        }
      }
    }
  }

  if (state.strokeEnabled && state.strokeAlpha > 0) {
    const int32_t sw    = mapStrokeWidth(state.strokeWidth, state, scale);
    const float rxOuter = rx + sw * 0.5;
    const float ryOuter = ry + sw * 0.5;
    const float rxInner = std::max(0.1, rx - sw * 0.5);
    const float ryInner = std::max(0.1, ry - sw * 0.5);

    for (int32_t y = y0; y <= y1; ++y) {
      for (int32_t x = x0; x <= x1; ++x) {
        const float nxo = (x - cx) / rxOuter;
        const float nyo = (y - cy) / ryOuter;
        const float nxi = (x - cx) / rxInner;
        const float nyi = (y - cy) / ryInner;

        // Compute antialiased coverage for outer and inner edges
        const float outerCoverage  = computeEllipseCoverage(nxo, nyo, 1.0f, true);
        const float innerCoverage  = computeEllipseCoverage(nxi, nyi, 1.0, false);
        const float strokeCoverage = outerCoverage * innerCoverage;

        if (strokeCoverage > 0.0) {
          const uint8_t blendedAlpha =
              static_cast<uint8_t>(std::lround(state.strokeAlpha * strokeCoverage));
          plot(x, y, state.strokeGray, blendedAlpha);
        }
      }
    }
  }
}

auto SvgDecoder::renderImage(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  std::string href = node.attribute("xlink:href").as_string();
  if (href.empty()) href = node.attribute("href").as_string();
  if (href.empty()) return;

  std::string mime;
  std::vector<uint8_t> imageBytes;
  if (!decodeDataUriImage(href, mime, imageBytes)) return;

  RasterImage image;
  bool decoded = false;
  if (mime == "image/png") {
    decoded = decodePngGray(imageBytes, image);
  } else if (mime == "image/jpeg" || mime == "image/jpg") {
    decoded = decodeJpegGray(imageBytes, image);
  }
  if (!decoded || image.width <= 0 || image.height <= 0 || image.pixels.empty()) return;

  auto x = parseLength(node.attribute("x").as_string());
  auto y = parseLength(node.attribute("y").as_string());
  auto w = parseLength(node.attribute("width").as_string());
  auto h = parseLength(node.attribute("height").as_string());

  if (!x.valid) x = LengthValue{0.0, true, false};
  if (!y.valid) y = LengthValue{0.0, true, false};
  if (!w.valid || w.isPercent || w.value <= 0.0)
    w = LengthValue{static_cast<float>(image.width), true, false};
  if (!h.valid || h.isPercent || h.value <= 0.0)
    h = LengthValue{static_cast<float>(image.height), true, false};

  const float x0 = x.value;
  const float y0 = y.value;
  const float x1 = x.value + w.value;
  const float y1 = y.value + h.value;

  const float dx0 = mapCoordX(x0, y0, state, scale);
  const float dy0 = mapCoordY(x0, y0, state, scale);
  const float dx1 = mapCoordX(x1, y1, state, scale);
  const float dy1 = mapCoordY(x1, y1, state, scale);

  const int32_t left   = static_cast<int32_t>(std::floor(std::min(dx0, dx1)));
  const int32_t right  = static_cast<int32_t>(std::ceil(std::max(dx0, dx1)));
  const int32_t top    = static_cast<int32_t>(std::floor(std::min(dy0, dy1)));
  const int32_t bottom = static_cast<int32_t>(std::ceil(std::max(dy0, dy1)));

  const int32_t dstW = right - left;
  const int32_t dstH = bottom - top;
  if (dstW <= 0 || dstH <= 0) return;

  const auto sampleBilinear = [&](float sx, float sy) -> uint8_t {
    sx = std::clamp(sx, 0.0f, static_cast<float>(image.width - 1));
    sy = std::clamp(sy, 0.0f, static_cast<float>(image.height - 1));

    const int32_t x0 = static_cast<int32_t>(std::floor(sx));
    const int32_t y0 = static_cast<int32_t>(std::floor(sy));
    const int32_t x1 = std::min(x0 + 1, image.width - 1);
    const int32_t y1 = std::min(y0 + 1, image.height - 1);

    const float tx = sx - static_cast<float>(x0);
    const float ty = sy - static_cast<float>(y0);

    const auto idx = [&](int32_t xx, int32_t yy) -> std::size_t {
      return static_cast<std::size_t>(yy) * static_cast<std::size_t>(image.width) +
             static_cast<std::size_t>(xx);
    };

    const float c00 = static_cast<float>(image.pixels[idx(x0, y0)]);
    const float c10 = static_cast<float>(image.pixels[idx(x1, y0)]);
    const float c01 = static_cast<float>(image.pixels[idx(x0, y1)]);
    const float c11 = static_cast<float>(image.pixels[idx(x1, y1)]);

    const float cx0 = c00 + (c10 - c00) * tx;
    const float cx1 = c01 + (c11 - c01) * tx;
    const float cxy = cx0 + (cx1 - cx0) * ty;
    return static_cast<uint8_t>(std::clamp(std::lround(cxy), 0L, 255L));
  };

  for (int32_t py = top; py < bottom; ++py) {
    if (py < 0 || py >= outputHeight_) continue;
    const float sy = ((static_cast<float>(py - top) + 0.5) * static_cast<float>(image.height) /
                      static_cast<float>(std::max(static_cast<int32_t>(1), dstH))) -
                     0.5;
    for (int32_t px = left; px < right; ++px) {
      if (px < 0 || px >= outputWidth_) continue;
      const float sx = ((static_cast<float>(px - left) + 0.5) * static_cast<float>(image.width) /
                        static_cast<float>(std::max(static_cast<int32_t>(1), dstW))) -
                       0.5;
      const uint8_t gray = sampleBilinear(sx, sy);
      plot(px, py, gray, 0xff);
    }
  }
}

auto SvgDecoder::renderText(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  if (!state.fillEnabled || state.fillAlpha == 0) return;

  const auto lx = parseLength(node.attribute("x").as_string("0"));
  const auto ly = parseLength(node.attribute("y").as_string("0"));
  auto ls       = parseLength(node.attribute("font-size").as_string());
  if (!ls.valid) {
    const std::string fsStyle = getStyleProperty(node, "font-size");
    if (!fsStyle.empty()) ls = parseLength(fsStyle);
  }
  if (!ly.valid) return;

  const float x                   = lx.valid ? lx.value : 0.0;
  const float y                   = ly.value;
  const float fontSize            = (ls.valid && ls.value > 0.0) ? ls.value : 10.0;
  const float mappedFontHeight    = std::abs(mapLenY(fontSize, state, scale));
  const float glyphH              = std::max(1.0f, mappedFontHeight * 0.75f);
  const float pixelScale          = glyphH / 7.0;
  const float adv                 = 6.0 * pixelScale;
  const std::string anchorValue   = getStyleProperty(node, "text-anchor");
  const std::string baselineValue = [&]() {
    std::string v = getStyleProperty(node, "dominant-baseline");
    if (v.empty()) v = getStyleProperty(node, "alignment-baseline");
    return v;
  }();
  const std::string_view anchor   = trim(anchorValue);
  const std::string_view baseline = trim(baselineValue);

  struct TextRunSpec {
    std::string text;
    float x{0.0};
    float y{0.0};
    float fontSize{0.0};
    std::string anchorOverride;
  };

  std::vector<TextRunSpec> runs;
  runs.reserve(8);

  bool hasPositionedTspan = false;
  float cursorX           = x;
  float cursorY           = y;

  for (const auto &child : node.children()) {
    std::string_view cname = child.name();
    const std::size_t c    = cname.find(':');
    if (c != std::string_view::npos) cname.remove_prefix(c + 1);
    if (cname != "tspan") continue;

    const bool hasPosAttr = child.attribute("y") || child.attribute("dy");
    if (!hasPosAttr) continue;
    hasPositionedTspan = true;

    const auto tx  = parseLength(child.attribute("x").as_string());
    const auto ty  = parseLength(child.attribute("y").as_string());
    const auto tdx = parseLength(child.attribute("dx").as_string());
    const auto tdy = parseLength(child.attribute("dy").as_string());

    if (tx.valid) cursorX = tx.value;
    if (ty.valid) cursorY = ty.value;
    if (tdx.valid) cursorX += tdx.value;
    if (tdy.valid) cursorY += tdy.value;

    std::string spanText;
    appendMixedText(child, spanText);
    if (spanText.empty()) continue;

    float spanFontSize = fontSize;
    if (const std::string childFontSize = getStyleProperty(child, "font-size");
        !childFontSize.empty()) {
      const auto childLs = parseLength(childFontSize);
      if (childLs.valid && childLs.value > 0.0) spanFontSize = childLs.value;
    }

    std::string tspanAnchorValue = getStyleProperty(child, "text-anchor");
    if (tspanAnchorValue.empty()) {
      tspanAnchorValue = std::string(anchor);
    } else {
      tspanAnchorValue = std::string(trim(tspanAnchorValue));
    }

    const float runX = cursorX;
    runs.push_back(
        TextRunSpec{std::move(spanText), runX, cursorY, spanFontSize, std::move(tspanAnchorValue)});
    cursorX = runX + static_cast<float>(countUtf8Codepoints(runs.back().text)) * adv;
  }

  if (!hasPositionedTspan) {
    std::string text;
    text.reserve(128);
    appendMixedText(node, text);

    text = std::string(trim(text));
    if (!text.empty())
      runs.push_back(TextRunSpec{std::move(text), x, y, fontSize, std::string(anchor)});
  }

  if (runs.empty()) return;

  const auto renderRun = [&](std::string_view text, float runX, float runY, float runFontSize,
                             std::string_view runAnchor) {
    const float baseX = mapCoordX(runX, runY, state, scale);
    const float baseY = mapCoordY(runX, runY, state, scale);

    const float runGlyphH   = std::max(1.0f, std::abs(mapLenY(runFontSize, state, scale)));
    const int16_t glyphSize = static_cast<int16_t>(std::clamp(std::lround(runGlyphH), 1L, 200L));

    struct GlyphRun {
      Glyph *glyph{nullptr};
      char32_t charcode{' '};
      int16_t advance{0};
    };

    std::vector<GlyphRun> run;
    run.reserve(text.size());
    float totalAdvance = 0.0;

    // Decode UTF-8 string and look up glyphs for each character
    const uint8_t *p   = reinterpret_cast<const uint8_t *>(text.data());
    const uint8_t *end = p + text.size();
    while (p < end) {
      char32_t charcode = decodeEntityChar(p, end);

      // Handle whitespace characters
      if (charcode == '\n' || charcode == '\r' || charcode == '\t') {
        charcode = ' ';
      }

      Glyph *glyph = font_->getGlyph(charcode, glyphSize);

      int16_t advance = 0;
      if (glyph != nullptr) {
        advance = (glyph->advance > 0) ? glyph->advance : static_cast<int16_t>(std::max(1.0f, adv));
      } else {
        advance = static_cast<int16_t>(std::max(1.0f, adv));
      }
      run.push_back(GlyphRun{glyph, charcode, advance});
      totalAdvance += static_cast<float>(advance);
    }

    float startPenX = baseX;
    if (runAnchor == "middle") {
      startPenX -= totalAdvance * 0.5;
    } else if (runAnchor == "end") {
      startPenX -= totalAdvance;
    }

    int32_t minTop       = 0;
    int32_t maxBottom    = glyphSize;
    bool hasGlyphMetrics = false;
    for (const auto &g : run) {
      if (g.glyph == nullptr) continue;
      const int32_t top    = g.glyph->yoff;
      const int32_t bottom = g.glyph->yoff + static_cast<int32_t>(g.glyph->dim.height);
      if (!hasGlyphMetrics) {
        minTop          = top;
        maxBottom       = bottom;
        hasGlyphMetrics = true;
      } else {
        minTop    = std::min(minTop, top);
        maxBottom = std::max(maxBottom, bottom);
      }
    }

    float baselineY = baseY;
    if (baseline == "middle" || baseline == "central") {
      baselineY = baseY - (static_cast<float>(minTop + maxBottom) * 0.5);
    } else if (baseline == "hanging" || baseline == "text-before-edge" ||
               baseline == "before-edge" || baseline == "top") {
      baselineY = baseY - static_cast<float>(minTop);
    } else if (baseline == "text-after-edge" || baseline == "after-edge" || baseline == "bottom") {
      baselineY = baseY - static_cast<float>(maxBottom);
    }

    float penX = startPenX;
    for (const auto &g : run) {
      if (g.glyph != nullptr && g.glyph->buffer != nullptr && g.glyph->dim.width > 0 &&
          g.glyph->dim.height > 0) {
        const int32_t gx0 =
            static_cast<int32_t>(std::lround(penX + static_cast<float>(g.glyph->xoff)));
        const int32_t gy0 =
            static_cast<int32_t>(std::lround(baselineY + static_cast<float>(g.glyph->yoff)));

        const int32_t width   = g.glyph->dim.width;
        const int32_t height  = g.glyph->dim.height;
        const int32_t pitch   = std::abs(static_cast<int32_t>(g.glyph->pitch));
        const bool monoPacked = pitch > 0 && pitch < width;

        for (int32_t gy = 0; gy < height; ++gy) {
          for (int32_t gx = 0; gx < width; ++gx) {
            uint8_t coverage = 0;
            if (!monoPacked) {
              const int32_t off = gy * pitch + gx;
              coverage          = g.glyph->buffer[off];
            } else {
              const int32_t byteOff = gy * pitch + (gx >> 3);
              const int bit         = 7 - (gx & 7);
              coverage              = (g.glyph->buffer[byteOff] & (1U << bit)) ? 255 : 0;
            }
            if (coverage == 0) continue;
            const uint8_t alpha =
                static_cast<uint8_t>((static_cast<uint32_t>(state.fillAlpha) * coverage) / 255U);
            plot(gx0 + gx, gy0 + gy, state.fillGray, alpha);
          }
        }
      }
      penX += static_cast<float>(g.advance);
    }
  };

  for (const auto &run : runs) {
    renderRun(run.text, run.x, run.y, run.fontSize, std::string_view(run.anchorOverride));
  }
}

auto SvgDecoder::fillPolygon(const std::vector<std::pair<int32_t, int32_t>> &polygon, uint8_t gray,
                             uint8_t alpha) -> void {
  if (polygon.size() < 3 || alpha == 0) return;

  int32_t minY = polygon.front().second;
  int32_t maxY = polygon.front().second;
  int32_t minX = polygon.front().first;
  int32_t maxX = polygon.front().first;

  for (const auto &[x, y] : polygon) {
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
  }

  // For small triangles (arrow heads), use antialiased rendering
  const bool isSmallPolygon = (maxX - minX) <= 30 && (maxY - minY) <= 30;

  if (isSmallPolygon) {
    // Antialiased triangle fill using coverage
    auto isInsideTriangle = [&polygon](float px, float py) -> bool {
      auto sign = [](float px, float py, float ax, float ay, float bx, float by) -> float {
        return (px - bx) * (ay - by) - (ax - bx) * (py - by);
      };

      const float d1 =
          sign(px, py, polygon[0].first, polygon[0].second, polygon[1].first, polygon[1].second);
      const float d2 =
          sign(px, py, polygon[1].first, polygon[1].second, polygon[2].first, polygon[2].second);
      const float d3 =
          sign(px, py, polygon[2].first, polygon[2].second, polygon[0].first, polygon[0].second);

      const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
      const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
      return !(hasNeg && hasPos);
    };

    for (int32_t y = minY - 1; y <= maxY + 1; ++y) {
      for (int32_t x = minX - 1; x <= maxX + 1; ++x) {
        // Sample 4 points within the pixel for coverage
        float coverage            = 0.0;
        const float offsets[4][2] = {{0.25, 0.25}, {0.75, 0.25}, {0.25, 0.75}, {0.75, 0.75}};
        for (int i = 0; i < 4; ++i) {
          const float px = x + offsets[i][0];
          const float py = y + offsets[i][1];
          if (isInsideTriangle(px, py)) coverage += 0.25;
        }

        if (coverage > 0.0) {
          const uint8_t blendedAlpha = static_cast<uint8_t>(std::lround(alpha * coverage));
          plot(x, y, gray, blendedAlpha);
        }
      }
    }
  } else {
    // Original scan-line fill for larger polygons
    for (int32_t y = minY; y <= maxY; ++y) {
      std::vector<int32_t> nodes;
      for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto [xi, yi] = polygon[i];
        const auto [xj, yj] = polygon[j];

        const bool cond = ((yi < y && yj >= y) || (yj < y && yi >= y));
        if (!cond || yi == yj) continue;

        const float xx = xi + static_cast<float>(y - yi) * static_cast<float>(xj - xi) /
                                  static_cast<float>(yj - yi);
        nodes.push_back(static_cast<int32_t>(std::lround(xx)));
      }

      std::sort(nodes.begin(), nodes.end());
      for (std::size_t k = 0; k + 1 < nodes.size(); k += 2) {
        for (int32_t x = nodes[k]; x <= nodes[k + 1]; ++x) {
          plot(x, y, gray, alpha);
        }
      }
    }
  }
}

auto SvgDecoder::renderPath(const pugi::xml_node &node, const PaintState &state, int32_t scale)
    -> void {
  const char *d = node.attribute("d").as_string();
  if (d == nullptr || *d == '\0') return;

  const std::string markerStartValue = getStyleProperty(node, "marker-start");
  const std::string markerEndValue   = getStyleProperty(node, "marker-end");
  const auto markerStart             = std::string_view(markerStartValue);
  const auto markerEnd               = std::string_view(markerEndValue);

  std::vector<PathSubpath> subpaths;
  PathSubpath current{};

  float curX      = 0.0;
  float curY      = 0.0;
  float startX    = 0.0;
  float startY    = 0.0;
  bool hasCurrent = false;

  float lastCubicCtrlX  = 0.0;
  float lastCubicCtrlY  = 0.0;
  bool hasLastCubicCtrl = false;

  float lastQuadCtrlX  = 0.0;
  float lastQuadCtrlY  = 0.0;
  bool hasLastQuadCtrl = false;

  char prevCmd = 0;
  char cmd     = 0;

  const auto finalizeCurrent = [&]() {
    if (!current.points.empty()) {
      subpaths.push_back(std::move(current));
      current = PathSubpath{};
    }
  };

  const auto startSubpath = [&](float x, float y) {
    finalizeCurrent();
    current.points.emplace_back(x, y);
    current.closed = false;
    curX           = x;
    curY           = y;
    startX         = x;
    startY         = y;
    hasCurrent     = true;
  };

  const auto lineTo = [&](float x, float y) {
    if (!hasCurrent) {
      startSubpath(x, y);
      return;
    }
    current.points.emplace_back(x, y);
    curX = x;
    curY = y;
  };

  const auto closeCurrent = [&]() {
    if (!hasCurrent || current.points.empty()) return;
    current.closed      = true;
    const auto [fx, fy] = current.points.front();
    if (std::abs(curX - fx) > 1e-9 || std::abs(curY - fy) > 1e-9) {
      current.points.emplace_back(fx, fy);
    }
    curX = startX;
    curY = startY;
  };

  const auto readPair = [&](const char *&p, const char *end, float &x, float &y) {
    const char *saved = p;
    if (!parsePathNumber(p, end, x)) {
      p = saved;
      return false;
    }
    if (!parsePathNumber(p, end, y)) {
      p = saved;
      return false;
    }
    return true;
  };

  const auto arcTo = [&](float rx, float ry, float xAxisRotationDeg, bool largeArcFlag,
                         bool sweepFlag, float x2, float y2) {
    const float x1 = curX;
    const float y1 = curY;

    if (!hasCurrent) {
      lineTo(x2, y2);
      return;
    }

    if (std::abs(x2 - x1) < 1e-9 && std::abs(y2 - y1) < 1e-9) return;
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx < 1e-9 || ry < 1e-9) {
      lineTo(x2, y2);
      return;
    }

    const float phi    = xAxisRotationDeg * std::acos(-1.0) / 180.0;
    const float cosPhi = std::cos(phi);
    const float sinPhi = std::sin(phi);

    const float dx2 = (x1 - x2) * 0.5;
    const float dy2 = (y1 - y2) * 0.5;
    const float x1p = cosPhi * dx2 + sinPhi * dy2;
    const float y1p = -sinPhi * dx2 + cosPhi * dy2;

    const float x1p2 = x1p * x1p;
    const float y1p2 = y1p * y1p;

    float rx2 = rx * rx;
    float ry2 = ry * ry;

    const float lambda = x1p2 / rx2 + y1p2 / ry2;
    if (lambda > 1.0) {
      const float s = std::sqrt(lambda);
      rx *= s;
      ry *= s;
      rx2 = rx * rx;
      ry2 = ry * ry;
    }

    const float numerator = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    const float denom     = rx2 * y1p2 + ry2 * x1p2;
    const float factor =
        (denom <= 0.0 || numerator <= 0.0)
            ? 0.0
            : std::sqrt(numerator / denom) * (largeArcFlag == sweepFlag ? -1.0 : 1.0);

    const float cxp = factor * (rx * y1p / ry);
    const float cyp = factor * (-ry * x1p / rx);

    const float cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) * 0.5;
    const float cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) * 0.5;

    const auto angleBetween = [](float ux, float uy, float vx, float vy) {
      const float dot = ux * vx + uy * vy;
      const float det = ux * vy - uy * vx;
      return std::atan2(det, dot);
    };

    const float ux = (x1p - cxp) / rx;
    const float uy = (y1p - cyp) / ry;
    const float vx = (-x1p - cxp) / rx;
    const float vy = (-y1p - cyp) / ry;

    float theta1     = angleBetween(1.0, 0.0, ux, uy);
    float deltaTheta = angleBetween(ux, uy, vx, vy);

    if (!sweepFlag && deltaTheta > 0.0) deltaTheta -= 2.0 * std::acos(-1.0);
    if (sweepFlag && deltaTheta < 0.0) deltaTheta += 2.0 * std::acos(-1.0);

    const int32_t segments =
        std::max(static_cast<int32_t>(1),
                 static_cast<int32_t>(std::ceil(std::abs(deltaTheta) / (std::acos(-1.0) / 32.0))));
    const float step = deltaTheta / static_cast<float>(segments);

    for (int32_t i = 1; i <= segments; ++i) {
      const float theta = theta1 + step * static_cast<float>(i);
      const float ct    = std::cos(theta);
      const float st    = std::sin(theta);

      const float x = cosPhi * rx * ct - sinPhi * ry * st + cx;
      const float y = sinPhi * rx * ct + cosPhi * ry * st + cy;
      lineTo(x, y);
    }
  };

  const auto drawMarker = [&](int32_t px, int32_t py, int32_t dx, int32_t dy,
                              std::string_view marker, const PaintState &markerState) {
    if (marker.empty() || marker == "none") return;
    if (dx == 0 && dy == 0) dx = 1;

    const float length = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
    const float ux     = static_cast<float>(dx) / length;
    const float uy     = static_cast<float>(dy) / length;
    const float pxn    = -uy;
    const float pyn    = ux;

    if (isMarkerLike(marker)) {
      const int32_t radius      = std::max(static_cast<int32_t>(1),
                                           mapStrokeWidth(markerState.strokeWidth, markerState, scale));
      const auto circleCoverage = [](float dist, float r) -> float {
        const float edge       = 1.0;
        const float signedDist = r - dist;
        if (signedDist >= edge) return 1.0;
        if (signedDist <= -edge) return 0.0;
        return (signedDist + edge) * 0.5;
      };

      for (int32_t oy = -radius - 1; oy <= radius + 1; ++oy) {
        for (int32_t ox = -radius - 1; ox <= radius + 1; ++ox) {
          const float dist = std::hypot(static_cast<float>(ox), static_cast<float>(oy));
          const float cov  = circleCoverage(dist, static_cast<float>(radius));
          if (cov <= 0.0) continue;
          const uint8_t alpha =
              static_cast<uint8_t>(std::lround(static_cast<float>(markerState.strokeAlpha) * cov));
          if (alpha > 0) plot(px + ox, py + oy, markerState.strokeGray, alpha);
        }
      }
      return;
    }

    const float size = std::max(
        9.0, static_cast<float>(mapStrokeWidth(markerState.strokeWidth, markerState, scale)) * 6.3);
    const float backX  = static_cast<float>(px) - ux * size;
    const float backY  = static_cast<float>(py) - uy * size;
    const float leftX  = backX + pxn * (size * 0.55);
    const float leftY  = backY + pyn * (size * 0.55);
    const float rightX = backX - pxn * (size * 0.55);
    const float rightY = backY - pyn * (size * 0.55);

    std::vector<std::pair<int32_t, int32_t>> triangle = {
        {px, py},
        {static_cast<int32_t>(std::lround(leftX)), static_cast<int32_t>(std::lround(leftY))},
        {static_cast<int32_t>(std::lround(rightX)), static_cast<int32_t>(std::lround(rightY))},
    };
    fillPolygon(triangle, markerState.strokeGray, markerState.strokeAlpha);
  };

  const char *p         = d;
  const char *const end = d + std::strlen(d);
  while (p < end) {
    skipPathSeparators(p, end);
    if (p >= end) break;

    if (isPathCommand(*p)) {
      cmd = *p;
      ++p;
      if (cmd == 'Z' || cmd == 'z') {
        closeCurrent();
        hasLastCubicCtrl = false;
        hasLastQuadCtrl  = false;
        prevCmd          = cmd;
        continue;
      }
    } else if (cmd == 0) {
      break;
    }

    const bool relative = (cmd >= 'a' && cmd <= 'z');
    const char upper    = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

    if (upper == 'M') {
      float x = 0.0;
      float y = 0.0;
      if (!readPair(p, end, x, y)) {
        prevCmd = cmd;
        continue;
      }
      if (relative) {
        x += curX;
        y += curY;
      }
      startSubpath(x, y);

      while (true) {
        const char *saved = p;
        if (!readPair(p, end, x, y)) {
          p = saved;
          break;
        }
        if (relative) {
          x += curX;
          y += curY;
        }
        lineTo(x, y);
      }
      hasLastCubicCtrl = false;
      hasLastQuadCtrl  = false;
    } else if (upper == 'L') {
      float x = 0.0;
      float y = 0.0;
      while (readPair(p, end, x, y)) {
        if (relative) {
          x += curX;
          y += curY;
        }
        lineTo(x, y);
      }
      hasLastCubicCtrl = false;
      hasLastQuadCtrl  = false;
    } else if (upper == 'H') {
      float x = 0.0;
      while (parsePathNumber(p, end, x)) {
        if (relative) x += curX;
        lineTo(x, curY);
      }
      hasLastCubicCtrl = false;
      hasLastQuadCtrl  = false;
    } else if (upper == 'V') {
      float y = 0.0;
      while (parsePathNumber(p, end, y)) {
        if (relative) y += curY;
        lineTo(curX, y);
      }
      hasLastCubicCtrl = false;
      hasLastQuadCtrl  = false;
    } else if (upper == 'C') {
      float x1 = 0.0;
      float y1 = 0.0;
      float x2 = 0.0;
      float y2 = 0.0;
      float x  = 0.0;
      float y  = 0.0;
      while (true) {
        const char *saved = p;
        if (!readPair(p, end, x1, y1) || !readPair(p, end, x2, y2) || !readPair(p, end, x, y)) {
          p = saved;
          break;
        }
        if (relative) {
          x1 += curX;
          y1 += curY;
          x2 += curX;
          y2 += curY;
          x += curX;
          y += curY;
        }
        if (!hasCurrent) startSubpath(curX, curY);
        flattenCubic(current.points, curX, curY, x1, y1, x2, y2, x, y);
        curX             = x;
        curY             = y;
        lastCubicCtrlX   = x2;
        lastCubicCtrlY   = y2;
        hasLastCubicCtrl = true;
        hasLastQuadCtrl  = false;
      }
    } else if (upper == 'S') {
      float x2 = 0.0;
      float y2 = 0.0;
      float x  = 0.0;
      float y  = 0.0;
      while (true) {
        const char *saved = p;
        if (!readPair(p, end, x2, y2) || !readPair(p, end, x, y)) {
          p = saved;
          break;
        }
        if (relative) {
          x2 += curX;
          y2 += curY;
          x += curX;
          y += curY;
        }

        const bool smoothFromCubic =
            (prevCmd == 'C' || prevCmd == 'c' || prevCmd == 'S' || prevCmd == 's') &&
            hasLastCubicCtrl;
        const float x1 = smoothFromCubic ? (2.0 * curX - lastCubicCtrlX) : curX;
        const float y1 = smoothFromCubic ? (2.0 * curY - lastCubicCtrlY) : curY;

        if (!hasCurrent) startSubpath(curX, curY);
        flattenCubic(current.points, curX, curY, x1, y1, x2, y2, x, y);
        curX             = x;
        curY             = y;
        lastCubicCtrlX   = x2;
        lastCubicCtrlY   = y2;
        hasLastCubicCtrl = true;
        hasLastQuadCtrl  = false;
      }
    } else if (upper == 'Q') {
      float cx = 0.0;
      float cy = 0.0;
      float x  = 0.0;
      float y  = 0.0;
      while (true) {
        const char *saved = p;
        if (!readPair(p, end, cx, cy) || !readPair(p, end, x, y)) {
          p = saved;
          break;
        }
        if (relative) {
          cx += curX;
          cy += curY;
          x += curX;
          y += curY;
        }
        if (!hasCurrent) startSubpath(curX, curY);
        flattenQuadratic(current.points, curX, curY, cx, cy, x, y);
        curX             = x;
        curY             = y;
        lastQuadCtrlX    = cx;
        lastQuadCtrlY    = cy;
        hasLastQuadCtrl  = true;
        hasLastCubicCtrl = false;
      }
    } else if (upper == 'T') {
      float x = 0.0;
      float y = 0.0;
      while (true) {
        const char *saved = p;
        if (!readPair(p, end, x, y)) {
          p = saved;
          break;
        }
        if (relative) {
          x += curX;
          y += curY;
        }
        const bool smoothFromQuad =
            (prevCmd == 'Q' || prevCmd == 'q' || prevCmd == 'T' || prevCmd == 't') &&
            hasLastQuadCtrl;
        const float cx = smoothFromQuad ? (2.0 * curX - lastQuadCtrlX) : curX;
        const float cy = smoothFromQuad ? (2.0 * curY - lastQuadCtrlY) : curY;

        if (!hasCurrent) startSubpath(curX, curY);
        flattenQuadratic(current.points, curX, curY, cx, cy, x, y);
        curX             = x;
        curY             = y;
        lastQuadCtrlX    = cx;
        lastQuadCtrlY    = cy;
        hasLastQuadCtrl  = true;
        hasLastCubicCtrl = false;
      }
    } else if (upper == 'A') {
      while (true) {
        const char *saved = p;
        float rx = 0.0, ry = 0.0, rot = 0.0, largeArc = 0.0, sweep = 0.0, x = 0.0, y = 0.0;
        if (!parsePathNumber(p, end, rx) || !parsePathNumber(p, end, ry) ||
            !parsePathNumber(p, end, rot) || !parsePathNumber(p, end, largeArc) ||
            !parsePathNumber(p, end, sweep) || !parsePathNumber(p, end, x) ||
            !parsePathNumber(p, end, y)) {
          p = saved;
          break;
        }
        (void)rx;
        (void)ry;
        (void)rot;
        (void)largeArc;
        (void)sweep;
        if (relative) {
          x += curX;
          y += curY;
        }
        arcTo(rx, ry, rot, largeArc >= 0.5, sweep >= 0.5, x, y);
      }
      hasLastCubicCtrl = false;
      hasLastQuadCtrl  = false;
    }

    prevCmd = cmd;
  }

  finalizeCurrent();
  if (subpaths.empty()) return;

  if (state.fillEnabled && state.fillAlpha > 0) {
    const std::string fillRuleValue = getStyleProperty(node, "fill-rule");
    const std::string_view fillRule = trim(fillRuleValue);
    const bool evenOdd              = (fillRule == "evenodd");

    std::vector<std::vector<std::pair<float, float>>> contours;
    contours.reserve(subpaths.size());
    for (const auto &sp : subpaths) {
      if (sp.points.size() < 3) continue;

      std::vector<std::pair<float, float>> scaled;
      scaled.reserve(sp.points.size() + 1);
      for (const auto &[x, y] : sp.points) {
        scaled.emplace_back(mapCoordX(x, y, state, scale), mapCoordY(x, y, state, scale));
      }

      if (scaled.size() >= 3) {
        const auto [fx, fy] = scaled.front();
        const auto [lx, ly] = scaled.back();
        if (std::abs(fx - lx) > 1e-9 || std::abs(fy - ly) > 1e-9) scaled.emplace_back(fx, fy);
        contours.push_back(std::move(scaled));
      }
    }

    if (!contours.empty()) {
      float minXf = std::numeric_limits<float>::max();
      float maxXf = std::numeric_limits<float>::lowest();
      float minYf = std::numeric_limits<float>::max();
      float maxYf = std::numeric_limits<float>::lowest();
      for (const auto &contour : contours) {
        for (const auto &[x, y] : contour) {
          minXf = std::min(minXf, x);
          maxXf = std::max(maxXf, x);
          minYf = std::min(minYf, y);
          maxYf = std::max(maxYf, y);
        }
      }

      const int32_t minX = static_cast<int32_t>(std::floor(minXf));
      const int32_t maxX = static_cast<int32_t>(std::ceil(maxXf));
      const int32_t minY = static_cast<int32_t>(std::floor(minYf));
      const int32_t maxY = static_cast<int32_t>(std::ceil(maxYf));

      const auto insideAt = [&](float sx, float sy) {
        if (evenOdd) {
          bool inside = false;
          for (const auto &contour : contours) {
            if (contour.size() < 3) continue;
            for (std::size_t i = 0, j = contour.size() - 1; i < contour.size(); j = i++) {
              const auto [xi, yi] = contour[i];
              const auto [xj, yj] = contour[j];
              if (std::abs(yi - yj) < 1e-12) continue;

              const bool crosses = ((yi <= sy && yj > sy) || (yj <= sy && yi > sy));
              if (!crosses) continue;

              const float xCross = xi + (sy - yi) * (xj - xi) / (yj - yi);
              if (xCross > sx) inside = !inside;
            }
          }
          return inside;
        }

        int32_t winding = 0;
        for (const auto &contour : contours) {
          if (contour.size() < 3) continue;
          for (std::size_t i = 0, j = contour.size() - 1; i < contour.size(); j = i++) {
            const auto [xi, yi] = contour[i];
            const auto [xj, yj] = contour[j];
            if (std::abs(yi - yj) < 1e-12) continue;

            const bool crosses = ((yi <= sy && yj > sy) || (yj <= sy && yi > sy));
            if (!crosses) continue;

            const float xCross = xi + (sy - yi) * (xj - xi) / (yj - yi);
            if (xCross > sx) winding += (yi < yj) ? 1 : -1;
          }
        }
        return winding != 0;
      };

      static constexpr int32_t kAaGrid         = 4;
      static constexpr uint32_t kAaSampleCount = static_cast<uint32_t>(kAaGrid * kAaGrid);

      for (int32_t y = minY; y <= maxY; ++y) {
        for (int32_t x = minX; x <= maxX; ++x) {
          uint32_t covered = 0;
          for (int32_t sy = 0; sy < kAaGrid; ++sy) {
            for (int32_t sx = 0; sx < kAaGrid; ++sx) {
              const float ox = (static_cast<float>(sx) + 0.5) / static_cast<float>(kAaGrid);
              const float oy = (static_cast<float>(sy) + 0.5) / static_cast<float>(kAaGrid);
              if (insideAt(static_cast<float>(x) + ox, static_cast<float>(y) + oy)) {
                ++covered;
              }
            }
          }
          if (covered == 0) continue;
          const uint8_t aaAlpha = static_cast<uint8_t>(
              (static_cast<uint32_t>(state.fillAlpha) * covered) / kAaSampleCount);
          if (aaAlpha == 0) continue;
          plot(x, y, state.fillGray, aaAlpha);
        }
      }
    }
  }

  if (state.strokeEnabled && state.strokeAlpha > 0) {
    const int32_t sw = mapStrokeWidth(state.strokeWidth, state, scale);
    for (const auto &sp : subpaths) {
      if (sp.points.size() < 2) continue;

      std::vector<std::pair<int32_t, int32_t>> scaled;
      scaled.reserve(sp.points.size());
      for (const auto &[x, y] : sp.points) {
        scaled.emplace_back(static_cast<int32_t>(std::lround(mapCoordX(x, y, state, scale))),
                            static_cast<int32_t>(std::lround(mapCoordY(x, y, state, scale))));
      }

      for (std::size_t i = 1; i < scaled.size(); ++i) {
        drawLine(scaled[i - 1].first, scaled[i - 1].second, scaled[i].first, scaled[i].second,
                 state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
      }
      if (sp.closed && scaled.size() >= 2) {
        const auto [fx, fy] = scaled.front();
        const auto [lx, ly] = scaled.back();
        if (fx != lx || fy != ly) {
          drawLine(lx, ly, fx, fy, state.strokeGray, state.strokeAlpha, sw,
                   state.strokeLineCapRound);
        }
      }

      if (scaled.size() >= 2) {
        drawMarker(scaled.front().first, scaled.front().second,
                   scaled.front().first - scaled[1].first, scaled.front().second - scaled[1].second,
                   markerStart, state);
        drawMarker(scaled.back().first, scaled.back().second,
                   scaled.back().first - scaled[scaled.size() - 2].first,
                   scaled.back().second - scaled[scaled.size() - 2].second, markerEnd, state);
      }
    }
  }
}

auto SvgDecoder::renderPolyline(const pugi::xml_node &node, const PaintState &state, int32_t scale,
                                bool closed) -> void {
  std::vector<std::pair<float, float>> points;
  if (!parsePoints(node.attribute("points").as_string(), points)) return;

  std::vector<std::pair<int32_t, int32_t>> scaled;
  scaled.reserve(points.size());
  for (const auto &[x, y] : points) {
    scaled.emplace_back(static_cast<int32_t>(std::lround(mapCoordX(x, y, state, scale))),
                        static_cast<int32_t>(std::lround(mapCoordY(x, y, state, scale))));
  }

  if (closed && state.fillEnabled && state.fillAlpha > 0 && scaled.size() >= 3) {
    fillPolygon(scaled, state.fillGray, state.fillAlpha);
  }

  if (state.strokeEnabled && state.strokeAlpha > 0) {
    const int32_t sw = mapStrokeWidth(state.strokeWidth, state, scale);
    for (std::size_t i = 1; i < scaled.size(); ++i) {
      drawLine(scaled[i - 1].first, scaled[i - 1].second, scaled[i].first, scaled[i].second,
               state.strokeGray, state.strokeAlpha, sw, state.strokeLineCapRound);
    }
    if (closed && scaled.size() >= 2) {
      drawLine(scaled.back().first, scaled.back().second, scaled.front().first,
               scaled.front().second, state.strokeGray, state.strokeAlpha, sw,
               state.strokeLineCapRound);
    }
  }
}
