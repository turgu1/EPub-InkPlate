// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/epub.hpp"

#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/fonts.hpp"
#include "viewers/book_viewer.hpp"
#include "viewers/msg_viewer.hpp"

#include "picture_factory.hpp"
#include "unzip.hpp"

#include "logging.hpp"
#if EPUB_INKPLATE_BUILD
  #include "esp_heap_caps.h"
#else
  #include <openssl/sha.h>
#endif

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sys/stat.h>

using namespace pugi;

const char *TAG = "EPUB";

auto packagePred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "package") == 0) || (strcmp(node.name(), "opf:package") == 0);
  LOG_D("package() result: %d", res);
  return res;
}

auto metadataPred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "metadata") == 0) || (strcmp(node.name(), "opf:metadata") == 0);
  LOG_D("metadata() result: %d", res);
  return res;
}

auto manifestPred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "manifest") == 0) || (strcmp(node.name(), "opf:manifest") == 0);
  LOG_D("manifest() result: %d", res);
  return res;
}

auto itemPred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "item") == 0) || (strcmp(node.name(), "opf:item") == 0);
  LOG_D("item() result: %d", res);
  return res;
}

auto spinePred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "spine") == 0) || (strcmp(node.name(), "opf:spine") == 0);
  LOG_D("spine() result: %d", res);
  return res;
}

auto itemrefPred(xml_node node) -> bool {
  bool res = (strcmp(node.name(), "itemref") == 0) || (strcmp(node.name(), "opf:itemref") == 0);
  LOG_D("itemref() result: %d", res);
  return res;
}

auto xmlnsPred(xml_attribute attr) -> bool {
  bool res = (strcmp(attr.name(), "xmlns") == 0) || (strcmp(attr.name(), "xmlns:opf") == 0);
  LOG_D("xmlns() result: %d", res);
  return res;
}

auto oneByAttr(xml_node n, const char *name1, const char *name2, const char *attr,
               const char *value) -> xml_node {
  xml_node res;

  if (!(res = n.find_child_by_attribute(name1, attr, value))) {
    if ((res = n.find_child_by_attribute(name2, attr, value))) {
      LOG_D("one by attr: %s Found", name2);
    } else
      LOG_D("one by attr: %s NOT Found", name2);
  } else
    LOG_D("one by attr: %s Found", name1);

  return res;
}

EPub::~EPub() { closeFile(); }

void extractPath(const char *fname, std::string &path) {
  path.clear();
  int i = strlen(fname) - 1;
  while ((i > 0) && (fname[i] != '/')) i--;
  if (i > 0) path.assign(fname, ++i);
}

auto EPub::checkMimetype() -> bool {
  FileContentPtr data;
  uint32_t size;

  // A file named 'mimetype' must be present and must contain the
  // string 'application/epub+zip'

  LOG_D("Check mimetype.");
  if (!(data = unzip.getFile("mimetype", size))) return false;
  if (strncmp(reinterpret_cast<const char *>(data.get()), "application/epub+zip", 20)) {
    LOG_E("This is not an EPUB ebook format.");
    return false;
  }

  return true;
}

#define ERR(e)                                                                                     \
  {                                                                                                \
    err = e;                                                                                       \
    break;                                                                                         \
  }

auto EPub::getFileObfuscation(const char *filename) -> ObfuscationType {
  ObfuscationType obfType = ObfuscationType::NONE;

  if (encryptionIsPresent()) {
    for (auto &n : encryption.child("encryption").children("enc:EncryptedData")) {
      if (strcmp(n.child("enc:CipherData").child("enc:CipherReference").attribute("URI").value(),
                 filename) == 0) {
        xml_attribute attr = n.child("enc:EncryptionMethod").attribute("Algorithm");
        if (strcmp(attr.value(), "http://ns.adobe.com/pdf/enc#RC") == 0) {
          obfType = ObfuscationType::ADOBE;
        } else if (strcmp(attr.value(), "http://www.idpf.org/2008/embedding") == 0) {
          obfType = ObfuscationType::IDPF;
        } else
          obfType = ObfuscationType::UNKNOWN;
        break;
      }
    }
  }

  return obfType;
}

auto EPub::getEncryptionXml() -> bool {
  static constexpr const char *fname = "META-INF/encryption.xml";

  uint32_t size;

  encryptionPresent = false;

  if ((unzip.fileExists(fname) && (encryptionData = unzip.getFile(fname, size)) != nullptr)) {
    xml_parse_result res = encryption.load_buffer_inplace(encryptionData.get(), size);
    if (res.status != status_ok) {
      LOG_E("encryption.xml load error: %d", res.status);
      return false;
    }

    if ((strcmp(encryption.child("encryption").attribute("xmlns").value(),
                "urn:oasis:names:tc:opendocument:xmlns:container") != 0) ||
        (strcmp(encryption.child("encryption").attribute("xmlns:enc").value(),
                "http://www.w3.org/2001/04/xmlenc#") != 0)) {

      LOG_E("encryption.xml file format not supported.");

      encryption.reset();
      return false;
    }
    getKeys();
    encryptionPresent = true;
  }

  return true;
}

auto EPub::getOpfFilename(std::string &filename) -> bool {
  int err = 0;
  uint32_t size;

  // A file named 'META-INF/container.xml' must be present and point to the OPF file
  LOG_D("Check container.xml.");
  auto data = unzip.getFile("META-INF/container.xml", size);

  if (!data) return false;

  xml_document doc;
  xml_node node;
  xml_attribute attr;

  xml_parse_result res = doc.load_buffer_inplace(data.get(), size);
  if (res.status != status_ok) {
    LOG_E("xml load error: %d", res.status);
    return false;
  }

  bool completed = false;
  while (!completed) {
    if (!(node = doc.child("container"))) ERR(1);
    if (!((attr = node.attribute("version")) && (strcmp(attr.value(), "1.0") == 0))) ERR(2);
    if (!(attr = node.child("rootfiles")
                     .find_child_by_attribute("rootfile", "media-type",
                                              "application/oebps-package+xml")
                     .attribute("full-path")))
      ERR(3);

    filename.assign(attr.value());
    completed = true;
  }

  if (!completed) {
    LOG_E("EPub getOpf error: %d", err);
  }

  doc.reset();

  return completed;
}

auto EPub::getUniqueIdentifier() -> std::string {
  xml_attribute attr;
  xml_node node, node2;
  const char *id;

  if ((node = opf.find_child(packagePred)) && (id = node.attribute("unique-identifier").value()) &&
      (node2 = node.find_child(metadataPred)) &&
      (node2.find_child_by_attribute("dc:identifier", "id", id))) {
    return node2.text().get();
  }
  return "";
}

auto hexToBin(char ch) -> uint8_t {
  if ((ch >= '0') && (ch <= '9')) return ch - '0';
  if ((ch >= 'A') && (ch <= 'F')) return ch - 'A' + 10;
  if ((ch >= 'a') && (ch <= 'f')) return ch - 'a' + 10;
  return 0;
}

inline auto validHex(char ch) -> bool {
  return (((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')) ||
          ((ch >= 'a') && (ch <= 'f')));
}

inline auto toBin(const char *from, uint8_t *to) -> bool {
  if (validHex(from[0]) && validHex(from[1])) {
    *to = (hexToBin(from[0]) << 4) + hexToBin(from[1]);
    return true;
  } else
    return false;
}

#if EPUB_INKPLATE_BUILD
  #include "mbedtls/md.h"

  auto EPub::sha1(const std::string &data) -> void {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (unsigned char *)data.c_str(), data.length());
    mbedtls_md_finish(&ctx, (unsigned char *)&shaUuid);
    mbedtls_md_free(&ctx);
  }
#else
  #include <openssl/sha.h>

  auto EPub::sha1(const std::string &data) -> void {
    SHA1((unsigned char *)data.c_str(), data.length(), (uint8_t *)&shaUuid);
  }
#endif

auto EPub::getKeys() -> bool {
  std::string unique_id = getUniqueIdentifier();
  const char *str       = unique_id.c_str();

  if (unique_id.empty()) return false;
  uint8_t pos = (unique_id.substr(0, 9) == "urn:uuid:") ? 9 : 0;

  // Basic validity checks
  if ((unique_id.length() == (size_t)(pos + 36)) && (str[pos + 8] == '-') &&
      (str[pos + 13] == '-') && (str[pos + 18] == '-') && (str[pos + 23] == '-')) {

    // Convert it to binary big-endien version
    static const uint8_t idxs[16] = {0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34};
    for (uint8_t idx = 0; idx < 16; idx++) {
      if (!toBin(&str[pos + idxs[idx]], &binUuid[idx])) return false;
    }
  }

  unique_id.erase(std::remove_if(unique_id.begin(), unique_id.end(), ::isspace), unique_id.end());
  sha1(unique_id);

  return true;
}

auto EPub::getOpf(std::string &filename) -> bool {
  int err = 0;
  uint32_t size;

  xml_node node;
  xml_attribute attr;

  bool completed = false;
  while (!completed) {
    extractPath(filename.c_str(), opfBasePath);
    LOG_D("opfBasePath: %s", opfBasePath.c_str());

    if (!(opfData = unzip.getFile(filename.c_str(), size))) ERR(6);

    xml_parse_result res = opf.load_buffer_inplace(opfData.get(), size);
    if (res.status != status_ok) {
      LOG_E("xml load error: %d", res.status);
      opf.reset();
      opfData.reset();
      return false;
    }

    // Verifie that the OPF is of one of the version understood by this application
    if (!((node = opf.find_child(packagePred)) && (attr = node.find_attribute(xmlnsPred)) &&
          (strcmp(attr.value(), "http://www.idpf.org/2007/opf") == 0) &&
          (attr = node.attribute("version")) &&
          ((strcmp(attr.value(), "1.0") == 0) || (strcmp(attr.value(), "2.0") == 0) ||
           (strcmp(attr.value(), "3.0") == 0)))) {
      LOG_E("This book is not compatible with this software.");
      break;
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub getOpf error: %d", err);
    opf.reset();
    opfData.reset();
  }

  LOG_D("getOpf() completed.");

  return completed;
}

auto EPub::filenameLocate(const char *fname) -> std::string {
  char name[256];
  uint8_t idx   = 0;
  const char *s = fname;
  while ((idx < 255) && (*s != 0)) {
    if (*s == '%') {
      name[idx++] = (hexToBin(s[1]) << 4) + hexToBin(s[2]);
      s += 3;
    } else if ((*s == '/') && (s[1] == '.') && (s[2] == '.') && (s[3] == '/')) {
      while (idx > 0) {
        idx -= 1;
        if (name[idx] == '/') break;
      }
      s += idx > 0 ? 3 : 4;
    } else {
      name[idx++] = *s++;
    }
  }
  name[idx] = 0;

  std::string filename = opfBasePath;
  filename.append(name);

  return filename;
}

auto EPub::retrieveFile(const char *fname, uint32_t &size) -> FileContentPtr {
  // Cleanup the filename that can contain characters as hexadecimal values
  // stating with '%' and relative folder change using '../'

  LOG_D("Retrieving file %s", fname);

  std::string filename = filenameLocate(fname);

  // LOG_D("Retrieving file %s", filename.c_str());

  auto str = unzip.getFile(filename.c_str(), size);

  return str;
}

auto EPub::loadFonts() -> void {
  for (auto &css : cssCache) {
    retrieveFontsFromCss(css);
  }
}

auto EPub::decrypt(void *buffer, const uint32_t size, ObfuscationType obfType) -> void {
  uint16_t decryptLength;
  uint8_t *key;
  uint8_t keySize;

  if (obfType == ObfuscationType::ADOBE) {
    decryptLength = 1024;
    keySize       = 16;
    key           = (uint8_t *)&binUuid;
  } else if (obfType == ObfuscationType::IDPF) {
    decryptLength = 1040;
    keySize       = 20;
    key           = (uint8_t *)&shaUuid;
  } else
    return;

  uint16_t length = (size > decryptLength) ? decryptLength : size;
  uint8_t keyIdx  = 0;
  uint8_t *str    = (uint8_t *)buffer;

  while (length--) {
    *str++ ^= key[keyIdx++];
    if (keyIdx >= keySize) keyIdx = 0;
  }
}

auto EPub::loadFont(const std::string filename, const std::string fontFamily,
                    const Fonts::FaceStyle style) -> bool {
  uint32_t size;
  LOG_D("Font file name: %s", filename.c_str());
  if ((size = unzip.getFileSize(filename.c_str())) > 0) {
    if ((fontsSize + size) > 800000) {
      fontsSizeTooLarge = true;
      LOG_E("Fonts are using too much space (max 800K). Kept the first fonts read.");
    } else {
      FileContentPtr buffer;
      ObfuscationType obfType = getFileObfuscation(filename.c_str());
      if (obfType != ObfuscationType::NONE) {
        if (obfType != ObfuscationType::UNKNOWN) {
          buffer = unzip.getFile(filename.c_str(), size);
          if (buffer == nullptr) {
            LOG_E("Unable to retrieve font file: %s", filename.c_str());
          } else {
            decrypt(buffer.get(), size, obfType);

            if (fonts.add(fontFamily, style, std::move(buffer), size, filename)) {
              fontsSize += size;
              return true;
            }
          }
        } else {
          LOG_E("Font %s obfuscated with an unknown algorithm.", filename.c_str());
        }
      } else {
        buffer = unzip.getFile(filename.c_str(), size);
        if (buffer == nullptr) {
          LOG_E("Unable to retrieve font file: %s", filename.c_str());
        } else {
          if (fonts.add(fontFamily, style, std::move(buffer), size, filename)) {
            fontsSize += size;
            return true;
          }
        }
      }
    }
  }

  return false;
}

auto EPub::retrieveFontsFromCss(CSSPtr &css) -> void {
  LOG_D("retrieveFontsFromCss()");

  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  #if USE_EPUB_FONTS

    if ((bookFormatParams.useFontsInBook == 0) || (fontsSizeTooLarge)) return;

    CSS::RulesMap font_rules;

    auto dom = DOM::Make();
    if (dom == nullptr) {
      LOG_E("Failed to allocate DOM object for font extraction");
      return;
    }

    auto ff = dom->addChild(dom->body, DOM::Tag::FONT_FACE);
    if (ff == nullptr) {
      LOG_E("Failed to add FONT_FACE child to DOM");
      dom.reset();
      return;
    }

    css->match(ff, font_rules);

    dom.reset();

    if (font_rules.empty()) return;

    bool first = true;

    for (auto &rule : font_rules) {
      const CSS::Values *values;
      if ((values = css->getValuesFromProps(*rule.second, CSS::PropertyId::FONT_FAMILY))) {

        Fonts::FaceStyle style       = Fonts::FaceStyle::NORMAL;
        Fonts::FaceStyle font_weight = Fonts::FaceStyle::NORMAL;
        Fonts::FaceStyle font_style  = Fonts::FaceStyle::NORMAL;
        std::string fontFamily       = values->front()->str;

        if ((values = css->getValuesFromProps(*rule.second, CSS::PropertyId::FONT_STYLE))) {
          font_style = (Fonts::FaceStyle)values->front()->choice.faceStyle;
        }
        if ((values = css->getValuesFromProps(*rule.second, CSS::PropertyId::FONT_WEIGHT))) {
          font_weight = (Fonts::FaceStyle)values->front()->choice.faceStyle;
        }
        style = fonts.adjustFontStyle(style, font_style, font_weight);
        // LOG_D("Style: %d text-style: %d text-weight: %d", style, font_style, font_weight);

        if (fonts.getIndex(fontFamily.c_str(), style) == -1) { // If not already loaded
          if ((values = css->getValuesFromProps(*rule.second, CSS::PropertyId::SRC)) &&
              (!values->empty()) && (values->front()->valueType == CSS::ValueType::URL)) {

            if (!pageLocsInstance) {
              if (first) {
                first = false;
                LOG_D("Displaying font loading msg.");
                MsgViewer::show(
                    MsgViewer::MsgType::INFO, false, false, "Retrieving Font(s)",
                    "The application is retrieving font(s) from the EPub file. Please wait.");
              }
            }

            std::string filename = css->getFolderPath() + values->front()->str;
            filename             = filenameLocate(filename.c_str());

            loadFont(filename, fontFamily, style);
            if (fontsSizeTooLarge) break;
          }
        }
      }
    }
  #endif

  LOG_D("end of retrieveFontsFromCss()");

  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
}

auto EPub::retrieveCss(ItemInfo &item) -> void {

  // Retrieve css files, puting them in the cssCache vector (as a cache).
  // The properties are then merged into the current_css map for the item
  // being processed.

  LOG_D("retrieveCss()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif

  xml_node node;
  xml_attribute attr;

  if ((node = item.xmlDoc.child("html").child("head").child("link"))) {
    do {
      if ((attr = node.attribute("type")) && (strcmp(attr.value(), "text/css") == 0) &&
          (attr = node.attribute("href"))) {

        std::string css_id = attr.value(); // uses href as id

        // search the list of css files to see if it already been parsed
        int16_t idx       = 0;
        auto css_cache_it = cssCache.begin();

        while (css_cache_it != cssCache.end()) {
          if ((*css_cache_it)->getId().compare(css_id) == 0) break;
          css_cache_it++;
          idx++;
        }
        if (css_cache_it == cssCache.end()) {

          // The css file was not found. Load it in the cache.
          uint32_t size;
          std::string fname = item.filePath;
          fname.append(css_id.c_str());
          auto data = retrieveFile(fname.c_str(), size);

          if (data != nullptr) {
            #if COMPUTE_SIZE
              memory_used += size;
            #endif
            LOG_D("CSS Filename: %s", fname.c_str());
            std::string path;
            extractPath(fname.c_str(), path);
            auto css_tmp = CSS::Make(css_id.c_str(), path.c_str(), (char *)data.get(), size, 0);
            if (css_tmp == nullptr) MsgViewer::outOfMemory("css temp allocation");

            // #if DEBUGGING
            //   css_tmp->show();
            // #endif

            retrieveFontsFromCss(css_tmp);
            cssCache.push_back(std::move(css_tmp));
            item.cssList.push_back(*cssCache.back());
          }
        } else {
          item.cssList.push_back(**css_cache_it); // The css file was found in the cache. Just add
                                                  // it to the list of css for the current item.
        }
      }
    } while ((node = node.next_sibling("link")));
  }

  // Now look at <style> tags presents in the <html><head>, creating a temporary
  // css object for each of them.

  if ((node = item.xmlDoc.child("html").child("head").child("style"))) {
    do {
      xml_node sub = node.first_child();
      const char *buffer;
      if (sub != nullptr) {
        buffer = sub.value();
      } else {
        buffer = node.child_value();
      }
      auto css_tmp = CSS::Make("current-item", item.filePath.c_str(), buffer, strlen(buffer), 1);
      if (!css_tmp) MsgViewer::outOfMemory("css temp allocation");
      retrieveFontsFromCss(css_tmp);
      // css_tmp->show();
      item.cssCache.push_back(std::move(css_tmp));
    } while ((node = node.next_sibling("style")));
  }

  // Populate the current item css structure with property suites present in
  // the identified css files in the <meta> portion of the html file.

  if (!(item.css = CSS::Make("MergedForItem"))) {
    MsgViewer::outOfMemory("css allocation");
  }
  for (auto &css : item.cssList) item.css->retrieveDataFromCss(css);
  for (auto &css : item.cssCache) item.css->retrieveDataFromCss(*css.get());

  // item.css->show();
  LOG_D("end of retrieveCss()");
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
}

auto EPub::getItem(pugi::xml_node itemref, ItemInfo &item) -> bool {
  int err = 0;
  #define ERR(e)                                                                                   \
    {                                                                                              \
      err = e;                                                                                     \
      break;                                                                                       \
    }

  if (!fileIsOpen) return false;

  // if ((item.data != nullptr) && (currentItemRef == itemref))
  // return true;

  clearItemData(item);

  xml_node node, node2;
  xml_attribute attr;

  const char *id = itemref.attribute("idref").value();

  bool completed = false;

  while (!completed) {
    if (!((node = opf.find_child(packagePred).find_child(manifestPred)) &&
          (node = oneByAttr(node, "item", "opf:item", "id", id))))
      ERR(1);

    if (!(attr = node.attribute("media-type"))) ERR(2);
    const char *mediaType = attr.value();

    if (strcmp(mediaType, "application/xhtml+xml") == 0)
      item.mediaType = MediaType::XML;
    else if (strcmp(mediaType, "picture/jpeg") == 0)
      item.mediaType = MediaType::JPEG;
    else if (strcmp(mediaType, "picture/png") == 0)
      item.mediaType = MediaType::PNG;
    else if (strcmp(mediaType, "picture/bmp") == 0)
      item.mediaType = MediaType::BMP;
    else if (strcmp(mediaType, "picture/gif") == 0)
      item.mediaType = MediaType::GIF;
    else
      ERR(3);

    if (!(attr = node.attribute("href"))) ERR(5);

    LOG_D("Retrieving file %s", attr.value());

    uint32_t size;
    extractPath(attr.value(), item.filePath);

    // LOG_D("item.filePath: %s.", item.filePath.c_str());

    if ((item.data = retrieveFile(attr.value(), size)) == nullptr) ERR(6);

    if (item.mediaType == MediaType::XML) {

      char *str;
      while ((str = strstr((char *)item.data.get(), "/*<![CDATA[*/")) != nullptr) {
        *str++ = ' ';
        *str   = ' ';
        str += 10;
        *str++ = ' ';
        *str   = ' ';
      }
      while ((str = strstr((char *)item.data.get(), "/*]]>*/")) != nullptr) {
        *str++ = ' ';
        *str   = ' ';
        str += 4;
        *str++ = ' ';
        *str   = ' ';
      }
      LOG_D("Reading file %s", attr.value());

      xml_parse_result res = item.xmlDoc.load_buffer_inplace(item.data.get(), size);
      if (res.status != status_ok) {
        LOG_E("item_doc xml load error: %d", res.status);
        // msg_viewer.show(
        //   MsgViewer::MsgType::ALERT,
        //   true, false,
        //   "XML Error in eBook.",
        //   "File %s contains XHTML errors and cannot be loaded.",
        //   attr.value()
        // );
        item.xmlDoc.reset();
        if (item.data != nullptr) {
          item.data.reset();
        }
        return false;
      }

      // current_item.parse<0>(current_item_data);

      // if (css->size() > 0) css->clear();

      retrieveCss(item);
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub getCurrentItem error: %d", err);
    clearItemData(item);
  }
  return completed;
}

auto EPub::updateBookFormatParams() -> void {
  constexpr int8_t default_value = -1;

  if (bookParams == nullptr) {
    bookFormatParams = {.ident = Screen::IDENT,
                        .orientation =
                            0, // Get de compiler happy (no warning). Will be set below...
                        .showTitle      = 0, // ... idem ...
                        .showPictures   = default_value,
                        .fontSize       = default_value,
                        .useFontsInBook = default_value,
                        .font           = default_value};
  } else {
    bookParams->get(BookParams::Ident::SHOW_PICTURES, &bookFormatParams.showPictures);
    bookParams->get(BookParams::Ident::FONT_SIZE, &bookFormatParams.fontSize);
    bookParams->get(BookParams::Ident::USE_FONTS_IN_BOOK, &bookFormatParams.useFontsInBook);
    bookParams->get(BookParams::Ident::FONT, &bookFormatParams.font);
  }

  config.get(Config::Ident::ORIENTATION, &bookFormatParams.orientation);
  config.get(Config::Ident::SHOW_TITLE, &bookFormatParams.showTitle);

  if (bookFormatParams.showPictures == default_value)
    config.get(Config::Ident::SHOW_PICTURES, &bookFormatParams.showPictures);
  if (bookFormatParams.fontSize == default_value)
    config.get(Config::Ident::FONT_SIZE, &bookFormatParams.fontSize);
  if (bookFormatParams.useFontsInBook == default_value)
    config.get(Config::Ident::USE_FONTS_IN_BOOKS, &bookFormatParams.useFontsInBook);
  if (bookFormatParams.font == default_value)
    config.get(Config::Ident::DEFAULT_FONT, &bookFormatParams.font);

  // if (!bookFormatParams.useFontsInBook) fonts.clear();
}

auto EPub::openParams(const std::string &epubFilename) -> void {
  std::string paramsFilename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".pars";
  bookParams                 = new BookParams(paramsFilename, false);
  if (bookParams != nullptr) {
    bookParams->read();
  }
}

auto EPub::open(const std::string &epubFilename) -> bool {
  if (fileIsOpen && (currentFilename == epubFilename)) return true;
  if (fileIsOpen) closeFile();

  #if COMPUTE_SIZE
    memory_used = 0;
  #endif

  LOG_D("Opening EPub file through unzip...");
  if (!unzip.openZipFile(epubFilename.c_str())) {
    LOG_E("EPub open_file: Unable to open zip file: %s", epubFilename.c_str());
    return false;
  }

  if (!checkMimetype()) return false;

  LOG_D("Getting the OPF file");
  std::string filename;
  if (!getOpfFilename(filename)) return false;

  if (!getOpf(filename)) {
    LOG_E("EPub open_file: Unable to get opf of %s", epubFilename.c_str());
    unzip.closeZipFile();
    return false;
  }

  getEncryptionXml();

  openParams(epubFilename);
  updateBookFormatParams();

  fonts.adjustDefaultFont(bookFormatParams.font);

  clearItemData(currentItemInfo);

  if (pageLocsInstance) {
    // PageLocs will regenerate the table of content. Remove the existing TOC file and
    // prepare the TOC object for that.
    toc = TOC::Make();
    if (toc == nullptr) {
      LOG_E("Unable to allocate memory for the book table of content.");
      return false;
    }

    toc->loadFromEpub(*this);

    auto filePath = epubFilename;
    auto dotPos   = filePath.find_last_of('.');

    if (dotPos != std::string::npos) {
      filePath.replace(dotPos, 5, ".toc");

      struct stat fileStat;
      if (stat(filePath.c_str(), &fileStat) != -1) {
        LOG_I("Deleting file : %s", filePath.c_str());
        unlink(filePath.c_str());
      }
    }
  }

  currentFilename   = epubFilename;
  fileIsOpen        = true;
  fontsSizeTooLarge = false;
  fontsSize         = 0;

  LOG_D("EPub file is now open.");

  return true;
}

auto EPub::clearItemData(ItemInfo &item) -> void {
  item.xmlDoc.reset();
  if (item.data != nullptr) {
    item.data.reset();
  }

  // for (auto * css : current_item_css_list) {
  //   delete css;
  // }
  item.cssList.clear();

  item.cssCache.clear();

  item.itemrefIndex = -1;
}

auto EPub::closeFile() -> bool {
  if (!fileIsOpen) return true;

  clearItemData(currentItemInfo);

  if (opfData) {
    opf.reset();
    opfData.reset();
  }

  opfBasePath.clear();

  if (encryptionData) {
    encryption.reset();
    encryptionData.reset();
  }

  if (!pageLocsInstance) {
    unzip.closeZipFile();
    fonts.clear();
  }

  cssCache.clear();

  fileIsOpen        = false;
  encryptionPresent = false;
  currentFilename.clear();

  if (bookParams != nullptr) {
    bookParams->save();
    delete bookParams;
    bookParams = nullptr;
  }

  return true;
}

auto EPub::getMeta(const std::string &name) -> const char * {
  if (!fileIsOpen) return nullptr;

  xml_node node;

  if ((node = opf.find_child(packagePred).find_child(metadataPred))) {
    return node.child_value(name.c_str());
  }
  return nullptr;

  // if (!((node = opf.child("package" ).child("metadata")))) {
  //   node = opf.child("package").child("opf:metadata");
  // }
  // return node == nullptr ? nullptr : node.child_value(name.c_str());
}

auto EPub::getCoverFilename() -> const char * {
  if (!fileIsOpen) return nullptr;

  xml_node node;
  xml_attribute attr;

  const char *itemref  = nullptr;
  const char *filename = nullptr;

  // First, try to find its from metadata

  if ((node = opf.find_child(packagePred).find_child(metadataPred)) &&
      (node = oneByAttr(node, "meta", "opf:meta", "name", "cover")) &&
      (itemref = node.attribute("content").value())) {

    for (auto n : opf.find_child(packagePred).find_child(manifestPred).children()) {
      if ((strcmp(n.name(), "item") == 0) || (strcmp(n.name(), "opf:item") == 0)) {
        if ((((attr = n.attribute("id")) && (strcmp(attr.value(), itemref) == 0)) ||
             ((attr = n.attribute("properties")) && (strcmp(attr.value(), itemref) == 0))) &&
            (attr = n.attribute("href"))) {
          filename = attr.value();
          break;
        }
      }
    }
  }

  if (filename == nullptr) {
    // Look inside manifest
    for (auto n : opf.find_child(packagePred).find_child(manifestPred).children()) {
      if ((strcmp(n.name(), "item") == 0) || (strcmp(n.name(), "opf:item") == 0)) {
        if ((attr = n.attribute("id")) &&
            ((strcmp(attr.value(), "cover-picture") == 0) ||
             (strcmp(attr.value(), "cover") == 0)) &&
            (attr = n.attribute("href"))) {
          filename = attr.value();
          break;
        }
      }
    }
  }

  return filename == nullptr ? "" : filename;
}

/// @brief Get the number of items in the spine of the book.
///
/// This is used to know how many pages the book has, and to be able to retrieve the page location
/// information for each page in the spine. The page location information is used to be able to jump
/// to a specific page in the book, and to be able to display the page number in the book viewer.
///
/// @return The number of items in the spine.
auto EPub::getItemCount() -> int16_t {
  if (!fileIsOpen) return 0;

  auto it       = opf.find_child(packagePred).find_child(spinePred).children("itemref");
  int16_t count = std::distance(it.begin(), it.end());

  if (count == 0) {
    it    = opf.find_child(packagePred).find_child(spinePred).children("opf:itemref");
    count = std::distance(it.begin(), it.end());
  }

  LOG_D("Item count: %d", count);
  return count;
}

auto EPub::getItemAtIndex(int16_t itemrefIndex) -> bool {
  if (!fileIsOpen) return false;

  if (currentItemInfo.itemrefIndex == itemrefIndex) return true;

  xml_node node = xml_node();
  int16_t index = 0;

  for (auto n : opf.find_child(packagePred).find_child(spinePred).children()) {
    if (index == itemrefIndex) {
      node = n;
      break;
    }
    index++;
  }

  if (node == nullptr) return false;

  bool res = false;

  if ((currentItemInfo.data == nullptr) || (currentItemRef != node)) {
    if ((res = getItem(node, currentItemInfo))) currentItemRef = node;
    currentItemInfo.itemrefIndex = itemrefIndex;
  }
  return res;
}

// This is in support of the pages location retrieval mechanism. The ItemInfo
// is being used to retrieve asynchroniously the book page numbers without
// interfering with the main book viewer thread.
auto EPub::getItemAtIndex(int16_t itemrefIndex, ItemInfo &item) -> bool {
  if (!fileIsOpen) return false;

  LOG_D("Mutex lock...");

  {
    std::scoped_lock guard(mutex);

    xml_node node = xml_node();
    int16_t index = 0;

    for (auto n : opf.find_child(packagePred).find_child(spinePred).children()) {
      if (index == itemrefIndex) {
        node = n;
        break;
      }
      index++;
    }

    bool res = false;

    if (node) {
      res               = getItem(node, item);
      item.itemrefIndex = itemrefIndex;
    }

    LOG_D("Mutex unlocked...");
    return res;
  }
}

auto EPub::getPicture(std::string &fname, bool load) -> PicturePtr {
  LOG_D("Mutex lock...");

  {
    std::scoped_lock guard(mutex);

    std::string filename = filenameLocate(fname.c_str());
    auto pict =
        PictureFactory::create(filename, Dim(Screen::getWidth(), Screen::getHeight()), load);

    if ((pict == nullptr) || (load && (pict->getBitmap() == nullptr)) ||
        (pict->getDim().height == 0) || (pict->getDim().width == 0)) {
      if (pict != nullptr) pict.reset();
      pict = nullptr;
    }

    // if (pict->getBitmap() != nullptr) {
    //   std::cout << "----- Picture content -----" << std::endl;
    //   for (int i = 0; i < 200; i++) {
    //     std::cout << std::hex << std::setw(2) << +pict->getBitmap()[i];
    //   }
    //   std::cout << std::endl << "-----" << std::endl;
    // }
    LOG_D("Mutex unlocked...");
    return pict;
  }
}