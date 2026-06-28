#include "fonts_db.hpp"
#include <cinttypes>
#include <fstream>
#include <sys/stat.h>

auto FontsDB::getFile(const char *filename, size_t size) -> HimemUniquePtr<char[]> {
  HimemUniquePtr<char[]> buff = nullptr;

  std::ifstream          f(filename, std::ios::binary);

  if (f.is_open()) {
    buff = makeUniqueHimem<char[]>(size);
    if (buff) {
      f.read(buff.get(), static_cast<std::streamsize>(size));
    }
    if (!buff || (f.gcount() != static_cast<std::streamsize>(size))) {
      buff.reset();
    }
  }

  return buff;
}

auto FontsDB::checkFile(const HimemString &filename) -> bool {
  constexpr const char *TAG = "Check File";
  struct stat           fileStat;
  if (!filename.empty()) {
    HimemString fullName = filename;
    if (filename.front() != '/') {
      fullName = HimemString(FONTS_FOLDER "/").append(filename);
    }
    if (stat(fullName.c_str(), &fileStat) != -1) {
      fontSize += fileStat.st_size;
      if (fontSize > (1024 * 300)) {
        LOG_E("Font size too big for {}", filename);
      } else {
        return true;
      }
    } else {
      LOG_E("Font file can't be found: {}", fullName);
    }
  } else {
    LOG_E("Null string...");
  }
  return false;
}

auto FontsDB::add(const HimemString &name, FaceStyle style, const HimemString &filename) -> bool {

  struct stat statBuf;
  if (stat(filename.c_str(), &statBuf) == -1) {
    LOG_E("add: Unable to open font file '{}'", filename);
    return false;
  } else {
    int32_t length = statBuf.st_size;

    LOG_D("Font File Length: {}", length);

    std::ifstream fontFile(filename.c_str(), std::ios::binary);
    if (!fontFile.is_open()) {
      LOG_E("add: Unable to open font file '{}'", filename);
      return false;
    }

    auto buffer = makeUniqueHimem<uint8_t[]>(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: {}", length + 1);
      return false;
    }

    fontFile.read(reinterpret_cast<char *>(buffer.get()), static_cast<std::streamsize>(length));
    if (fontFile.gcount() != static_cast<std::streamsize>(length)) {
      LOG_E("add: Unable to read file content");
      return false;
    }

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

  struct stat statBuf;
  if (stat(filename.c_str(), &statBuf) == -1) {
    LOG_E("replace: Unable to open font file '{}'", filename);
    return false;
  } else {
    int32_t length = statBuf.st_size;

    LOG_D("Font File Length: {}", length);

    std::ifstream fontFile(filename.c_str(), std::ios::binary);
    if (!fontFile.is_open()) {
      LOG_E("replace: Unable to open font file '{}'", filename);
      return false;
    }

    auto buffer = makeUniqueHimem<uint8_t[]>(length + 1);

    if (buffer == nullptr) {
      LOG_E("Unable to allocate font buffer: {}", length + 1);
      return false;
    }

    fontFile.read(reinterpret_cast<char *>(buffer.get()), static_cast<std::streamsize>(length));
    if (fontFile.gcount() != static_cast<std::streamsize>(length)) {
      LOG_E("replace: Unable to read file content");
      return false;
    }

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

auto FontsDB::load(uint8_t configFontIndex) -> bool {

  constexpr static const char *xmlFontsDB = MAIN_FOLDER "/fonts_list.xml";

  pugi::xml_document           fd;
  HimemUniquePtr<char[]>       fdData = nullptr;
  struct stat                  fileStat;

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
      if (!(fnt && add("Icon", FaceStyle::ITALIC,
                       HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value())))) {
        LOG_E("Unable to find SYSTEM ICON font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("normal");
      if (!(fnt && add("System", FaceStyle::NORMAL,
                       HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value())))) {
        LOG_E("Unable to find SYSTEM NORMAL font.");
        return false;
      }

      fnt = sysGroup.find_child_by_attribute("font", "name", "TEXT").child("italic");
      if (!(fnt && add("System", FaceStyle::ITALIC,
                       HimemString(FONTS_FOLDER "/").append(fnt.attribute("filename").value())))) {
        LOG_E("Unable to find SYSTEM ITALIC font.");
        return false;
      }
    } else {
      LOG_E("SYSTEM group not found in fonts_list.xml.");
      return false;
    }

    uint8_t fontCount = 0;

    auto    userGroup = fd.child("fonts").find_child_by_attribute("group", "name", "USER");
    for (auto &fnt : userGroup.children("font")) {
      if (fontCount >= 8) { break; }
      HimemString str = fnt.attribute("name").value();

      if (!str.empty()) {
        fontSize = 0;
        LOG_D("{}...", str);
        fontNames[fontCount] = str;
        str                  = fnt.child("normal").attribute("filename").value();
        if (checkFile(str)) {
          regularFname[fontCount] = str;
          str                     = fnt.child("bold").attribute("filename").value();
          if (checkFile(str)) {
            boldFname[fontCount] = str;
            str                  = fnt.child("italic").attribute("filename").value();
            if (checkFile(str)) {
              italicFname[fontCount] = str;
              str                    = fnt.child("bold-italic").attribute("filename").value();
              if (checkFile(str)) {
                boldItalicFname[fontCount] = str;
                LOG_I("Font {} OK", fontNames[fontCount]);
                ++fontCount;
              }
            }
          }
        }
      }
    }

    LOG_D("Got {} fonts from xml file.", fontCount);

    if (fontCount == 0) {
      LOG_E("No USER font detected!");
      return false;
    }

    standardFontCount = fontCount;

    int8_t fontIndex = configFontIndex;
    if ((fontIndex < 0) || (fontIndex >= fontCount)) { fontIndex = 0; }

    HimemString normal     = HimemString(FONTS_FOLDER "/").append(regularFname[fontIndex]);
    HimemString bold       = HimemString(FONTS_FOLDER "/").append(boldFname[fontIndex]);
    HimemString italic     = HimemString(FONTS_FOLDER "/").append(italicFname[fontIndex]);
    HimemString boldItalic = HimemString(FONTS_FOLDER "/").append(boldItalicFname[fontIndex]);

    if (!add(fontNames[fontIndex], FaceStyle::NORMAL, normal)) { return false; }
    if (!add(fontNames[fontIndex], FaceStyle::BOLD, bold)) { return false; }
    if (!add(fontNames[fontIndex], FaceStyle::ITALIC, italic)) { return false; }
    if (!add(fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) { return false; }

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

    if (!replace(3, fontNames[fontIndex], FaceStyle::NORMAL, normal)) { return; }
    if (!replace(4, fontNames[fontIndex], FaceStyle::BOLD, bold)) { return; }
    if (!replace(5, fontNames[fontIndex], FaceStyle::ITALIC, italic)) { return; }
    if (!replace(6, fontNames[fontIndex], FaceStyle::BOLD_ITALIC, boldItalic)) { return; }

    LOG_D("Default font is now {}", fontNames[fontIndex]);
  }
}
