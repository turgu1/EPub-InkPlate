// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// #undef DEBUGGING
// #define DEBUGGING 1

#include "models/toc.hpp"
#include "models/page_locs.hpp"

// Defined in epub.cpp

extern auto packagePred(xml_node node) -> bool;
extern auto metadataPred(xml_node node) -> bool;
extern auto manifestPred(xml_node node) -> bool;
auto itemPred(xml_node node) -> bool;
auto spinePred(xml_node node) -> bool;
auto itemrefPred(xml_node node) -> bool;
auto xmlnsPred(xml_attribute attr) -> bool;

extern auto oneByAttr(xml_node n, const char *name1, const char *name2, const char *attr,
                      const char *value) -> xml_node;

auto TOC::buildFilename(EPubPtr &epub) -> HimemString {
  HimemString epubFilename = epub->getCurrentFilename();
  return epubFilename.substr(0, epubFilename.find_last_of('.')) + ".toc";
}

auto TOC::load(EPubPtr &epub) -> bool {
  LOG_D("load()");
  HimemString filename = buildFilename(epub);
  clean();

  LOG_D("Reading toc: %s.", filename.c_str());

  if (!db->open(filename)) {
    LOG_E("Can't open toc: %s", filename.c_str());
    return false;
  }

  // We first verify if the database content is of the current version

  bool versionOk = false;
  VersionRecord versionRecord;

  if (db->getRecordCount() > 0) {
    db->gotoFirst();
    if (db->getRecordSize() == sizeof(versionRecord)) {
      db->getRecord(&versionRecord, sizeof(versionRecord));
      if ((versionRecord.version == TOC_DB_VERSION) &&
          (strcmp(versionRecord.appName, TOC_NAME) == 0)) {
        versionOk = true;
      }
    }
  }

  if (!versionOk) {
    LOG_E("Toc is of a wrong version or is empty");
    db->close();
    return false;
  }

  if (db->gotoNext()) {
    charBufferSize = db->getRecordSize();
    if (charBufferSize > 0) {
      charBuffer = makeUniqueHimem<char[]>(charBufferSize);
      if (db->getRecord(charBuffer.get(), charBufferSize)) {
        uint16_t count = db->getRecordCount() - 2;
        if (count > 0) {
          entries.resize(count);
          uint16_t idx = 0;
          while ((idx < count) && db->gotoNext()) {
            if (db->getRecordSize() == sizeof(EntryRecord)) {
              if (!db->getRecord(&entries[idx], sizeof(EntryRecord))) break;
              entries[idx].label = charBuffer.get() + (size_t)entries[idx].label;
              idx++;
            } else {
              LOG_E("DB corrupted.");
              break;
            }
          }
          if (idx != count) {
            LOG_E("The toc has been partially read: %d records.", idx);
          } else {
            ready = compacted = saved = true;
          }
        } else {
          LOG_E("DB empty.");
        }
      } else {
        LOG_E("Unable to get char buffer content.");
      }
    } else {
      LOG_D("Toc db seems to be empty.");
      ready = compacted = saved = true;
    }
  } else {
    LOG_D("Toc db is empty.");
    ready = compacted = saved = true;
  }

  db->close();

  #if DEBUGGING
    show();
  #endif

  if (ready) LOG_I("Reading toc completed. Entry count: %d.", entries.size());
  #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
    ESP::show_heaps_info();
  #endif
  return ready;
}

auto TOC::save(HimemString epubFilename) -> bool {
  LOG_D("save()");
  if (saved) return true;

  HimemString filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".toc";

  if (!compact()) return false;

  if (db->create(filename)) {
    VersionRecord versionRecord;
    strcpy(versionRecord.appName, TOC_NAME);
    versionRecord.version = TOC_DB_VERSION;

    if (db->addRecord(&versionRecord, sizeof(VersionRecord))) {
      if (db->addRecord(charBuffer.get(), charBufferSize)) {
        uint16_t idx;
        for (idx = 0; idx < entries.size(); idx++) {
          EntryRecord e = entries[idx];
          e.label       = e.label - (size_t)charBuffer.get();
          if (!db->addRecord(&e, sizeof(EntryRecord))) {
            LOG_E("Unable to add entry record.");
            break;
          }
        }
        ready = saved = (idx == entries.size());
      } else {
        LOG_E("Unable to save charBuffer.");
      }
    } else {
      LOG_E("Unable to save version record.");
    }
  } else {
    LOG_E("Unable to create toc db.");
  }

  db->close();

  #if DEBUGGING
    show();
  #endif

  return saved;
}

auto hexToBin(char ch) -> unsigned char;

auto TOC::cleanFilename(char *fname) -> void {
  LOG_D("cleanFilename()");

  char *s = fname;
  char *d = fname;

  while (*s) {
    if (*s == '%') {
      *d++ = (hexToBin(s[1]) << 4) + hexToBin(s[2]);
      s += 3;
    } else {
      *d++ = *s++;
    }
  }

  *d = 0;
}

/// @brief Recursively processes navigation points in an NCX (Navigation Control eXtensible)
/// document.
///
/// Parses each navPoint element to extract the label and file reference, resolves the file
/// to its corresponding spine itemref index, and builds a table of contents structure.
/// Handles fragment identifiers (anchors) by storing them in an infos map for later lookup.
///
/// @param node Reference to the current XML navPoint node being processed.
/// @param level The hierarchical depth level of this navPoint (0 for root level).
///
/// @return true if all navPoint elements were successfully processed and resolved;
///         false if any inconsistency is found (missing manifest item, missing spine reference,
///         malformed navPoint structure, or unable to resolve spine itemref).
///
/// @details
/// - Extracts navLabel/text content as the TOC entry label.
/// - Parses content/@src attribute to get the file reference and optional fragment ID.
/// - Allocates memory from charPool for label and filename strings.
/// - Resolves the filename to a manifest item ID via OPF document.
/// - Maps the item ID to its index in the spine's itemref list.
/// - If a fragment ID exists, stores the mapping (itemrefIndex, fragment_id) -> entry_index.
/// - Recursively processes nested navPoint children with incremented level.
///
/// @note Assumes opf, charPool, and related class members are properly initialized.
///       Returns false with error logging on any critical data structure mismatch.
auto TOC::doNavPoints(pugi::xml_node &node, uint8_t level) -> bool {
  LOG_D("doNavPoints()");

  xml_attribute attr;

  do {
    const char *label = node.child("navLabel").child("text").text().as_string();
    const char *fname = node.child("content").attribute("src").value();

    xml_node n;

    EntryRecord entry;
    std::string theId;
    char *filenameToFind;

    entry.label = charPool->allocate(strlen(label) + 1);
    if (entry.label == nullptr) {
      LOG_E("Unable to allocate memory for TOC label.");
      return false;
    }
    strcpy(entry.label, label);
    entry.pageId = PageId(0, -1);
    entry.level  = level;

    const char *hashPos = strchr(fname, '#');
    if (hashPos != nullptr) {
      theId.assign(hashPos + 1);
      filenameToFind = charPool->allocate(hashPos - fname + 1);
      if (filenameToFind == nullptr) {
        LOG_E("Unable to allocate memory for TOC filename.");
        return false;
      }
      strlcpy(filenameToFind, fname, hashPos - fname + 1);
    } else {
      theId.clear();
      filenameToFind = charPool->allocate(strlen(fname) + 1);
      if (filenameToFind == nullptr) {
        LOG_E("Unable to allocate memory for TOC filename.");
        return false;
      }
      strcpy(filenameToFind, fname);
    }

    cleanFilename(filenameToFind);

    if ((n = opf->find_child(packagePred).find_child(manifestPred)) &&
        (n = oneByAttr(n, "item", "opf:item", "href", filenameToFind))) {
      // Is the node with the same filename is found, we then search the
      // itemref in the spine with the same id associated with the item.
      if ((attr = n.attribute("id"))) {
        const char *idref = attr.value();

        if ((n = opf->find_child(packagePred).find_child(spinePred))) {

          int16_t index = 0;
          bool found    = false;

          for (auto nn : n.children()) {
            if (strcmp(nn.attribute("idref").value(), idref) == 0) {
              found = true;
              break;
            }
            index++;
          }

          if (found) {
            entry.pageId.itemrefIndex = index;
            if (!theId.empty()) {
              infos.insert(std::make_pair(std::make_pair(index, theId),
                                          static_cast<int16_t>(entries.size())));
              someIds = true;
            } else {
              entry.pageId.offset = 0;
            }
            entries.push_back(entry);
          } else {
            LOG_E("Unable to find reference %s in spine", idref);
            return false;
          }
        } else {
          LOG_E("No spine in OPF!!");
          return false;
        }
      } else {
        LOG_E("NCX inconsistency.");
        return false;
      }
    } else {
      LOG_E("OPF inconsistency.");
      return false;
    }

    if ((n = node.child("navPoint"))) {
      if (!doNavPoints(n, level + 1)) return false;
    }

    node = node.next_sibling();
  } while (node);

  return true;
}

auto TOC::loadFromEpub(EPub &epub) -> bool {
  LOG_D("loadFromEpub()");

  xml_node node, node2;
  xml_attribute attr;
  const char *filename = nullptr;

  opf = &epub.getOpf();

  clean();

  // Retrieve the ncx filename
  // Sometimes, the id is "ncx", sometimes "toc"

  if (((node = opf->find_child(packagePred).find_child(manifestPred)) &&
       ((node2 = oneByAttr(node, "item", "opf:item", "id", "ncx")) ||
        (node2 = oneByAttr(node, "item", "opf:item", "id", "toc")))) &&
      (strcmp(node2.attribute("media-type").value(), "application/x-dtbncx+xml") == 0)) {
    filename = node2.attribute("href").value();
  }

  // If filename was not found, returns gracefully. This is usually related
  // to a version 3 epub format that doesn't supply a V2 table of content.

  if (filename == nullptr) return true;

  // retrieve the ncx file data

  uint32_t ncxSize;
  bool result = false;

  auto ncxData = epub.retrieveFile(filename, ncxSize);
  if (ncxData == nullptr) return false;

  auto ncxOpf = std::make_unique<pugi::xml_document>();
  if (ncxOpf == nullptr) return false;

  // parse xml and load navPoint entries

  xml_parse_result res = ncxOpf->load_buffer_inplace(ncxData.get(), ncxSize);
  if (res.status != status_ok) {
    LOG_E("xml load error: %d", res.status);
  } else {
    if ((node = ncxOpf->child("ncx").child("navMap").child("navPoint"))) {
      if (!charPool) charPool = CharPool::Make();
      if ((charPool != nullptr) && doNavPoints(node, 0)) {
        result = !entries.empty();
      } else {
        LOG_E("Unable to load nav points.");
      }
    }
  }

  #if DEBUGGING
    show();
    showInfo();
  #endif

  return result;
}

auto TOC::compact() -> bool {
  LOG_D("compact()");

  if (compacted) return true;

  charBuffer.reset();
  charBufferSize = 0;

  if (!entries.empty()) {
    for (auto &e : entries) {
      const size_t labelLen = strlen(e.label) + 1;
      if (labelLen > (std::numeric_limits<size_t>::max() - charBufferSize)) {
        LOG_E("TOC compact size overflow.");
        return false;
      }
      charBufferSize += labelLen;
    }

    if (charBufferSize > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
      LOG_E("TOC compact size too large: %u", static_cast<unsigned>(charBufferSize));
      return false;
    }

    charBuffer = makeUniqueHimem<char[]>(charBufferSize);
    if (!charBuffer) return false;

    char *buff = charBuffer.get();
    for (auto &e : entries) {
      strcpy(buff, e.label);
      e.label = buff;
      buff += strlen(buff) + 1;
    }
  }

  charPool.reset();
  infos.clear();

  compacted = true;
  return true;
}

auto TOC::clean() -> void {
  LOG_D("clean()");

  charPool.reset();
  charBuffer.reset();

  infos.clear();
  entries.clear();

  ready     = false;
  compacted = false;
  saved     = false;
  someIds   = false;
}

auto TOC::set(std::string &id, int32_t currentOffset) -> void {
  auto itemrefIndex = pageLocs.getCurrentItemrefIndex();
  if (itemrefIndex < 0) return;

  Infos::iterator infosIt = infos.find(std::make_pair(itemrefIndex, id));

  if (infosIt != infos.end()) {
    entries[infosIt->second].pageId.offset = currentOffset;
  }
}

auto TOC::set(int32_t currentOffset) -> void {
  auto itemrefIndex = pageLocs.getCurrentItemrefIndex();
  if (itemrefIndex < 0) return;
  int16_t idx = -1;

  for (auto &e : entries) {
    idx++;
    if (e.pageId.itemrefIndex == itemrefIndex) {
      break;
    }
  }
  if ((idx >= 0) && (idx < (int16_t)entries.size())) {
    entries[idx].pageId.offset = currentOffset;
  }
}

auto TOC::exists(const HimemString &epubFilename) -> bool {
  HimemString filename = epubFilename.substr(0, epubFilename.find_last_of('.')) + ".toc";
  auto db              = SimpleDB::Make();
  if (!db->open(filename)) return false;

  VersionRecord versionRecord;
  bool ok = false;

  if ((db->getRecordCount() > 0) && db->gotoFirst() &&
      (db->getRecordSize() == sizeof(versionRecord)) &&
      db->getRecord(&versionRecord, sizeof(versionRecord)) &&
      (versionRecord.version == TOC_DB_VERSION) && (strcmp(versionRecord.appName, TOC_NAME) == 0)) {
    ok = true;
  }

  db->close();
  return ok;
}

#if DEBUGGING

  auto TOC::show() -> void {
    std::cout << "----- Table of Content: -----" << std::endl;

    for (auto &e : entries) {
      std::cout << e.label << " : [" << e.pageId.itemrefIndex << ", " << e.pageId.offset << "]"
                << std::endl;
    }

    std::cout << "----- End TOC -----" << std::endl;
  }

  auto TOC::showInfo() -> void {
    std::cout << "----- TOC Infos -----" << std::endl;

    for (auto &e : infos) {
      std::cout << "Id: " << e.first.second << ", "
                << "Item index: " << e.first.first << ", "
                << "TOC Entry index: " << e.second << std::endl;
    }

    std::cout << "----- End TOC Infos -----" << std::endl;
  }

#endif