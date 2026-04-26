// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// Standalone Config-class verification tool
//
// This test is intentionally isolated from the main test suite so Config can be
// validated while other test sections are still being adapted to the ongoing
// fonts-management changes.
// ---------------------------------------------------------------------------

#define __GLOBAL__ 0
#define __CONFIG__ 1
#include "models/config.hpp"
#undef __CONFIG__

#include "himem.hpp"
#include "screen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

struct ExpectedFontEntry {
  HimemString name;
  FaceStyle style;
  HimemString filename;
};

static auto trim(const HimemString &s) -> HimemString {
  const std::size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const std::size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

static auto extractAttr(const HimemString &line, const char *attr) -> HimemString {
  HimemString pattern(attr);
  pattern += "=\"";
  const std::size_t start = line.find(pattern);
  if (start == std::string::npos) return "";
  const std::size_t valStart = start + pattern.size();
  const std::size_t valEnd   = line.find('"', valStart);
  if (valEnd == std::string::npos) return "";
  return line.substr(valStart, valEnd - valStart);
}

static auto resolveDpiPlaceholder(const HimemString &filename) -> HimemString {
  HimemString resolved = filename;
  const HimemString key("%DPI%");
  const std::size_t pos = resolved.find(key);
  if (pos != HimemString::npos) {
    resolved.replace(pos, key.size(), std::to_string(Screen::RESOLUTION));
  }
  if (!resolved.empty() && (resolved.front() != '/')) {
    resolved = HimemString(MAIN_FOLDER) + "/fonts/" + resolved;
  }
  return resolved;
}

static auto parseStyleTag(const HimemString &line, FaceStyle *styleOut) -> bool {
  if (line.rfind("<normal", 0) == 0) {
    *styleOut = FaceStyle::NORMAL;
    return true;
  }
  if (line.rfind("<bold-italic", 0) == 0) {
    *styleOut = FaceStyle::BOLD_ITALIC;
    return true;
  }
  if (line.rfind("<bold", 0) == 0) {
    *styleOut = FaceStyle::BOLD;
    return true;
  }
  if (line.rfind("<italic", 0) == 0) {
    *styleOut = FaceStyle::ITALIC;
    return true;
  }
  return false;
}

static auto parseExpectedFontsFromXml() -> std::vector<ExpectedFontEntry> {
  std::vector<ExpectedFontEntry> entries;

  std::ifstream in(std::string(MAIN_FOLDER) + "/fonts_list.xml");
  if (!in.is_open()) return entries;

  HimemString currentFontName;
  HimemString line;
  while (std::getline(in, line)) {
    line = trim(line);

    if (line.rfind("<font ", 0) == 0) {
      currentFontName = extractAttr(line, "name");
      continue;
    }

    if (line.rfind("</font>", 0) == 0) {
      currentFontName.clear();
      continue;
    }

    if (currentFontName.empty()) continue;

    FaceStyle style;
    if (!parseStyleTag(line, &style)) continue;

    HimemString filename = extractAttr(line, "filename");
    if (filename.empty()) continue;

    entries.push_back({currentFontName, style, resolveDpiPlaceholder(filename)});
  }

  return entries;
}

static auto parseLoadedFontsFromXml(uint8_t defaultFontIndex) -> std::vector<ExpectedFontEntry> {
  const std::vector<ExpectedFontEntry> allEntries = parseExpectedFontsFromXml();
  std::vector<ExpectedFontEntry> loaded;

  std::size_t userFontStart = allEntries.size();
  std::optional<ExpectedFontEntry> iconEntry;
  std::optional<ExpectedFontEntry> systemNormalEntry;
  std::optional<ExpectedFontEntry> systemItalicEntry;

  for (std::size_t i = 0; i < allEntries.size(); ++i) {
    auto entry = allEntries[i];
    if (entry.name == "ICON") {
      entry.name  = "Icon";
      entry.style = FaceStyle::ITALIC;
      iconEntry   = entry;
      continue;
    }
    if (entry.name == "TEXT") {
      entry.name = "System";
      if (entry.style == FaceStyle::NORMAL) {
        systemNormalEntry = entry;
      } else if (entry.style == FaceStyle::ITALIC) {
        systemItalicEntry = entry;
      }
      continue;
    }

    if ((entry.name != "Icon") && (entry.name != "System")) {
      userFontStart = i;
      break;
    }
  }

  if (iconEntry.has_value()) loaded.push_back(*iconEntry);
  if (systemNormalEntry.has_value()) loaded.push_back(*systemNormalEntry);
  if (systemItalicEntry.has_value()) loaded.push_back(*systemItalicEntry);

  std::vector<HimemString> userFontNames;
  for (std::size_t i = userFontStart; i < allEntries.size(); ++i) {
    const HimemString &name = allEntries[i].name;
    if (userFontNames.empty() || (userFontNames.back() != name)) {
      userFontNames.push_back(name);
    }
  }

  if (userFontNames.empty()) return loaded;

  const std::size_t selectedUserFont =
      (defaultFontIndex < userFontNames.size()) ? defaultFontIndex : 0;
  const HimemString &selectedName = userFontNames[selectedUserFont];

  for (std::size_t i = userFontStart; i < allEntries.size(); ++i) {
    if (allEntries[i].name == selectedName) {
      loaded.push_back(allEntries[i]);
    }
  }

  return loaded;
}

#define CFGS_LOG(fmt, ...) std::printf("[config-standalone] " fmt "\n", ##__VA_ARGS__)

static int s_pass = 0;
static int s_fail = 0;

#define CFGS_CHECK(cond, msg)                                                                      \
  do {                                                                                             \
    if (cond) {                                                                                    \
      CFGS_LOG("PASS %s", msg);                                                                    \
      ++s_pass;                                                                                    \
    } else {                                                                                       \
      CFGS_LOG("FAIL [%s:%d] %s", __FILE__, __LINE__, msg);                                        \
      ++s_fail;                                                                                    \
    }                                                                                              \
  } while (0)

static void testFixtureConfigRead() {
  CFGS_LOG("--- fixture config read ---");

  Config cfg(CONFIG_FILE, false);
  CFGS_CHECK(cfg.read(), "read() succeeds on fixture config file");

  int8_t byteVal        = -1;
  int8_t defaultFontVal = -1;
  int32_t intVal        = -1;
  HimemString strVal;

  CFGS_CHECK(cfg.get(ConfigIdent::VERSION, &byteVal) && byteVal == 1, "version == 1");

  CFGS_CHECK(cfg.get(ConfigIdent::SSID, strVal) && strVal == "your_pwd_ssid",
             "wifi_ssid matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::PWD, strVal) && strVal == "your_wifi_pwd",
             "wifi_pwd matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::DNS_NAME, strVal) && strVal == "inkplate",
             "dns_name matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::AP_SSID, strVal) && strVal == "inkplate",
             "ap_wifi_ssid matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::AP_PWD, strVal) && strVal == "qwerty1234",
             "ap_wifi_pwd matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::NTP_SERVER, strVal) && strVal == "pool.ntp.org",
             "ntp_server matches fixture");
  CFGS_CHECK(cfg.get(ConfigIdent::TIME_ZONE, strVal) && strVal == "EST5EDT,M3.2.0,M11.1.0",
             "tz matches fixture");

  CFGS_CHECK(cfg.get(ConfigIdent::PORT, &intVal) && intVal == 80, "http_port == 80");

  CFGS_CHECK(cfg.get(ConfigIdent::BATTERY, &byteVal) && byteVal == 2, "battery == 2");
  CFGS_CHECK(cfg.get(ConfigIdent::TIMEOUT, &byteVal) && byteVal == 5, "timeout == 5");
  CFGS_CHECK(cfg.get(ConfigIdent::FONT_SIZE, &byteVal) && byteVal == 12, "font_size == 12");
  CFGS_CHECK(cfg.get(ConfigIdent::DEFAULT_FONT, &defaultFontVal) && defaultFontVal == 5,
             "default_font == 5");
  CFGS_CHECK(cfg.get(ConfigIdent::USE_FONTS_IN_BOOKS, &byteVal) && byteVal == 1,
             "use_fonts_in_books == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::SHOW_PICTURES, &byteVal) && byteVal == 1, "show_images == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::ORIENTATION, &byteVal) && byteVal == 1, "orientation == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::PIXEL_RESOLUTION, &byteVal) && byteVal == 1, "resolution == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::SHOW_HEAP, &byteVal) && byteVal == 0, "show_heap == 0");
  CFGS_CHECK(cfg.get(ConfigIdent::SHOW_TITLE, &byteVal) && byteVal == 1, "show_title == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::FRONT_LIGHT, &byteVal) && byteVal == 15, "front_light == 15");
  CFGS_CHECK(cfg.get(ConfigIdent::DIR_VIEW, &byteVal) && byteVal == 0, "dir_view == 0");
  CFGS_CHECK(cfg.get(ConfigIdent::COVER_SIZE, &byteVal) && byteVal == 1, "cover_size == 1");
  CFGS_CHECK(cfg.get(ConfigIdent::SHOW_RTC, &byteVal) && byteVal == 1, "show_rtc == 1");

  FontsDB *fontsDB = nullptr;
  CFGS_CHECK(cfg.get(ConfigIdent::FONTS_DB, &fontsDB) && (fontsDB != nullptr),
             "fonts descriptor instance is reachable through Config");

  const std::vector<ExpectedFontEntry> expectedFonts = parseLoadedFontsFromXml(defaultFontVal);
  CFGS_CHECK(!expectedFonts.empty(), "expected fonts parsed from fonts_list.xml");

  if (fontsDB != nullptr) {
    CFGS_CHECK(fontsDB->getFontFaceCount() == expectedFonts.size(),
               "fontsDB count matches fonts_list.xml entries");

    const std::size_t maxToCheck = (fontsDB->getFontFaceCount() < expectedFonts.size())
                                       ? fontsDB->getFontFaceCount()
                                       : expectedFonts.size();

    for (std::size_t i = 0; i < maxToCheck; ++i) {
      const auto got = fontsDB->getFontFaceDescriptor(i);
      CFGS_CHECK(got != nullptr, "FontsDB entry exists");
      if (got == nullptr) continue;

      const auto &exp = expectedFonts[i];

      CFGS_LOG("FontsDB[%zu] got{name='%s', style=%d, filename='%s'}", i, got->name.c_str(),
               static_cast<int>(got->style), got->filename.c_str());
      CFGS_LOG("FontsDB[%zu] exp{name='%s', style=%d, filename='%s'}", i, exp.name.c_str(),
               static_cast<int>(exp.style), exp.filename.c_str());

      CFGS_CHECK(got->name == exp.name, "FontsDB name matches xml");
      CFGS_CHECK(got->style == exp.style, "FontsDB style matches xml");
      CFGS_CHECK(got->filename == exp.filename, "FontsDB filename matches xml");
    }
  }

  CFGS_CHECK(!cfg.isModified(), "isModified() is false after read-only fixture load");
}

int main() {
  CFGS_LOG("============================================");
  CFGS_LOG(" Config standalone verification");
  CFGS_LOG("============================================");

  testFixtureConfigRead();

  CFGS_LOG("--------------------------------------------");
  CFGS_LOG("RESULT: pass=%d fail=%d", s_pass, s_fail);
  CFGS_LOG("--------------------------------------------");

  return s_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
