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
#include <memory>

class TOC;
class EPub;

using EPubPtr = HimemUniquePtr<class EPub>;
class EPub {

  public:
    enum class MediaType : uint8_t { XML, JPEG, PNG, GIF, BMP };

    enum class ObfuscationType : uint8_t { NONE, ADOBE, IDPF, UNKNOWN };

    using CSSList    = std::list<CSSPtr>;
    using CSSRefList = std::list<std::reference_wrapper<CSS> >;

    struct ItemInfo {
      HimemString filePath{};
      int16_t itemrefIndex{};
      pugi::xml_document xmlDoc{};
      CSSList cssCache; ///< style attributes part of the current processed item are kept here. They
                        ///< will be destroyed when the item is no longer required.
      CSSRefList cssList; ///< List of css sources for the current item file shown. Those are indexes
                          ///< inside cssCache.
      CSSPtr css{};     ///< Ghost CSS created through merging css suites from cssList and cssCache.
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
      int8_t ident;     ///< Device identity (screen.hpp IDENT constant)
      int8_t orientation; ///< Config option only
      int8_t showTitle;
      int8_t showPictures;
      int8_t fontSize;
      int8_t lineHeight;
      int8_t useFontsInBook;
      int8_t font;
      int8_t columnCount;
      Dim screenDim;
    };
    #pragma pack(pop)

    using BinUUID = uint8_t[16];
    using ShaUUID = uint8_t[20];

    HimemUniquePtr<TOC> toc;

  private:
    static constexpr char const *TAG = "EPub";

    static constexpr float lineHeightFactors[3] = { 0.75, 0.9, 1.2 };

    pugi::xml_document opf; ///< The OPF document description.
    pugi::xml_document encryption;
    pugi::xml_node currentItemRef{ pugi::xml_node(nullptr) };

    BinUUID binUuid{};
    ShaUUID shaUuid{};

    FileContentPtr opfData{ nullptr };
    FileContentPtr encryptionData{ nullptr };
    HimemString currentFilename{ "" };
    HimemString opfBasePath{ "" };

    ItemInfo currentItemInfo{};
    std::unique_ptr<BookParams> bookParams{ nullptr };
    BookFormatParams bookFormatParams{};

    Fonts fonts; ///< Fonts loaded from the e-book. They are used in priority to the default
                 ///< application fonts when the "use fonts in book" option is enabled. They are
                 ///< cleared when a new book is loaded.

    CSS::CSSPools cssPools;
    DOM::DOMPools domPools;

    CSSList cssCache; ///< All css files in the ebook are maintained here.

    bool fileIsOpen{ false };
    bool encryptionPresent{ false };
    bool fontsSizeTooLarge{ false };
    int32_t fontsSize{ 0 };
    bool pageLocsInstance{ false };

    auto getMeta(const std::string &name) -> const char *;
    auto getOpf(std::string &filename) -> bool;
    auto checkMimetype() -> bool;
    auto getOpfFilename(std::string &filename) -> bool;
    auto getEncryptionXml() -> bool;
    auto retrieveFontsFromCss(CSSPtr &css) -> void;
    auto sha1(const std::string &data) -> void;


    EPub() {
      fonts.setup();
    };

  public:
    ~EPub();

    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend HimemUniquePtr<T> makeUniqueHimem(Args &&... args);

    static inline auto Make() {
      return makeUniqueHimem<EPub>();
    }

    auto open(const HimemString &epubFilename) -> bool;
    auto closeFile() -> bool;
    auto getPicture(HimemString &fname, bool load) -> PicturePtr;
    auto retrieveFile(const char *fname, uint32_t &size) -> HimemUniquePtr<uint8_t[]>;
    auto getItem(pugi::xml_node itemref, ItemInfo &item) -> bool;
    auto getItemAtIndex(int16_t itemrefIndex) -> bool;
    auto getItemAtIndex(int16_t itemrefIndex, ItemInfo &item) -> bool;
    auto getUniqueIdentifier() -> std::string;
    auto getKeys() -> bool;
    auto filenameLocate(const char *fname) -> HimemString;
    auto getItemCount() -> int16_t;
    auto getFileObfuscation(const char *filename) -> ObfuscationType;
    auto getFonts() -> Fonts & { return fonts; }
    auto getCssPools() -> CSS::CSSPools & { return cssPools; }
    auto getDomPools() -> DOM::DOMPools & { return domPools; }
    auto loadFont(const HimemString &filename, const HimemString &fontFamily, const FaceStyle style)
    -> bool;

    auto retrieveCss(ItemInfo &item) -> void;
    auto loadFonts() -> void;
    auto clearItemData(ItemInfo &item) -> void;
    auto openParams(const HimemString &epubFilename) -> bool;
    auto retrieveBookFormatParams() -> void;
    auto decrypt(void *buffer, const uint32_t size, ObfuscationType obfType) -> void;

    [[nodiscard]] inline auto getLineHeightFactor() const -> float {
      int8_t lineHeight = bookFormatParams.lineHeight;
      if ((lineHeight < 0) || (lineHeight > 2)) { lineHeight = 1; } // default
      return lineHeightFactors[lineHeight];
    }

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

    [[nodiscard]] inline auto getCssCache() const -> const CSSList & { return cssCache; }
    inline auto clearCssCache() -> void { cssCache.clear(); }
    [[nodiscard]] inline auto getCurrentItemCss() const -> const CSSPtr & {
      return currentItemInfo.css;
    }
    [[nodiscard]] inline auto getCurrentItemInfo() const -> const ItemInfo & {
      return currentItemInfo;
    }
    [[nodiscard]] inline auto getCurrentItemFilePath() const -> const HimemString & {
      return currentItemInfo.filePath;
    }
    [[nodiscard]] inline auto getItemrefIndex() const -> int16_t {
      return currentItemInfo.itemrefIndex;
    }
    [[nodiscard]] inline auto getTitle() -> const char * { return getMeta("dc:title"); }
    [[nodiscard]] inline auto getAuthor() -> const char * { return getMeta("dc:creator"); }
    [[nodiscard]] inline auto getDescription() -> const char * { return getMeta("dc:description"); }
    [[nodiscard]] inline auto getCurrentItem() const -> const pugi::xml_document & {
      return currentItemInfo.xmlDoc;
    }
    [[nodiscard]] inline auto getCurrentFilename() const -> const HimemString & {
      return currentFilename;
    }
    [[nodiscard]] inline auto filenameIsEmpty() -> bool { return currentFilename.empty(); }
    [[nodiscard]] inline auto getBookParams() -> BookParams * { return bookParams.get(); }
    [[nodiscard]] inline auto getBookFormatParams() -> BookFormatParams * {
      return &bookFormatParams;
    }
    [[nodiscard]] inline auto getOpfBasePath() const -> const HimemString & { return opfBasePath; }
    [[nodiscard]] inline auto getOpf() -> const pugi::xml_document & { return opf; }
    [[nodiscard]] inline auto encryptionIsPresent() const -> bool { return encryptionPresent; }
    [[nodiscard]] inline auto getBinUuid() const -> const BinUUID & { return binUuid; }

    inline auto setPageLocsInstance(bool val) -> void { pageLocsInstance = val; }
};

#include "models/toc.hpp"