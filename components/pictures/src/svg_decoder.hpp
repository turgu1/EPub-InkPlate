#pragma once

#include "font.hpp"
#include "himem.hpp"
#include "pugixml.hpp"

#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

enum class SvgOption : uint8_t {
  SCALE_HALF, SCALE_QUARTER, SCALE_EIGHTH, Count
};

using SvgOptions = std::bitset<static_cast<uint8_t>(SvgOption::Count)>;

#define SVG_OPTION(opt) static_cast<uint8_t>(SvgOption::opt)

enum class SvgError : uint8_t {
  SUCCESS, INVALID_PARAM, INVALID_FILE, PARSE_ERROR, DECODE_ERROR
};

struct SvgDraw {
  int32_t x{0};
  int32_t y{0};
  int32_t iWidth{0};
  int32_t iHeight{0};
  int32_t iBpp{8};
  uint8_t *pPixels{nullptr};
  void *pUser{nullptr};
};

using SvgDrawCallback = int32_t (*)(SvgDraw *pDraw);

class SvgDecoder {
public:
  SvgDecoder(FontPtr &font) : font_(font) { font_->setPreferAntialiasing(true); }
  ~SvgDecoder() { font_->setPreferAntialiasing(false); }

  auto openRAM(const uint8_t *pData, int32_t iDataSize, SvgDrawCallback pfnDraw) -> int32_t;
  auto close() -> void;

  auto decode(int32_t x, int32_t y, SvgOptions iOptions = {}) -> int32_t;

  auto setUserPointer(void *p) -> void;

  [[nodiscard]] auto getLastError() const -> SvgError;
  [[nodiscard]] auto getWidth() const -> int32_t;
  [[nodiscard]] auto getHeight() const -> int32_t;
  [[nodiscard]] auto getOutputWidth() const -> int32_t;
  [[nodiscard]] auto getOutputHeight() const -> int32_t;

private:
  struct PaintState {
    struct Transform {
      float a{1.0};
      float b{0.0};
      float c{0.0};
      float d{1.0};
      float e{0.0};
      float f{0.0};
    };

    bool fillEnabled{true};
    uint8_t fillGray{0x00};
    uint8_t fillAlpha{0xff};

    bool strokeEnabled{false};
    uint8_t strokeGray{0x00};
    uint8_t strokeAlpha{0xff};
    float strokeWidth{1.0};
    bool strokeLineCapRound{false};
    Transform transform{};
  };

  auto reset() -> void;
  auto parseDimensions() -> bool;
  auto parseEmbeddedStyles() -> void;

  [[nodiscard]] auto getStyleProperty(const pugi::xml_node &node, std::string_view attrName,
                                      bool includeClass = true) const -> std::string;

  auto renderDocument(int32_t scale) -> bool;
  auto renderNode(const pugi::xml_node &node, const PaintState &parent, int32_t scale) -> bool;

  auto flushChunks(int32_t x, int32_t y) -> bool;

  [[nodiscard]] auto mapCoordX(float x, float y, const PaintState &state, int32_t scale) const
      -> float;
  [[nodiscard]] auto mapCoordY(float x, float y, const PaintState &state, int32_t scale) const
      -> float;
  [[nodiscard]] auto mapLenX(float len, const PaintState &state, int32_t scale) const -> float;
  [[nodiscard]] auto mapLenY(float len, const PaintState &state, int32_t scale) const -> float;
  [[nodiscard]] auto mapStrokeWidth(float strokeWidth, const PaintState &state, int32_t scale) const
      -> int32_t;

  auto plot(int32_t x, int32_t y, uint8_t gray, uint8_t alpha) -> void;
  auto drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t gray, uint8_t alpha,
                int32_t thickness, bool roundCaps = false) -> void;

  auto renderRect(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderLine(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderCircle(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderEllipse(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderImage(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderText(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderPath(const pugi::xml_node &node, const PaintState &state, int32_t scale) -> void;
  auto renderPolyline(const pugi::xml_node &node, const PaintState &state, int32_t scale,
                      bool closed) -> void;
  auto fillPolygon(const std::vector<std::pair<int32_t, int32_t>> &polygon, uint8_t gray,
                   uint8_t alpha) -> void;

private:
  const uint8_t *data_{nullptr};
  int32_t size_{0};
  SvgDrawCallback drawCb_{nullptr};
  void *user_{nullptr};

  FontPtr &font_;

  pugi::xml_document doc_{};
  pugi::xml_node root_{};

  using ClassStyleMap =
      std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
  ClassStyleMap classStyles_{};

  int32_t width_{0};
  int32_t height_{0};
  int32_t outputWidth_{0};
  int32_t outputHeight_{0};

  bool hasViewBox_{false};
  float viewX_{0.0};
  float viewY_{0.0};
  float viewW_{0.0};
  float viewH_{0.0};
  float viewportScaleX_{1.0};
  float viewportScaleY_{1.0};
  float viewportOffsetX_{0.0};
  float viewportOffsetY_{0.0};

  HimemUniquePtr<uint8_t[]> bitmap_{nullptr};

  SvgError lastError_{SvgError::SUCCESS};
};
