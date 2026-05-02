#include "models/fonts_db.hpp"

#include "screen.hpp"

#include "fonts_db.hpp"
#include <cinttypes>
#include <sys/stat.h>

auto FontsDB::getFile(const char *filename, size_t size) -> HimemUniquePtr<char[]> {
  FILE *f                     = fopen(filename, "r");
  HimemUniquePtr<char[]> buff = nullptr;

  if (f) {
    buff = makeUniqueHimem<char[]>(size);
    if (buff && (fread(buff.get(), size, 1, f) != 1)) {
      buff.reset();
    }
    fclose(f);
  }

  return buff;
}

auto FontsDB::checkFile(const HimemString &filename) -> bool {
  constexpr const char *TAG = "Check File";
  struct stat fileStat;
  if (!filename.empty()) {
    HimemString fullName = filename;
    if (filename.front() != '/') {
      fullName = HimemString(FONTS_FOLDER "/").append(filename);
    }
    if (stat(fullName.c_str(), &fileStat) != -1) {
      fontSize += fileStat.st_size;
      if (fontSize > (1024 * 300)) {
        LOG_E("Font size too big for %s", filename.c_str());
      } else {
        return true;
      }
    } else {
      LOG_E("Font file can't be found: %s", fullName.c_str());
    }
  } else {
    LOG_E("Null string...");
  }
  return false;
}

auto FontsDB::add(const HimemString &name, FaceStyle style, const HimemString &filename) -> bool {

  FILE *fontFile;

  if ((fontFile = fopen(filename.c_str(), "r")) == nullptr) {
    LOG_E("add: Unable to open font file '%s'", filename.c_str());
    return false;
  } else {
    struct stat statBuf;
    fstat(fileno(fontFile), &statBuf);
    int32_t length = statBuf.st_size;

    LOG_D("Font File Length: %" PRIi32, length);

    auto buffer = makeUniqueHimem<uint8_t[]>(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %" PRIi32, (int32_t)(length + 1));
      return false;
    }

    if (fread(buffer.get(), length, 1, fontFile) != 1) {
      LOG_E("add: Unable to read file content");
      fclose(fontFile);
      return false;
    }

    fclose(fontFile);

    buffer[length] = 0;

    auto descriptor          = FontFaceDescriptor::Make();
    descriptor->name         = name;
    descriptor->style        = style;
    descriptor->filename     = filename;
    descriptor->fontData     = std::move(buffer);
    descriptor->fontDataSize = length;

    fontFaceDescriptors.push_back(std::move(descriptor));

    return true;
  }
}

auto FontsDB::replace(int16_t index, const HimemString &name, FaceStyle style,
                      const HimemString &filename) -> bool {

  FILE *fontFile;

  if ((fontFile = fopen(filename.c_str(), "r")) == nullptr) {
    LOG_E("replace: Unable to open font file '%s'", filename.c_str());
    return false;
  } else {
    struct stat statBuf;
    fstat(fileno(fontFile), &statBuf);
    int32_t length = statBuf.st_size;

    LOG_D("Font File Length: %" PRIi32, length);

    auto buffer = makeUniqueHimem<uint8_t[]>(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: %" PRIi32, (int32_t)(length + 1));
      return false;
    }

    if (fread(buffer.get(), length, 1, fontFile) != 1) {
      LOG_E("replace: Unable to read file content");
      fclose(fontFile);
      return false;
    }

    fclose(fontFile);

    buffer[length] = 0;

    auto descriptor = FontFaceDescriptor::Make();

    descriptor->name         = name;
    descriptor->style        = style;
    descriptor->filename     = filename;
    descriptor->fontData     = std::move(buffer);
    descriptor->fontDataSize = length;

    fontFaceDescriptors[index] = std::move(descriptor);

    return true;
  }
}

auto FontsDB::filterFilename(HimemString &fname) -> HimemString & {
  std::size_t pos = fname.find("%DPI%");
  if (pos != HimemString::npos) {
    char dpi[5];
    int_to_str(Screen::RESOLUTION, dpi, 5);
    fname.replace(pos, 5, dpi);
  }
  return fname;
}

auto FontsDB::load(uint8_t configFontIndex) -> bool {

  constexpr static const char *xmlFontsDB = MAIN_FOLDER "/fonts_list.xml";

  pugi::xml_document fd;
  HimemUniquePtr<char[]> fdData = nullptr;
  struct stat fileStat;

  if ((stat(xmlFontsDB, &fileStat) != -1) &&
      ((fdData = getFile(xmlFontsDB, fileStat.st_size)) != nullptr) &&
      (checkRes(fd.load_buffer_inplace(fdData.get(), fileStat.st_size)))) {

    fontFaceDescriptors.clear();
    fontSize = 0;

    LOG_I("Reading fonts definition from fonts_list.xml...");

    // System fonts
    auto sysGroup = fd.child("fonts").find_child_by_attribute("group", "name", "SYSTEM");

    if (sysGroup) {
      auto fnt = sysGroup.find_child_by_attribute("font", "name", "ICON").child("normal");
      if (!(fnt &&
            add("Icon", FaceStyle::ITALIC,
                filterFilename(
                    HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ICON font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("normal");
      if (!(fnt &&
            add("System", FaceStyle::NORMAL,
                filterFilename(
                    HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM NORMAL font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("italic");
      if (!(fnt &&
            add("System", FaceStyle::ITALIC,
                filterFilename(
                    HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ITALIC font.");
        return false;
      }
    } else {
      LOG_E("SYSTEM group not found in fonts_list.xml.");
      return false;
    }

    uint8_t fontCount = 0;

    auto userGroup = fd.child("fonts").find_child_by_attribute("group", "name", "USER");
    for (auto &fnt : userGroup.children("font")) {
      if (fontCount >= 8) break;
      HimemString str = fnt.attribute("name").value();

      if (!str.empty()) {
        fontSize = 0;
        LOG_D("%s...", str.c_str());
        fontNames[fontCount] = str;
        str                  = fnt.child("normal").attribute("filename").value();
        if (checkFile(str = filterFilename(str))) {
          regularFname[fontCount] = str;
          str                     = fnt.child("bold").attribute("filename").value();
          if (checkFile(str = filterFilename(str))) {
            boldFname[fontCount] = str;
            str                  = fnt.child("italic").attribute("filename").value();
            if (checkFile(str = filterFilename(str))) {
              italicFname[fontCount] = str;
              str                    = fnt.child("bold-italic").attribute("filename").value();
              if (checkFile(str = filterFilename(str))) {
                boldItalicFname[fontCount] = str;
                LOG_I("Font %s OK", fontNames[fontCount].c_str());
                ++fontCount;
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

    standardFontCount = fontCount;

    int8_t fontIndex = configFontIndex;
    if ((fontIndex < 0) || (fontIndex >= fontCount)) fontIndex = 0;

    HimemString normal     = HimemString(FONTS_FOLDER "/").append(regularFname[fontIndex]);
    HimemString bold       = HimemString(FONTS_FOLDER "/").append(boldFname[fontIndex]);
    HimemString italic     = HimemString(FONTS_FOLDER "/").append(italicFname[fontIndex]);
    HimemString boldItalic = HimemString(FONTS_FOLDER "/").append(boldItalicFname[fontIndex]);

    if (!add(fontNames[fontIndex], FaceStyle::NORMAL, normal)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::BOLD, bold)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::ITALIC, italic)) return false;
    if (!add(fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) return false;

    fd.reset();

    return true;
  } else {
    LOG_E("Unable to read fonts_list.xml.");
  }
  return false;
}

auto FontsDB::adjustDefaultFont(uint8_t fontIndex) -> void {
  if (!isDefaultFont(fontIndex)) {
    HimemString normal     = HimemString(FONTS_FOLDER "/").append(regularFname[fontIndex]);
    HimemString bold       = HimemString(FONTS_FOLDER "/").append(boldFname[fontIndex]);
    HimemString italic     = HimemString(FONTS_FOLDER "/").append(italicFname[fontIndex]);
    HimemString boldItalic = HimemString(FONTS_FOLDER "/").append(boldItalicFname[fontIndex]);

    if (!replace(3, fontNames[fontIndex], FaceStyle::NORMAL, normal)) return;
    if (!replace(4, fontNames[fontIndex], FaceStyle::BOLD, bold)) return;
    if (!replace(5, fontNames[fontIndex], FaceStyle::ITALIC, italic)) return;
    if (!replace(6, fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) return;

    LOG_D("Default font is now %s", fontNames[fontIndex]);
  }
}
