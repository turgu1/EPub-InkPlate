// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "himem.hpp"
#include "picture.hpp"
#include "pugixml.hpp"
#include "unzip.hpp"

#include "models/book_params.hpp"
#include "models/css.hpp"
#include "viewers/page.hpp"

#include <forward_list>
#include <list>
#include <map>
#include <mutex>

class TOC;
class EPub;

using EPubPtr = himemUniquePtr<class EPub>;
class EPub {

public:
  enum class MediaType : uint8_t {
    XML, JPEG, PNG, GIF, BMP
  };

  enum class ObfuscationType : uint8_t {
    NONE, ADOBE, IDPF, UNKNOWN
  };

  using CSSList    = std::list<CSSPtr>;
  using CSSRefList = std::list<std::reference_wrapper<CSS>>;

  struct ItemInfo {
    std::string filePath{};
    int16_t itemrefIndex{};
    pugi::xml_document xmlDoc{};
    CSSList cssCache;   ///< style attributes part of the current processed item are kept here. They
                        ///< will be destroyed when the item is no longer required.
    CSSRefList cssList; ///< List of css sources for the current item file shown. Those are indexes
                        ///< inside cssCache.
    CSSPtr css{};       ///< Ghost CSS created through merging css suites from cssList and cssCache.
    FileContentPtr data{};
    MediaType mediaType{};

    ItemInfo()  = default;
    ~ItemInfo() = default;
  };

  // This struct contains the current parameters that influence
  // the rendering of e-book pages. Its content is constructed from
  // both the e-book's specific parameters and default configuration options.
  #pragma pack(push, 1)
  struct BookFormatParams {
    int8_t ident;       ///< Device identity (screen.hpp IDENT constant)
    int8_t orientation; ///< Config option only
    int8_t showTitle;
    int8_t showPictures;
    int8_t fontSize;
    int8_t useFontsInBook;
    int8_t font;
  };
  #pragma pack(pop)

  using BinUUID = uint8_t[16];
  using ShaUUID = uint8_t[20];

  himemUniquePtr<TOC> toc;

private:
  static constexpr char const *TAG = "EPub";

  std::recursive_timed_mutex mutex;

  pugi::xml_document opf; ///< The OPF document description.
  pugi::xml_document encryption;
  pugi::xml_node currentItemRef{pugi::xml_node(nullptr)};

  BinUUID binUuid{};
  ShaUUID shaUuid{};

  FileContentPtr opfData{nullptr};
  FileContentPtr encryptionData{nullptr};
  std::string currentFilename{""};
  std::string opfBasePath{""};

  ItemInfo currentItemInfo{};
  BookParams *bookParams{nullptr};
  BookFormatParams bookFormatParams{};

  CSSList cssCache; ///< All css files in the ebook are maintained here.

  bool fileIsOpen{false};
  bool encryptionPresent{false};
  bool fontsSizeTooLarge{false};
  int32_t fontsSize{0};
  bool pageLocsInstance{false};

  auto getMeta(const std::string &name) -> const char *;
  auto getOpf(std::string &filename) -> bool;
  auto checkMimetype() -> bool;
  auto getOpfFilename(std::string &filename) -> bool;
  void retrieveFontsFromCss(CSSPtr &css);
  auto getEncryptionXml() -> bool;
  void sha1(const std::string &data);

  EPub() = default;

public:
  ~EPub();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make() { return makeUniqueHimem<EPub>(); }

  void retrieveCss(ItemInfo &item);
  void loadFonts();
  void clearItemData(ItemInfo &item);
  void openParams(const std::string &epubFilename);
  auto open(const std::string &epubFilename) -> bool;
  auto closeFile() -> bool;
  auto getPicture(std::string &fname, bool load) -> PicturePtr;
  auto retrieveFile(const char *fname, uint32_t &size) -> himemUniquePtr<uint8_t[]>;
  auto getItem(pugi::xml_node itemref, ItemInfo &item) -> bool;
  auto getItemAtIndex(int16_t itemrefIndex) -> bool;
  auto getItemAtIndex(int16_t itemrefIndex, ItemInfo &item) -> bool;
  auto getUniqueIdentifier() -> std::string;
  auto getKeys() -> bool;
  auto filenameLocate(const char *fname) -> std::string;
  auto getItemCount() -> int16_t;
  void updateBookFormatParams();
  auto getFileObfuscation(const char *filename) -> ObfuscationType;
  void decrypt(void *buffer, const uint32_t size, ObfuscationType obfType);
  auto loadFont(const std::string filename, const std::string fontFamily,
                const Fonts::FaceStyle style) -> bool;
  /**
   * @brief Retrieve cover's filename
   *
   * Look inside the opf file to grab the cover filename. First search in the
   * metadata. If not found, search in the manifest for an entry with type
   * cover-picture
   *
   * @return char * filename, or nullptr if not found
   */
  auto getCoverFilename() -> const char *;

  // clang-format off
  inline auto getCssCache() const            -> const CSSList &            { return cssCache; }
  inline auto getCurrentItemCss() const      -> const CSSPtr &             { return currentItemInfo.css; }
  inline auto getCurrentItemInfo() const     -> const ItemInfo &           { return currentItemInfo; }
  inline auto getCurrentItemFilePath() const -> const std::string &        { return currentItemInfo.filePath; }
  inline auto getItemrefIndex() const        -> int16_t                    { return currentItemInfo.itemrefIndex; }
  inline auto getTitle()                     -> const char *               { return getMeta("dc:title"); }
  inline auto getAuthor()                    -> const char *               { return getMeta("dc:creator"); }
  inline auto getDescription()               -> const char *               { return getMeta("dc:description"); }
  inline auto getCurrentItem() const         -> const pugi::xml_document & { return currentItemInfo.xmlDoc; }
  inline auto getCurrentFilename()           -> std::string                { return currentFilename; }
  inline auto filenameIsEmpty()              -> bool                       { return currentFilename.empty(); }
  inline auto getBookParams()                -> BookParams *               { return bookParams; }
  inline auto getBookFormatParams()          -> BookFormatParams *         { return &bookFormatParams; }
  inline auto getOpfBasePath() const         -> const std::string &        { return opfBasePath; }
  inline auto getOpf()                       -> const pugi::xml_document & { return opf; }
  inline auto encryptionIsPresent() const    -> bool                       { return encryptionPresent; }
  inline auto getBinUuid() const             -> const BinUUID &            { return binUuid; }
  
  inline void setPageLocsInstance(bool val)                            { pageLocsInstance = val; }
  // clang-format on
};

#include "models/toc.hpp"