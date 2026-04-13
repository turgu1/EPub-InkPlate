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

using EPubPtr = himem_unique_ptr<class EPub>;
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
    std::string file_path{};
    int16_t itemref_index{};
    pugi::xml_document xml_doc{};
    CSSList css_cache; ///< style attributes part of the current processed item are kept here. They
                       ///< will be destroyed when the item is no longer required.
    CSSRefList css_list; ///< List of css sources for the current item file shown. Those are indexes
                         ///< inside css_cache.
    CSSPtr css{}; ///< Ghost CSS created through merging css suites from css_list and css_cache.
    FileContentPtr data{};
    MediaType media_type{};

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
    int8_t show_title;
    int8_t show_pictures;
    int8_t font_size;
    int8_t use_fonts_in_book;
    int8_t font;
  };
  #pragma pack(pop)

  using BinUUID = uint8_t[16];
  using ShaUUID = uint8_t[20];

  himem_unique_ptr<TOC> toc;

private:
  static constexpr char const *TAG = "EPub";

  std::recursive_timed_mutex mutex;

  pugi::xml_document opf; ///< The OPF document description.
  pugi::xml_document encryption;
  pugi::xml_node current_itemref{pugi::xml_node(nullptr)};

  BinUUID bin_uuid{};
  ShaUUID sha_uuid{};

  FileContentPtr opf_data{nullptr};
  FileContentPtr encryption_data{nullptr};
  std::string current_filename{""};
  std::string opf_base_path{""};

  ItemInfo current_item_info{};
  BookParams *book_params{nullptr};
  BookFormatParams book_format_params{};

  CSSList css_cache; ///< All css files in the ebook are maintained here.

  bool file_is_open{false};
  bool encryption_present{false};
  bool fonts_size_too_large{false};
  int32_t fonts_size{0};
  bool page_locs_instance{false};

  auto get_meta(const std::string &name) -> const char *;
  auto get_opf(std::string &filename) -> bool;
  auto check_mimetype() -> bool;
  auto get_opf_filename(std::string &filename) -> bool;
  void retrieve_fonts_from_css(CSSPtr &css);
  auto get_encryption_xml() -> bool;
  void sha1(const std::string &data);

  EPub() = default;

public:
  ~EPub();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<EPub>(); }

  void retrieve_css(ItemInfo &item);
  void load_fonts();
  void clear_item_data(ItemInfo &item);
  void open_params(const std::string &epub_filename);
  auto open(const std::string &epub_filename) -> bool;
  auto close_file() -> bool;
  auto get_picture(std::string &fname, bool load) -> PicturePtr;
  auto retrieve_file(const char *fname, uint32_t &size) -> himem_unique_ptr<uint8_t[]>;
  auto get_item(pugi::xml_node itemref, ItemInfo &item) -> bool;
  auto get_item_at_index(int16_t itemref_index) -> bool;
  auto get_item_at_index(int16_t itemref_index, ItemInfo &item) -> bool;
  auto get_unique_identifier() -> std::string;
  auto get_keys() -> bool;
  auto filename_locate(const char *fname) -> std::string;
  auto get_item_count() -> int16_t;
  void update_book_format_params();
  auto get_file_obfuscation(const char *filename) -> ObfuscationType;
  void decrypt(void *buffer, const uint32_t size, ObfuscationType obf_type);
  auto load_font(const std::string filename, const std::string font_family,
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
  auto get_cover_filename() -> const char *;

  // clang-format off
  inline auto get_css_cache() const              -> const CSSList &       { return css_cache; }
  inline auto get_current_item_css() const       -> const CSSPtr &        { return current_item_info.css; }
  inline auto get_current_item_info() const      -> const ItemInfo &      { return current_item_info; }
  inline auto get_current_item_file_path() const -> const std::string &   { return current_item_info.file_path; }
  inline auto get_itemref_index() const     -> int16_t                    { return current_item_info.itemref_index; }
  inline auto get_title()                   -> const char *               { return get_meta("dc:title"); }
  inline auto get_author()                  -> const char *               { return get_meta("dc:creator"); }
  inline auto get_description()             -> const char *               { return get_meta("dc:description"); }
  inline auto get_current_item() const      -> const pugi::xml_document & { return current_item_info.xml_doc; }
  inline auto get_current_filename()        -> std::string                { return current_filename; }
  inline auto filename_is_empty()           -> bool                       { return current_filename.empty(); }
  inline auto get_book_params()             -> BookParams *               { return book_params; }
  inline auto get_book_format_params()      -> BookFormatParams *         { return &book_format_params; }
  inline auto get_opf_base_path() const     -> const std::string &        { return opf_base_path; }
  inline auto get_opf()                     -> const pugi::xml_document & { return opf; }
  inline auto encryption_is_present() const -> bool                       { return encryption_present; }
  inline auto get_bin_uuid() const          -> const BinUUID &            { return bin_uuid; }
  
  inline void set_page_locs_instance(bool val)                            { page_locs_instance = val; }
  // clang-format on
};

#include "models/toc.hpp"