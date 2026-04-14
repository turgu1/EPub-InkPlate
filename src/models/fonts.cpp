// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FONTS__ 1
#include "models/fonts.hpp"

#include "alloc.hpp"
#include "pugixml.hpp"
#include "unzip.hpp"

#include "controllers/book_param_controller.hpp"
#include "controllers/option_controller.hpp"
#include "models/config.hpp"
#include "models/font_factory.hpp"

#include "viewers/form_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#include <algorithm>
#include <sys/stat.h>

using namespace pugi;

static constexpr const char *fontFnames[8] = {"CrimsonPro",  "Caladea",
                                              "Asap",        "AsapCondensed",
                                              "DejaVuSerif", "DejaVuSerifCondensed",
                                              "DejaVuSans",  "DejaVuSansCondensed"};

static constexpr const char *fontLabels[8] = {"CRIMSON S", "CALADEA S",  "ASAP",
                                              "ASAP COND", "DEJAVUE S",  "DEJAVUE COND S",
                                              "DEJAVU",    "DEJAVU COND"};

Fonts::Fonts() {
  #if USE_EPUB_FONTS
    fontCache.reserve(20);
  #else
    fontCache.reserve(4);
  #endif
}

auto Fonts::getFile(const char *filename, uint32_t size) -> char * {
  FILE *f    = fopen(filename, "r");
  char *buff = nullptr;

  if (f) {
    buff = (char *)allocate(size);
    if (buff && (fread(buff, size, 1, f) != 1)) {
      free(buff);
      buff = nullptr;
    }
    fclose(f);
  }

  return buff;
}

static uint32_t fontSize;
inline auto checkRes(xml_parse_result res) -> bool { return res.status == status_ok; }

static auto checkFile(const std::string &filename) -> bool {
  constexpr const char *TAG = "Check File";
  struct stat fileStat;
  if (!filename.empty()) {
    std::string fullName = std::string(FONTS_FOLDER "/").append(filename);
    if (stat(fullName.c_str(), &fileStat) != -1) {
      fontSize += fileStat.st_size;
      if (fontSize > (1024 * 300)) {
        LOG_E("Font size too big for %s", filename.c_str());
      } else
        return true;
    } else {
      LOG_E("Font file can't be found: %s", fullName.c_str());
    }
  } else {
    LOG_E("Null string...");
  }
  return false;
}

auto Fonts::filterFilename(std::string &fname) -> std::string & {
  std::size_t pos = fname.find("%DPI%");
  if (pos != std::string::npos) {
    char dpi[5];
    int_to_str(Screen::RESOLUTION, dpi, 5);
    fname.replace(pos, 5, dpi);
  }
  return fname;
}

auto Fonts::setup() -> bool {
  FontEntry fontEntry;
  struct stat fileStat;

  LOG_D("Fonts initialization");

  clearEverything();

  charPool = CharPool::Make();

  constexpr static const char *xmlFontsDescr = MAIN_FOLDER "/fonts_list.xml";

  xml_document fd;
  char *fdData = nullptr;

  if ((stat(xmlFontsDescr, &fileStat) != -1) &&
      ((fdData = getFile(xmlFontsDescr, fileStat.st_size)) != nullptr) &&
      (checkRes(fd.load_buffer_inplace(fdData, fileStat.st_size)))) {

    LOG_I("Reading fonts definition from fonts_list.xml...");

    // System fonts
    auto sysGroup = fd.child("fonts").find_child_by_attribute("group", "name", "SYSTEM");

    if (sysGroup) {
      auto fnt = sysGroup.find_child_by_attribute("font", "name", "ICON").child("normal");
      if (!(fnt &&
            add("Icon", FaceStyle::ITALIC,
                filterFilename(
                    std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ICON font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("normal");
      if (!(fnt &&
            add("System", FaceStyle::NORMAL,
                filterFilename(
                    std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM NORMAL font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("italic");
      if (!(fnt &&
            add("System", FaceStyle::ITALIC,
                filterFilename(
                    std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ITALIC font.");
        return false;
      }
    } else {
      LOG_E("SYSTEM group not found in fonts_list.xml.");
      return false;
    }

    fontCount      = 0;
    auto userGroup = fd.child("fonts").find_child_by_attribute("group", "name", "USER");
    for (auto fnt : userGroup.children("font")) {
      if (fontCount >= 8) break;
      std::string str = fnt.attribute("name").value();
      fontSize        = 0;
      if (!str.empty()) {
        LOG_D("%s...", str.c_str());
        fontNames[fontCount] = charPool->set(str);
        str                  = fnt.child("normal").attribute("filename").value();
        if (checkFile(str = filterFilename(str))) {
          regularFname[fontCount] = charPool->set(str);
          str                     = fnt.child("bold").attribute("filename").value();
          if (checkFile(str = filterFilename(str))) {
            boldFname[fontCount] = charPool->set(str);
            str                  = fnt.child("italic").attribute("filename").value();
            if (checkFile(str = filterFilename(str))) {
              italicFname[fontCount] = charPool->set(str);
              str                    = fnt.child("bold-italic").attribute("filename").value();
              if (checkFile(str = filterFilename(str))) {
                boldItalicFname[fontCount] = charPool->set(str);
                LOG_I("Font %s OK", fontNames[fontCount]);
                fontCount++;
              }
            }
          }
        }
      }
    }

    LOG_D("Got %d fonts from xml file.", fontCount);

    if (fontCount == 0) {
      LOG_E("No USER font detected!");
      return false;
    }

    FormChoiceField::adjustFontChoices(fontNames, fontCount);

    bookParamController.setFontCount(fontCount);
    optionController.setFontCount(fontCount);

    int8_t fontIndex = 0;
    config.get(Config::Ident::DEFAULT_FONT, &fontIndex);
    if ((fontIndex < 0) || (fontIndex >= fontCount)) fontIndex = 0;

    std::string normal     = std::string(FONTS_FOLDER "/").append(regularFname[fontIndex]);
    std::string bold       = std::string(FONTS_FOLDER "/").append(boldFname[fontIndex]);
    std::string italic     = std::string(FONTS_FOLDER "/").append(italicFname[fontIndex]);
    std::string boldItalic = std::string(FONTS_FOLDER "/").append(boldItalicFname[fontIndex]);

    if (!add(fontNames[fontIndex], FaceStyle::NORMAL, normal)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::BOLD, bold)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::ITALIC, italic)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) return false;

    fd.reset();
    free(fdData);
    return true;
  }

  LOG_E("Unable to read fonts definition file fonts_list.xml.");
  return false;
}

Fonts::~Fonts() {
  for (auto &entry : fontCache) {
    delete entry.font;
  }
  fontCache.clear();
}

auto Fonts::clear(bool all) -> void {
  std::scoped_lock guard(mutex);

  // LOG_D("Fonts Clear!");
  // Keep the first 7 fonts as they are reused. Caches will be cleared.
  #if USE_EPUB_FONTS
    int i = 0;
    for (auto &entry : fontCache) {
      if ((all && (i >= 3)) || (i >= 7))
        delete entry.font;
      else
        entry.font->clearCache();
      i++;
    }
    fontCache.resize(all ? 3 : 7);
    fontCache.reserve(20);
  #endif
}

auto Fonts::clearEverything() -> void {
  std::scoped_lock guard(mutex);

  charPool.reset();

  // LOG_D("Fonts Clear!");
  // Keep the first 7 fonts as they are reused. Caches will be cleared.
  for (auto &entry : fontCache) {
    delete entry.font;
  }
  fontCache.resize(0);
  fontCache.reserve(20);
}

auto Fonts::adjustDefaultFont(uint8_t fontIndex) -> void {
  if (fontCache.at(3).name.compare(fontNames[fontIndex]) != 0) {
    std::string normal     = std::string(FONTS_FOLDER "/").append(regularFname[fontIndex]);
    std::string bold       = std::string(FONTS_FOLDER "/").append(boldFname[fontIndex]);
    std::string italic     = std::string(FONTS_FOLDER "/").append(italicFname[fontIndex]);
    std::string boldItalic = std::string(FONTS_FOLDER "/").append(boldItalicFname[fontIndex]);

    if (!replace(3, fontNames[fontIndex], FaceStyle::NORMAL, normal)) return;
    if (!replace(4, fontNames[fontIndex], FaceStyle::BOLD, bold)) return;
    if (!replace(5, fontNames[fontIndex], FaceStyle::ITALIC, italic)) return;
    if (!replace(6, fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) return;

    LOG_D("Default font is now %s", fontNames[fontIndex]);
  }
}

auto Fonts::clearGlyphCaches() -> void {
  for (auto &entry : fontCache) {
    entry.font->clearCache();
  }
}

auto Fonts::getIndex(const std::string &name, FaceStyle style) -> int16_t {
  int16_t idx = 0;

  {
    std::scoped_lock guard(mutex);

    for (auto &entry : fontCache) {
      if ((entry.name.compare(name) == 0) && (entry.style == style)) return idx;
      idx++;
    }
  }
  return -1;
}

auto Fonts::replace(int16_t index, const std::string &name, FaceStyle style,
                    const std::string &filename) -> bool {
  std::scoped_lock guard(mutex);

  FontEntry f;
  if ((f.font = FontFactory::create(filename))) {
    if (f.font->isReady()) {
      f.name  = name;
      f.style = style;
      f.font->setFontsCacheIndex(index);
      delete fontCache.at(index).font;
      fontCache.at(index) = f;

      LOG_D("Font %s (%s) replacement at index %d and style %d.", f.name.c_str(), filename.c_str(),
            f.font->getFontsCacheIndex(), (int)f.style);
      return true;
    } else {
      delete f.font;
    }
  } else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.outOfMemory("font allocation");
  }

  return false;
}

auto Fonts::add(const std::string &name, FaceStyle style, const std::string &filename) -> bool {
  std::scoped_lock guard(mutex);

  // If the font is already loaded, return promptly
  for (auto &font : fontCache) {
    if ((name.compare(font.name) == 0) && (font.style == style)) return true;
  }

  FontEntry f;
  if ((f.font = FontFactory::create(filename))) {
    if (f.font->isReady()) {
      f.name  = name;
      f.style = style;
      f.font->setFontsCacheIndex(fontCache.size());
      fontCache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.", f.name.c_str(),
            f.font->getFontsCacheIndex(), (int)f.style);
      return true;
    } else {
      delete f.font;
    }
  } else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.outOfMemory("font allocation");
  }

  return false;
}

auto Fonts::add(const std::string &name, FaceStyle style, MemoryFontPtr buffer, int32_t size,
                const std::string &filename) -> bool {
  std::scoped_lock guard(mutex);

  // If the font is already loaded, return promptly
  for (auto &font : fontCache) {
    if ((name.compare(font.name) == 0) && (font.style == style)) return true;
  }

  FontEntry f;

  if ((f.font = FontFactory::create(filename, std::move(buffer), size))) {
    if (f.font->isReady()) {
      f.name  = name;
      f.style = style;
      f.font->setFontsCacheIndex(fontCache.size());
      fontCache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.", f.name.c_str(),
            f.font->getFontsCacheIndex(), (int)f.style);
      return true;
    } else {
      delete f.font;
    }
  } else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.outOfMemory("font allocation");
  }

  return false;
}

auto Fonts::adjustFontStyle(FaceStyle style, FaceStyle fontStyle, FaceStyle fontWeight) const
    -> Fonts::FaceStyle {
  if (fontStyle == FaceStyle::ITALIC) {
    // NORMAL -> ITALIC
    // BOLD -> BOLD_ITALIC
    // ITALIC (no change)
    // BOLD_ITALIC (no change)
    if (style == FaceStyle::NORMAL)
      style = FaceStyle::ITALIC;
    else if (style == FaceStyle::BOLD)
      style = FaceStyle::BOLD_ITALIC;
  } else if (fontStyle == FaceStyle::NORMAL) {
    // NORMAL
    // BOLD
    // ITALIC -> NORMAL
    // BOLD_ITALIC -> BOLD
    if (style == FaceStyle::BOLD_ITALIC)
      style = FaceStyle::BOLD;
    else if (style == FaceStyle::ITALIC)
      style = FaceStyle::NORMAL;
  }
  if (fontWeight == FaceStyle::BOLD) {
    // NORMAL -> BOLD
    // BOLD
    // ITALIC -> BOLD_ITALIC
    // BOLD_ITALIC
    if (style == FaceStyle::ITALIC)
      style = FaceStyle::BOLD_ITALIC;
    else if (style == FaceStyle::NORMAL)
      style = FaceStyle::BOLD;
  } else if (fontWeight == FaceStyle::NORMAL) {
    // NORMAL
    // BOLD -> NORMAL
    // ITALIC
    // BOLD_ITALIC -> ITALIC
    if (style == FaceStyle::BOLD)
      style = FaceStyle::NORMAL;
    else if (style == FaceStyle::BOLD_ITALIC)
      style = FaceStyle::ITALIC;
  }

  return style;
}