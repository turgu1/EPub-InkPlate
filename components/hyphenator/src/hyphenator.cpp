#include "hyphenator.hpp"

#include <frozen/unordered_map.h>
#include <frozen/string.h>

// #define TRACE(...) LOG_I(__VA_ARGS__)
#define TRACE(...)

enum class Language : uint8_t {
  EN, FR, HR, CS, DA, DE, ES, IS, IT, NL, PL, PT, SL, SK, SQ, TK,
  NONE
};

static frozen::unordered_map<frozen::string, Language, 16> languageMap = {
   { "en", Language::EN },
   { "fr", Language::FR },
   { "hr", Language::HR },
   { "cs", Language::CS },
   { "da", Language::DA },
   { "de", Language::DE },
   { "es", Language::ES },
   { "is", Language::IS },
   { "it", Language::IT },
   { "nl", Language::NL },
   { "pl", Language::PL },
   { "pt", Language::PT },
   { "sl", Language::SL },
   { "sk", Language::SK },
   { "sq", Language::SQ },
   { "tk", Language::TK },
};

Hyphenator::Hyphenator(const char *lang) {

  trieData = nullptr;
  trieSize = 0;

  if (strlen(lang) < 2) { return; }

  auto it = languageMap.find(frozen::string((const char *)(lang), 2));
  auto language = (it != languageMap.end()) ? it->second : Language::NONE;

  switch (language) {
    case Language::EN:  // English
      trieData = enStart;
      trieSize = enSize;
      break;
    case Language::FR: // French
      trieData = frStart;
      trieSize = frSize;
      break;
    case Language::HR: // Croatian
      trieData = hrStart;
      trieSize = hrSize;
      break;
#if EPUB_INKPLATE_BUILD
    case Language::CS: // Czech
      trieData = csStart;
      trieSize = csSize;
      break;
    case Language::DA: // Danish
      trieData = daStart;
      trieSize = daSize;
      break;
    case Language::DE: // German
      trieData = deStart;
      trieSize = deSize;
      break;
    case Language::ES: // Spanish
      trieData = esStart;
      trieSize = esSize;
      break;
    case Language::IS: // Icelandic
      trieData = isStart;
      trieSize = isSize;
      break;
    case Language::IT: // Italian
      trieData = itStart;
      trieSize = itSize;
      break;
    case Language::NL: // Dutch
      trieData = nlStart;
      trieSize = nlSize;
      break;
    case Language::PL: // Polish
      trieData = plStart;
      trieSize = plSize;
      break;
    case Language::PT: // Portuguese
      trieData = ptStart;
      trieSize = ptSize;
      break;
    case Language::SL: // Slovenian
      trieData = slStart;
      trieSize = slSize;
      break;
    case Language::SK: // Slovak
      trieData = skStart;
      trieSize = skSize;
      break;
    case Language::SQ: // Albanian
      trieData = sqStart;
      trieSize = sqSize;
      break;
    case Language::TK: // Turkish
      trieData = tkStart;
      trieSize = tkSize;
      break;
#endif
    default:
      return;
  }

  if (trieData != nullptr) {
    trieRootOffset = (trieData[0] << 24) | (trieData[1] << 16) | (trieData[2] << 8) | trieData[3];
    if (trieRootOffset > trieSize) {
      LOG_E("Bad Trie Format!");
      trieData = nullptr;
    // } else {
    //   LOG_I("Setting language to {} with size {} and root offset {}", lang, trieSize, trieRootOffset);
    }
  // } else {
  //   LOG_W("Language [{}] not found", lang);
  }
}

auto Hyphenator::decodeTrieNode(size_t addr) -> TrieNodePtr {
  TRACE("-> decodeTrieNode");
  if (addr >= trieSize) {
    TRACE("<- decodeTrieNode NULL 1");
    return nullptr;
  }
  const uint8_t* base = &trieData[addr];
  size_t         rem = trieSize - addr;
  size_t         pos = 0;

  const uint8_t  hdr = base[pos++];
  const bool     hasLevels = (hdr & 0x80u) != 0;
  uint8_t        stride = (hdr >> 5) & 0x03u;
  if (stride == 0) {
    stride = 1;
  }

  size_t childCount = hdr & 0x1Fu;
  if (childCount == 31u) {
    if (pos >= rem) {
      TRACE("<- decodeTrieNode NULL 2");
      return nullptr;
    }
    childCount = base[pos++];
  }

  const uint8_t* levels = nullptr;
  uint8_t        levelsLen = 0;
  if (hasLevels) {
    if (pos + 1 >= rem) {
      TRACE("<- decodeTrieNode NULL 3");
      return nullptr;
    }
    const uint8_t hi = base[pos++];
    const uint8_t loLen = base[pos++];
    const size_t  offset = (static_cast<size_t>(hi) << 4) | (loLen >> 4);
    levelsLen = loLen & 0x0Fu;
    if (offset < 4u || offset + levelsLen > trieSize) {
      TRACE("<- decodeTrieNode NULL");
      return nullptr;
    }
    levels = &trieData[offset];
  }

  if (pos + childCount > rem) {
    TRACE("<- decodeTrieNode NULL 4");
    return nullptr;
  }
  const uint8_t* transitions = &base[pos];
  pos += childCount;
  if (pos + static_cast<size_t>(childCount) * stride > rem) {
    TRACE("<- decodeTrieNode NULL 5");
    return nullptr;
  }
  const uint8_t* targets = &base[pos];

  TRACE("<- decodeTrieNode");
  return std::make_unique<TrieNode>(TrieNode{ .addr = addr,
                                              .stride = stride,
                                              .childCount = static_cast<uint8_t>(childCount < 255 ? childCount : 255),
                                              .transitions = transitions,
                                              .targets = targets,
                                              .levels = levels,
                                              .levelsLen = levelsLen });
}

auto Hyphenator::decodeDelta(const uint8_t* buf, uint8_t stride) -> int32_t {
  TRACE("-> decodeDelta");
  if (stride == 1) {
    TRACE("<- decodeDelta 1");
    return static_cast<int8_t>(buf[0]);
  }
  if (stride == 2) {
    TRACE("<- decodeDelta 2");
    return static_cast<int16_t>((static_cast<uint16_t>(buf[0]) << 8) | buf[1]);
  }
  const int32_t v = (static_cast<int32_t>(buf[0]) << 16) | (static_cast<int32_t>(buf[1]) << 8) | buf[2];
  TRACE("<- decodeDelta 3");
  return v - (1 << 23);
}

auto Hyphenator::trieStep(const TrieNodePtr node, uint8_t ch) -> TrieNodePtr {
  TRACE("-> trieStep");
  for (size_t i = 0; i < node->childCount; ++i) {
    if (node->transitions[i] != ch) {
      continue;
    }
    const uint8_t* dp = &node->targets[i * node->stride];
    const size_t   delta = decodeDelta(dp, node->stride);
    if ((trieSize - delta) > node->addr) {
      TRACE("<- trieStep 1");
      return decodeTrieNode(node->addr + delta);
    } else {
      TRACE("<- trieStep 2");
      return nullptr;
    }
  }
  TRACE("<- trieStep 3");
  return nullptr;
}

auto Hyphenator::findHyphenIndices(const std::string& word, uint8_t min, uint8_t max) -> OffsetsPtr {
  TRACE("-> findHyphenIndices for {}", word);
  if (word.empty() || (trieData == nullptr)) {
    TRACE("<- findHyphenIndices 1");
    return nullptr;
  }
  uint16_t wordLength = word.length();
  if (wordLength > MAX_WORD_LENGTH) {
    wordLength = MAX_WORD_LENGTH;
  }

  uint8_t aug[MAX_WORD_LENGTH + 3];
  aug[0] = '.';
  for (size_t i = 0; i < wordLength; ++i) {
    uint8_t c = static_cast<uint8_t>(word[i]);
    if (c >= 0x41u && c <= 0x5Au) {
      c += 0x20u;     // ASCII uppercase
    } else if (i > 0 && static_cast<uint8_t>(word[i - 1]) == 0xC3u) {
      // UTF-8 continuation byte after C3 prefix — lowercase if in uppercase range
      if (c >= 0x80u && c <= 0x9Eu && c != 0x97u) {
        c += 0x20u;
      }
    }
    aug[1 + i] = c;
  }
  const int32_t augLength = wordLength + 2;
  aug[augLength - 1] = '.';

  uint8_t scores[MAX_WORD_LENGTH + 3];
  std::memset(scores, 0, augLength);

  const TrieNodePtr root = decodeTrieNode(trieRootOffset);
  if (root == nullptr) {
    TRACE("<- findHyphenIndices 2");
    return nullptr;
  }

  for (int32_t start = 0; start < augLength; ++start) {
    TrieNodePtr node = std::make_unique<TrieNode>(*root);
    for (int32_t cursor = start; cursor < augLength; ++cursor) {
      if ((node = trieStep(std::move(node), aug[cursor])) == nullptr) {
        break;
      }

      if (node->levels && node->levelsLen > 0) {
        size_t offset = 0;
        for (uint8_t li = 0; li < node->levelsLen; ++li) {
          const uint8_t packed = node->levels[li];
          offset += packed / 10;
          const uint8_t level = packed % 10;
          const int32_t splitPos = start + offset;
          if (splitPos < augLength && level > scores[splitPos]) {
            scores[splitPos] = level;
          }
        }
      }
    }
  }

  OffsetsPtr offsets = std::make_unique<Offsets>();

  for (uint8_t k = 1; k <= wordLength; ++k) {
    if ((scores[k + 1] & 1) && k >= min && k <= max) {
      offsets->push_back(k);
    }
  }

  TRACE("<- findHyphenIndices 3 {}", offsets->size());
  return offsets->size() > 0 ? std::move(offsets) : nullptr;
}