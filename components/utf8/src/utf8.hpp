#pragma once

#include "global.hpp"

#include <vector>
#include <tuple>
#include <utility>
#include <cstdint>
#include <string>

class UTF8 {
  public:
    static auto toUnicode(const char *str, TextTransform transform, bool first)
           -> std::pair<char32_t, const char *>;
    static auto toUTF8(char32_t codePoint, std::string &concatTo) -> int;
    static auto extractWord(const char *word, int16_t maxIdx)
        -> std::tuple<std::string, std::vector<uint16_t>, int16_t>;
};