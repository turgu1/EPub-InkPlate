// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// Hyphenator test suite
//
// Full implementation lives here (Linux test build).  The ESP32 build uses
// the copy in components/hyphenator/src/hyphenator.cpp which is kept in sync.
// ---------------------------------------------------------------------------

#include "hyphenator.hpp"
#include "utf8.hpp"
#include "test_stats.hpp"
#include "himem.hpp"

#include <cstdio>
#include <vector>
#include <iostream>

#define HT_LOG(fmt, ...) std::printf("[hyphenator_test] " fmt "\n", ##__VA_ARGS__)

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

static int s_pass = 0;
static int s_fail = 0;

#define HT_CHECK(cond, msg, ...)                                                                        \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      HT_LOG("FAIL [%s:%d] %s",  __FILE__, __LINE__, std::format(msg, ##__VA_ARGS__).c_str());                                             \
      ++s_fail;                                                                                    \
    } else {                                                                                       \
      HT_LOG("PASS %s", std::format(msg, ##__VA_ARGS__).c_str());                                                                         \
      ++s_pass;                                                                                    \
    }                                                                                              \
  } while (0)

// ===========================================================================
// Hyphenator::Make() — instantiation and basic structure
// ===========================================================================
static void testHyphenatorMake() {
  HT_LOG("--- Hyphenator::Make() ---");

  auto hyphenator = Hyphenator::Make("en");
  HT_CHECK(hyphenator != nullptr, "Hyphenator::Make(\"en\") returns a non-null pointer");
  HT_CHECK(hyphenator->getTrieData() != nullptr, "hyphenator->getTrieData() returns a non-null pointer");

  hyphenator = Hyphenator::Make("fr");
  HT_CHECK(hyphenator != nullptr, "Hyphenator::Make(\"fr\") returns a non-null pointer");
  HT_CHECK(hyphenator->getTrieData() != nullptr, "hyphenator->getTrieData() returns a non-null pointer");

  hyphenator = Hyphenator::Make("aa");
  HT_CHECK(hyphenator != nullptr, "Hyphenator::Make(\"aa\") returns a non-null pointer");
  HT_CHECK(hyphenator->getTrieData() == nullptr, "hyphenator->getTrieData() returns a null pointer");
}

using Res = std::vector<uint8_t>;

// ===========================================================================
// testExtractWord
// ===========================================================================

static void testExtractWord(const char *word, int16_t idx, 
                            std::string expectedWord, Res expectedPositions, int16_t expectedIdx) {

  std::cout << "[hyphenator_test] testExtractWord(): <" << word << "> [" << idx << "]" << std::endl;

  auto [w, v, k] = UTF8::extractWord(word, idx);

  HT_CHECK(w == expectedWord, "testExtractWord() Word Extracted is conform to the expectations");

  std::cout << "Positions: ";
  bool first = true;
  for (auto val: v) { std::cout << (first ? "[" : ", ") << (int)val; first = false; }
  std::cout << "]" << std::endl;

  std::cout << "result Idx: " << (int)k << std::endl;

  HT_CHECK(v.size() == expectedPositions.size(), "Positions is of the right size");

  if (v.size() == expectedPositions.size()) {
    bool resultOk = true;
    for (size_t i = 0; i < v.size(); i++) {
      if (v[i] != expectedPositions[i]) {
        resultOk = false;
        break;
      }
    }

    HT_CHECK(resultOk, "testExtractWord() positions are the same as expectedResult.");
  }

  HT_CHECK(k == expectedIdx, "testExtractWord() idx is as expected");
}

// ===========================================================================
// testOneWord
// ===========================================================================
using Res = std::vector<uint8_t>;

static void testOneWord(HyphenatorPtr &hyphenator, const char *word, Res expectedResult) {

  std::cout << "[hyphenator_test] testOneWord(): <" << word << ">" << std::endl;

  HT_CHECK(hyphenator != nullptr, "Hyphenator instance is not a null pointer");

  HT_CHECK(hyphenator->getTrieData() != nullptr, "hyphenator->getTrieData() returns a non-null pointer");

  auto result = hyphenator->findHyphenIndices(word, 0, strlen(word) - 1);
  
  HT_CHECK(result != nullptr, "hyphenator->findHyphenIndices() result is a non-null pointer");

  if (result != nullptr) {
    HT_CHECK(result->size() == expectedResult.size(), "hyphenator->findHyphenIndices() is of expected size");

    std::cout << "Result: ";
    bool first = true;
    for (auto val: *result) { std::cout << (first ? "[" : ", ") << (int)val; first = false; }
    std::cout << "]" << std::endl;

    if (result->size() == expectedResult.size()) {
      bool resultOk = true;
      for (size_t i = 0; i < result->size(); i++) {
        if (result->at(i) != expectedResult[i]) {
          resultOk = false;
          break;
        }
      }

      HT_CHECK(resultOk, "testOneWord() result is the same as expectedResult.");
    }
  }
}

// ===========================================================================
// testHyphenatorFindHyphenIndices
// ===========================================================================
static void testHyphenatorFindHyphenIndices() {
  HT_LOG("--- testHyphenatorFindHyphenIndices ---");

  auto hyphenator = Hyphenator::Make("en");
  testOneWord(hyphenator, "similarity", Res({3, 4, 7, 8}));
  testOneWord(hyphenator, "space-efficient", Res({6, 8, 10, 14}));
  testOneWord(hyphenator, "pneumonoultramicroscopicsilicovolcanoconiosis", Res({1, 4, 6, 10, 13, 15, 18, 22, 24, 27, 28, 30, 33, 39, 42}));

  hyphenator = Hyphenator::Make("fr");
  testOneWord(hyphenator, "continuent", Res({5}));
  testOneWord(hyphenator, "similarité", Res({2, 4, 6, 8}));
  testOneWord(hyphenator, "Bonjour", Res({3}));
  testOneWord(hyphenator, "l'envie", Res({4}));
  testOneWord(hyphenator, "l’envie", Res({6}));
  testOneWord(hyphenator, "élever", Res({4}));
  testOneWord(hyphenator, "l'immitation", Res({4, 6, 8}));
  testOneWord(hyphenator, "récréation", Res({3, 8}));
  testOneWord(hyphenator, "l'initiation", Res({5, 8}));
  testOneWord(hyphenator, "Anticonstitutionnellement", Res({2, 4, 8, 10, 12, 16, 19, 21}));
  testOneWord(hyphenator, "Aminométhylpyrimidinylhydroxyéthylméthythiazolium", Res({3, 5, 8, 12, 14, 16, 18, 20, 23, 25, 32, 36, 39, 42, 46, 48}));
}

// ===========================================================================
// testHyphenatorExtractWord
// ===========================================================================
static void testHyphenatorExtractWord() {
  HT_LOG("--- testHyphenatorExtractWord ---");

  testExtractWord("similarity",    0, "similarity", Res({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}), 0);
  testExtractWord("“similarity”",  3, "similarity", Res({3, 4, 5, 6, 7, 8, 9, 10, 11, 12}), 0);
  testExtractWord("“similarity”",  0, "similarity", Res({3, 4, 5, 6, 7, 8, 9, 10, 11, 12}), -1);
  testExtractWord("“similarity”",  7, "similarity", Res({3, 4, 5, 6, 7, 8, 9, 10, 11, 12}), 4);
  testExtractWord("récréation!;",  2, "récréation", Res({0, 1, 1, 3, 4, 5, 5, 7, 8, 9, 10, 11}), 1);
  testExtractWord("‘récréation’,", 4, "récréation", Res({3, 4, 4, 6, 7, 8, 8, 10, 11, 12, 13, 14}), 1);
  testExtractWord("‘récréation’",  0, "récréation", Res({3, 4, 4, 6, 7, 8, 8, 10, 11, 12, 13, 14}), -1);
  testExtractWord("‘récréation’", 15, "récréation", Res({3, 4, 4, 6, 7, 8, 8, 10, 11, 12, 13, 14}), -1);
  testExtractWord("‘ré&shy;cr&eacute;&shy;ation’", 14, "récréation", Res({3, 4, 4, 11, 12, 13, 13, 26, 27, 28, 29, 30}), 5);
}

} // namespace

// ===========================================================================
// Public entry point
// ===========================================================================
auto testHyphenator() -> TestStats {
  s_pass = 0;
  s_fail = 0;

  testHyphenatorMake();
  testHyphenatorExtractWord();
  testHyphenatorFindHyphenIndices();

  HT_LOG("--- Hyphenator tests complete: %d passed, %d failed ---", s_pass, s_fail);
  return TestStats{s_pass, s_fail};
}
