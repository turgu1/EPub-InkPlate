#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <vector>
#include <string_view>
#include <algorithm>
#include <cstring>
#include <cstdint>

#if EPUB_INKPLATE_BUILD
  #define STR_HELPER(x) #x
  #define STR(x) STR_HELPER(x)

  #define LANGUAGE(lang) \
          extern const uint8_t lang ## Start[] asm ("_binary_" STR(lang) "_bin_start"); \
          extern const uint8_t lang ## End[] asm ("_binary_" STR(lang) "_bin_end"); \
          const size_t lang ##         Size = ( lang ## End - lang ## Start);

  LANGUAGE(cs)
  LANGUAGE(da)
  LANGUAGE(de)
  LANGUAGE(en)
  LANGUAGE(es)
  LANGUAGE(fr)
  LANGUAGE(hr)
  LANGUAGE(is)
  LANGUAGE(it)
  LANGUAGE(nl)
  LANGUAGE(pl)
  LANGUAGE(pt)
  LANGUAGE(sl)
  LANGUAGE(sk)
  LANGUAGE(tk)


#else
  const uint8_t enStart[] {
    #embed "../embed/en.bin"
  };
  const size_t  enSize = sizeof(enStart);

  const uint8_t frStart[] {
    #embed "../embed/fr.bin"
  };
  const size_t frSize = sizeof(frStart);

  const uint8_t hrStart[] {
    #embed "../embed/hr.bin"
  };
  const size_t hrSize = sizeof(hrStart);
#endif

using HyphenatorPtr = HimemUniquePtr<class Hyphenator>;

class Hyphenator {
  private:
    static constexpr char const *TAG = "Hyphenator";
    const uint16_t MAX_WORD_LENGTH = 128;

    const uint8_t* trieData;
    size_t trieSize;
    size_t trieRootOffset;
    
    struct TrieNode {
      size_t addr;
      uint8_t stride;
      uint8_t childCount;
      const uint8_t* transitions;
      const uint8_t* targets;
      const uint8_t* levels;
      uint8_t levelsLen;
    };
    using TrieNodePtr = std::unique_ptr<TrieNode>;

    auto decodeTrieNode(size_t addr) -> TrieNodePtr;
    auto decodeDelta(const uint8_t* buf, uint8_t stride) -> int32_t;
    auto trieStep(const TrieNodePtr node, uint8_t ch) -> TrieNodePtr;
    
    Hyphenator(const char *lang);

  public:
    template <typename T, typename ... Args>
    requires(!std::is_array_v<T>)
    friend auto makeUniqueHimem(Args &&... args)->HimemUniquePtr<T>;

    static inline auto Make(const char * language) { return makeUniqueHimem<Hyphenator>(language); }

    using Offsets = std::vector<uint8_t>;
    using OffsetsPtr = std::unique_ptr<Offsets>;
    
    auto inline getTrieData() -> const uint8_t * const { return trieData; }

    auto findHyphenIndices(const std::string& word, uint8_t min, uint8_t max) -> OffsetsPtr;
};

#if EPUB_INKPLATE_BUILD
  #undef STR_HELPER
  #undef STR
  #undef LANGUAGE
#endif