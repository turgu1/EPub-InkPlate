// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

// Table of content class

#include "global.hpp"
#include "himem.hpp"

#include "alloc.hpp"
#include "char_pool.hpp"
#include "logging.hpp"
#include "pugixml.hpp"
#include "simple_db.hpp"

#include "models/epub.hpp"

#include <forward_list>
#include <map>
#include <utility>

using TOCPtr = himemUniquePtr<class TOC>;
class TOC {
private:
  TOC() : db(SimpleDB::Make()) {}

public:
  ~TOC() = default;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make() { return makeUniqueHimem<TOC>(); }

  #pragma pack(push, 1)
  struct EntryRecord {
    char *label;
    PageId pageId;
    uint8_t level;
    EntryRecord() {
      label = nullptr;
      level = 0;
    }
  };

  struct VersionRecord {
    uint16_t version;
    char appName[32];
    VersionRecord() {
      version = 0;
      memset(appName, 0, 32);
    }
  };
  #pragma pack(pop)

  using Infos   = std::map<std::pair<int16_t, std::string>, uint16_t>;
  using Entries = std::vector<EntryRecord>;

  const Entries &getEntries() { return entries; }
 [[nodiscard]] inline auto isReady() -> bool { return ready; }
 [[nodiscard]] inline auto isEmpty() -> bool { return entries.empty(); }
 [[nodiscard]] inline auto thereIsSomeIds() -> bool { return someIds; }
 [[nodiscard]] inline auto getEntryCount() -> int16_t { return entries.size(); }
 [[nodiscard]] inline auto getEntry(int16_t idx) -> const EntryRecord & { return entries[idx]; }

  /**
   * @brief Load table of content from a file.
   *
   * This is used to load the table of content related to the current
   * ePub book. The file is a complete table with labels and page_ids. The filename is
   * the same as for the e-book, with extension .toc
   *
   * @return True if loading the file was successfull
   */
  auto load(EPubPtr &epub) -> bool;

  /**
   * @brief Save constructed table of content to a file.
   *
   * This method will save the table of content once it has been
   * constructed by the PageLocs class. The filename is
   * the same as for the e-book, with extension .toc
   *
   * @return True if saving was successfull.
   */
  auto save(EPubPtr &epub) -> bool;

  /**
   * @brief Load table of content from ePub file.
   *
   * This is used to prepare the construction of a table of
   * content. This method will load the  *.ncx* file, retrieving
   * ids, filenames and labels for each entry in the table of content.
   * The PageLocs class will then interact with the TOC class to complete
   * the information with the PageId related to each entry,
   *
   * @return True if the information has been retrieved successfully.
   */
  auto loadFromEpub(EPubPtr &epub) -> bool;

  /**
   * @brief Compact the char strings to a single buffer.
   *
   * This is used to retrieve all char strings in a single buffer
   * in preparation to be saved to a file.
   *
   * @return True if the compaction was successfull.
   */
  auto compact() -> bool;

  /**
   * @brief set table of content entry with page id
   *
   * If the id correspond to an entry of the table
   * of content, its location will be set with the
   * pageId received.
   *
   * @param id HTML id attribute that is part of an item.
   * @param currentOffset The location offset of the id in the item
   */
  auto set(std::string &id, int32_t currentOffset) -> void;
  auto set(int32_t currentOffset) -> void;

private:
  static constexpr char const *TAG      = "TOC";
  static constexpr char const *TOC_NAME = "EPUB_INKPLATE_TOC";
  static const uint16_t TOC_DB_VERSION  = 1;

  Entries entries;
  Infos infos;

  SimpleDBPtr db; ///< The SimpleDB table

  CharPoolPtr charPool{nullptr};
  himemUniquePtr<char[]> charBuffer{nullptr};
  uint16_t charBufferSize{0};

  const pugi::xml_document *opf{nullptr};

  volatile bool ready{false}; // true if the table of content has been populated
  bool compacted{false};
  bool saved{false};
  bool someIds{false};

  auto clean() -> void;
  auto cleanFilename(char *fname) -> void;
  auto buildFilename(EPubPtr &epub) -> std::string;
  auto doNavPoints(pugi::xml_node &node, uint8_t level) -> bool;

  #if DEBUGGING
    auto show() -> void;
    auto showInfo() -> void;
  #endif
};
