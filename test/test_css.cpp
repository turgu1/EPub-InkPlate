// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_css.cpp — CSS parser unit tests
//
// Exercises CSSParser by parsing test/fixtures/test.css and verifying:
//   * rules are created for every selector kind
//   * property values have the expected numeric / string / enum results
//   * specificity ordering (id > class > element)
//   * @font-face generates a rule
//   * silently-skipped at-rules (@import, @namespace, @page, @media) do not
//     crash the parser
//   * !important is consumed without error
//   * all supported CSS length units decode to the correct ValueType
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

// CSS and its dependencies
#include "models/css.hpp"
#include "models/css_parser.hpp"
#include "models/dom.hpp"

// ---------------------------------------------------------------------------
// Minimal test helpers (same pattern as the other test suites)
// ---------------------------------------------------------------------------

static int css_pass = 0;
static int css_fail = 0;

#define SUITE_CHECK(cond, msg)                                      \
  do {                                                              \
    if (cond) {                                                     \
      ++css_pass;                                                   \
    } else {                                                        \
      ++css_fail;                                                   \
      std::printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg);\
    }                                                               \
  } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Load the fixture file, parse it, return a CSS instance (nullptr on failure).
static auto loadFixture(const char *path) -> CSSPtr {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return nullptr;
  std::ostringstream ss;
  ss << ifs.rdbuf();
  std::string buf = ss.str();
  auto css = CSS::Make("test", "", buf.c_str(), static_cast<int32_t>(buf.size()), 0);
  return css;
}

// Return the first value of the given property from rulesMap, or nullptr.
static auto firstValue(const CSS::RulesMap &rules, CSS::PropertyId pid) -> const CSS::Value * {
  const CSS::Values *vals = CSS::getValuesFromRules(rules, pid);
  if (!vals) return nullptr;
  auto it = vals->begin();
  return (it != vals->end()) ? *it : nullptr;
}

// Count how many rules in the map have a node whose tag equals `tag`.
static auto countRulesForTag(const CSS::RulesMap &rules, DOM::Tag tag) -> int {
  int count = 0;
  for (const auto &[sel, props] : rules) {
    for (const auto *node : sel->selectorNodeList) {
      if (node->tag == tag) { ++count; break; }
    }
  }
  return count;
}

// Count rules that have at least one selector node with the given class name.
static auto countRulesForClass(const CSS::RulesMap &rules, const std::string &cls) -> int {
  int count = 0;
  for (const auto &[sel, props] : rules) {
    for (const auto *node : sel->selectorNodeList) {
      for (const auto &c : node->classList) {
        if (c == cls) { ++count; goto next_rule; }
      }
    }
    next_rule:;
  }
  return count;
}

// Count rules that have at least one selector node with the given id.
static auto countRulesForId(const CSS::RulesMap &rules, const std::string &id) -> int {
  int count = 0;
  for (const auto &[sel, props] : rules) {
    for (const auto *node : sel->selectorNodeList) {
      if (node->idCount > 0 && node->id == id) { ++count; break; }
    }
  }
  return count;
}

// Return true if any selector node in the map has a universal (Tag::ANY) or no-tag entry
// together with zero id/class counts (the universal * selector).
static auto hasUniversalRule(const CSS::RulesMap &rules) -> bool {
  for (const auto &[sel, props] : rules) {
    bool allUniversal = true;
    for (const auto *node : sel->selectorNodeList) {
      if (node->tag != DOM::Tag::ANY && node->tag != DOM::Tag::NONE) { allUniversal = false; break; }
      if (node->classCount > 0 || node->idCount > 0) { allUniversal = false; break; }
    }
    if (allUniversal) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Individual sub-test functions
// ---------------------------------------------------------------------------

// ── test 2: element selectors ────────────────────────────────────────────────
static auto testElementSelectors(const CSS::RulesMap &rules) -> void {
  std::printf("  [testElementSelectors]\n");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::P)          > 0, "no rule for <p>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::BODY)       > 0, "no rule for <body>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::SPAN)       > 0, "no rule for <span>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::DIV)        > 0, "no rule for <div>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H1)         > 0, "no rule for <h1>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H2)         > 0, "no rule for <h2>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H3)         > 0, "no rule for <h3>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::BLOCKQUOTE) > 0, "no rule for <blockquote>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::EM)         > 0, "no rule for <em>");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::SUP)        > 0, "no rule for <sup>");
  SUITE_CHECK(hasUniversalRule(rules),                           "no rule for * (universal)");
}

// ── test 3: class & id selectors ─────────────────────────────────────────────
static auto testClassAndIdSelectors(const CSS::RulesMap &rules) -> void {
  std::printf("  [testClassAndIdSelectors]\n");
  SUITE_CHECK(countRulesForClass(rules, "highlight")      > 0, "no rule for .highlight");
  SUITE_CHECK(countRulesForClass(rules, "hidden")         > 0, "no rule for .hidden");
  SUITE_CHECK(countRulesForClass(rules, "lower")          > 0, "no rule for .lower");
  SUITE_CHECK(countRulesForClass(rules, "no-transform")   > 0, "no rule for .no-transform");
  SUITE_CHECK(countRulesForClass(rules, "left-align")     > 0, "no rule for .left-align");
  SUITE_CHECK(countRulesForClass(rules, "units-test")     > 0, "no rule for .units-test");
  SUITE_CHECK(countRulesForClass(rules, "important-test") > 0, "no rule for .important-test");
  SUITE_CHECK(countRulesForId(rules, "main")              > 0, "no rule for #main");
}

// ── test 4: @font-face generates a rule ──────────────────────────────────────
static auto testFontFace(const CSS::RulesMap &rules) -> void {
  std::printf("  [testFontFace]\n");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::FONT_FACE) > 0, "no @font-face rule");
}

// ── test 5: property values for <p> ──────────────────────────────────────────
static auto testParagraphProperties(const CSS::RulesMap &rules) -> void {
  std::printf("  [testParagraphProperties]\n");

  // Collect only the bare <p> rule: single node, no combinator, no class/id/qualifier.
  CSS::RulesMap matched;
  for (const auto &[sel, props] : rules) {
    auto it = sel->selectorNodeList.begin();
    if (it == sel->selectorNodeList.end()) continue;
    if (std::next(it) != sel->selectorNodeList.end()) continue; // multi-node
    const auto *node = *it;
    if (node->tag != DOM::Tag::P) continue;
    if (node->op != CSS::SelOp::NONE) continue;
    if (node->classCount != 0 || node->idCount != 0) continue;
    if (node->qualifier != CSS::Qualifier::NONE) continue;
    matched.insert({const_cast<CSS::Selector*>(sel), props});
  }

  // font-size: 12px
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::FONT_SIZE);
    SUITE_CHECK(v != nullptr,                              "p: no font-size value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::PX,    "p: font-size not PX");
      SUITE_CHECK(v->num == 12.0f,                       "p: font-size != 12");
    }
  }
  // font-style: italic → Fonts::FaceStyle::ITALIC
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::FONT_STYLE);
    SUITE_CHECK(v != nullptr,                             "p: no font-style value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::STR,   "p: font-style not STR");
      SUITE_CHECK(v->choice.faceStyle == Fonts::FaceStyle::ITALIC, "p: font-style != ITALIC");
    }
  }
  // font-weight: bold → Fonts::FaceStyle::BOLD
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::FONT_WEIGHT);
    SUITE_CHECK(v != nullptr,                             "p: no font-weight value");
    if (v) {
      SUITE_CHECK(v->choice.faceStyle == Fonts::FaceStyle::BOLD,   "p: font-weight != BOLD");
    }
  }
  // text-align: justify → Align::JUSTIFY
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::TEXT_ALIGN);
    SUITE_CHECK(v != nullptr,                             "p: no text-align value");
    if (v) {
      SUITE_CHECK(v->choice.align == CSS::Align::JUSTIFY,"p: text-align != JUSTIFY");
    }
  }
  // text-indent: 1em
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::TEXT_INDENT);
    SUITE_CHECK(v != nullptr,                             "p: no text-indent value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::EM,    "p: text-indent not EM");
      SUITE_CHECK(v->num == 1.0f,                        "p: text-indent != 1");
    }
  }
  // text-transform: uppercase → TextTransform::UPPERCASE
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::TEXT_TRANSFORM);
    SUITE_CHECK(v != nullptr,                             "p: no text-transform value");
    if (v) {
      SUITE_CHECK(v->choice.textTransform == CSS::TextTransform::UPPERCASE,
                  "p: text-transform != UPPERCASE");
    }
  }
  // line-height: 1.5em
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::LINE_HEIGHT);
    SUITE_CHECK(v != nullptr,                             "p: no line-height value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::EM,    "p: line-height not EM");
      SUITE_CHECK(v->num == 1.5f,                        "p: line-height != 1.5");
    }
  }
  // width: 80%
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::WIDTH);
    SUITE_CHECK(v != nullptr,                               "p: no width value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::PERCENT, "p: width not PERCENT");
      SUITE_CHECK(v->num == 80.0f,                         "p: width != 80");
    }
  }
  // height: 200px
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::HEIGHT);
    SUITE_CHECK(v != nullptr,                             "p: no height value");
    if (v) {
      SUITE_CHECK(v->valueType == CSS::ValueType::PX,    "p: height not PX");
      SUITE_CHECK(v->num == 200.0f,                      "p: height != 200");
    }
  }
  // display: block → Display::BLOCK
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::DISPLAY);
    SUITE_CHECK(v != nullptr,                              "p: no display value");
    if (v) {
      SUITE_CHECK(v->choice.display == CSS::Display::BLOCK,"p: display != BLOCK");
    }
  }
  // vertical-align: sub → VerticalAlign::SUB
  {
    const CSS::Value *v = firstValue(matched, CSS::PropertyId::VERTICAL_ALIGN);
    SUITE_CHECK(v != nullptr,                              "p: no vertical-align value");
    if (v) {
      SUITE_CHECK(v->choice.verticalAlign == CSS::VerticalAlign::SUB,
                  "p: vertical-align != SUB");
    }
  }
}

// ── test 6: class-selector values ────────────────────────────────────────────
static auto testClassProperties(const CSS::RulesMap &rules) -> void {
  std::printf("  [testClassProperties]\n");

  // .highlight: font-size 1.2em, text-align center, display inline
  for (const auto &[sel, props] : rules) {
    bool singleHighlight = false;
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->classCount == 1) {
      for (const auto &c : (*it)->classList) {
        if (c == "highlight") { singleHighlight = true; break; }
      }
    }
    if (!singleHighlight) continue;

    CSS::RulesMap single;
    single.insert({const_cast<CSS::Selector*>(sel), props});
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::FONT_SIZE);
      SUITE_CHECK(v != nullptr,                           ".highlight: no font-size");
      if (v) {
        SUITE_CHECK(v->valueType == CSS::ValueType::EM,  ".highlight: font-size not EM");
        SUITE_CHECK(std::abs(v->num - 1.2f) < 0.001f,   ".highlight: font-size != 1.2");
      }
    }
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::TEXT_ALIGN);
      SUITE_CHECK(v != nullptr,                           ".highlight: no text-align");
      if (v) {
        SUITE_CHECK(v->choice.align == CSS::Align::CENTER,".highlight: text-align != CENTER");
      }
    }
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::DISPLAY);
      SUITE_CHECK(v != nullptr,                           ".highlight: no display");
      if (v) {
        SUITE_CHECK(v->choice.display == CSS::Display::INLINE,".highlight: display != INLINE");
      }
    }
    break;
  }

  // #main: display inline-block, text-align right, font-size 2rem
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->idCount != 1 || (*it)->id != "main" || (*it)->tag != DOM::Tag::NONE) continue;

    CSS::RulesMap single;
    single.insert({const_cast<CSS::Selector*>(sel), props});
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::DISPLAY);
      SUITE_CHECK(v != nullptr,                               "#main: no display");
      if (v) {
        SUITE_CHECK(v->choice.display == CSS::Display::INLINE_BLOCK,"#main: display != INLINE_BLOCK");
      }
    }
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::TEXT_ALIGN);
      SUITE_CHECK(v != nullptr,                               "#main: no text-align");
      if (v) {
        SUITE_CHECK(v->choice.align == CSS::Align::RIGHT,    "#main: text-align != RIGHT");
      }
    }
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::FONT_SIZE);
      SUITE_CHECK(v != nullptr,                               "#main: no font-size");
      if (v) {
        SUITE_CHECK(v->valueType == CSS::ValueType::REM,     "#main: font-size not REM");
        SUITE_CHECK(v->num == 2.0f,                          "#main: font-size != 2");
      }
    }
    break;
  }
}

// ── test 7: enum values — display none, lowercase, left, super ───────────────
static auto testEnumValues(const CSS::RulesMap &rules) -> void {
  std::printf("  [testEnumValues]\n");

  // .hidden → display: none
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) { if (c == "hidden") { match = true; break; } }
    if (!match) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::DISPLAY) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->choice.display == CSS::Display::NONE, ".hidden: display != NONE");
        }
      }
    }
    break;
  }

  // .no-transform → text-transform: none
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) { if (c == "no-transform") { match = true; break; } }
    if (!match) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::TEXT_TRANSFORM) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->choice.textTransform == CSS::TextTransform::NONE,
                      ".no-transform: text-transform != NONE");
        }
      }
    }
    break;
  }

  // .lower → text-transform: lowercase
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) { if (c == "lower") { match = true; break; } }
    if (!match) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::TEXT_TRANSFORM) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->choice.textTransform == CSS::TextTransform::LOWERCASE,
                      ".lower: text-transform != LOWERCASE");
        }
      }
    }
    break;
  }

  // .left-align → text-align: left
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) { if (c == "left-align") { match = true; break; } }
    if (!match) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::TEXT_ALIGN) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->choice.align == CSS::Align::LEFT, ".left-align: text-align != LEFT");
        }
      }
    }
    break;
  }

  // sup → vertical-align: super
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->tag != DOM::Tag::SUP) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::VERTICAL_ALIGN) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->choice.verticalAlign == CSS::VerticalAlign::SUPER,
                      "sup: vertical-align != SUPER");
        }
      }
    }
    break;
  }

  // em → line-height: inherit
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->tag != DOM::Tag::EM) continue;
    for (const auto *p : *props) {
      if (p->id == CSS::PropertyId::LINE_HEIGHT) {
        auto vi = p->values.begin();
        if (vi != p->values.end()) {
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::INHERIT,
                      "em: line-height not INHERIT");
        }
      }
    }
    break;
  }
}

// ── test 8: length units ─────────────────────────────────────────────────────
static auto testUnits(const CSS::RulesMap &rules) -> void {
  std::printf("  [testUnits]\n");

  // .units-test: width 2cm, height 15mm, margin-left 0.5in, font-size 14pt, text-indent 1pc
  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) { if (c == "units-test") { match = true; break; } }
    if (!match) continue;

    for (const auto *p : *props) {
      auto vi = p->values.begin();
      if (vi == p->values.end()) continue;
      switch (p->id) {
        case CSS::PropertyId::WIDTH:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::CM,  ".units-test: width not CM");
          SUITE_CHECK((*vi)->num == 2.0f,                      ".units-test: width != 2");
          break;
        case CSS::PropertyId::HEIGHT:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::MM,  ".units-test: height not MM");
          SUITE_CHECK((*vi)->num == 15.0f,                     ".units-test: height != 15");
          break;
        case CSS::PropertyId::MARGIN_LEFT:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::IN,  ".units-test: margin-left not IN");
          SUITE_CHECK(std::abs((*vi)->num - 0.5f) < 0.001f,   ".units-test: margin-left != 0.5");
          break;
        case CSS::PropertyId::FONT_SIZE:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::PT,  ".units-test: font-size not PT");
          SUITE_CHECK((*vi)->num == 14.0f,                     ".units-test: font-size != 14");
          break;
        case CSS::PropertyId::TEXT_INDENT:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::PC,  ".units-test: text-indent not PC");
          SUITE_CHECK((*vi)->num == 1.0f,                      ".units-test: text-indent != 1");
          break;
        default: break;
      }
    }
    break;
  }
}

// ── test 9: @font-face property values ───────────────────────────────────────
static auto testFontFaceProperties(const CSS::RulesMap &rules) -> void {
  std::printf("  [testFontFaceProperties]\n");

  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->tag != DOM::Tag::FONT_FACE) continue;

    for (const auto *p : *props) {
      auto vi = p->values.begin();
      if (vi == p->values.end()) continue;
      switch (p->id) {
        case CSS::PropertyId::FONT_FAMILY:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::STR, "@font-face: font-family not STR");
          SUITE_CHECK((*vi)->str == "TestFont",                "@font-face: font-family != TestFont");
          break;
        case CSS::PropertyId::SRC:
          SUITE_CHECK((*vi)->valueType == CSS::ValueType::URL, "@font-face: src not URL");
          SUITE_CHECK((*vi)->str.find("test.ttf") != std::string::npos,
                      "@font-face: src url doesn't contain test.ttf");
          break;
        case CSS::PropertyId::FONT_STYLE:
          SUITE_CHECK((*vi)->choice.faceStyle == Fonts::FaceStyle::NORMAL,
                      "@font-face: font-style != NORMAL");
          break;
        case CSS::PropertyId::FONT_WEIGHT:
          SUITE_CHECK((*vi)->choice.faceStyle == Fonts::FaceStyle::BOLD,
                      "@font-face: font-weight != BOLD");
          break;
        default: break;
      }
    }
    break;
  }
}

// ── test 10: specificity ordering ────────────────────────────────────────────
// The rulesMap multimap is sorted ascending by specificity (least → most
// specific). An id selector (#main, classCount=1 in idCount) must appear after
// a plain element selector (p, tagCount=1 only) in the iteration order.
static auto testSpecificity(const CSS::RulesMap &rules) -> void {
  std::printf("  [testSpecificity]\n");

  uint32_t specP    = 0; // specificity of 'p' rule
  uint32_t specId   = 0; // specificity of '#main' rule
  uint32_t specCls  = 0; // specificity of '.highlight' rule

  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    // single-node <p>
    if ((*it)->tag == DOM::Tag::P && (*it)->classCount == 0 && (*it)->idCount == 0 &&
        std::next(it) == sel->selectorNodeList.end())
      specP = sel->specificity.value;
    // #main (no tag)
    if ((*it)->idCount == 1 && (*it)->id == "main" && (*it)->tag == DOM::Tag::NONE &&
        std::next(it) == sel->selectorNodeList.end())
      specId = sel->specificity.value;
    // .highlight (no tag)
    if ((*it)->classCount == 1 && (*it)->idCount == 0 && (*it)->tag == DOM::Tag::NONE) {
      for (const auto &c : (*it)->classList) {
        if (c == "highlight" && std::next(it) == sel->selectorNodeList.end())
          specCls = sel->specificity.value;
      }
    }
  }

  SUITE_CHECK(specP   > 0,         "specificity of 'p' is zero");
  SUITE_CHECK(specCls > 0,         "specificity of '.highlight' is zero");
  SUITE_CHECK(specId  > 0,         "specificity of '#main' is zero");
  SUITE_CHECK(specP   < specCls,   "element selector not less specific than class selector");
  SUITE_CHECK(specCls < specId,    "class selector not less specific than id selector");
}

// ── test 11: combinators ─────────────────────────────────────────────────────
static auto testCombinators(const CSS::RulesMap &rules) -> void {
  std::printf("  [testCombinators]\n");

  bool foundDescendant = false; // body p
  bool foundChild      = false; // div > p
  bool foundAdjacent   = false; // h1 + p
  bool foundFirstChild = false; // p:first-child

  for (const auto &[sel, props] : rules) {
    auto it = sel->selectorNodeList.begin();
    if (it == sel->selectorNodeList.end()) continue;
    auto nit = std::next(it);
    if (nit == sel->selectorNodeList.end()) {
      // single-node: check :first-child qualifier
      if ((*it)->tag == DOM::Tag::P && (*it)->qualifier == CSS::Qualifier::FIRST_CHILD)
        foundFirstChild = true;
      continue;
    }
    // two-node selectors — remember the list is stored reversed (most-specific node first)
    // it = right-hand node (e.g. <p>), nit = left-hand node (e.g. <body>/<div>/<h1>)
    if ((*it)->tag == DOM::Tag::P) {
      if ((*it)->op == CSS::SelOp::DESCENDANT && (*nit)->tag == DOM::Tag::BODY)
        foundDescendant = true;
      if ((*it)->op == CSS::SelOp::CHILD && (*nit)->tag == DOM::Tag::DIV)
        foundChild = true;
      if ((*it)->op == CSS::SelOp::ADJACENT && (*nit)->tag == DOM::Tag::H1)
        foundAdjacent = true;
    }
  }

  SUITE_CHECK(foundDescendant, "descendant combinator (body p) not found");
  SUITE_CHECK(foundChild,      "child combinator (div > p) not found");
  SUITE_CHECK(foundAdjacent,   "adjacent combinator (h1 + p) not found");
  SUITE_CHECK(foundFirstChild, ":first-child pseudo-class (p:first-child) not found");
}

// ── test 12: multi-selector comma (h1, h2, h3) ────────────────────────────
// The parser must produce separate rules for each selector.
static auto testCommaSelector(const CSS::RulesMap &rules) -> void {
  std::printf("  [testCommaSelector]\n");
  // h1, h2, h3 { font-weight: bold; line-height: 120%; }
  // Each becomes an independent rule sharing the same Properties*.
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H1) > 0, "comma-list: no rule for h1");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H2) > 0, "comma-list: no rule for h2");
  SUITE_CHECK(countRulesForTag(rules, DOM::Tag::H3) > 0, "comma-list: no rule for h3");
}

// ── test 13: margin shorthand (multi-value) ───────────────────────────────
// margin: 10px 20px must produce two values in the forward_list.
static auto testMarginShorthand(const CSS::RulesMap &rules) -> void {
  std::printf("  [testMarginShorthand]\n");

  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    if ((*it)->tag != DOM::Tag::P || (*it)->classCount != 0 || (*it)->idCount != 0) continue;
    if (std::next(it) != sel->selectorNodeList.end()) continue;

    for (const auto *p : *props) {
      if (p->id != CSS::PropertyId::MARGIN) continue;
      int cnt = 0;
      for (auto vi = p->values.begin(); vi != p->values.end(); ++vi) ++cnt;
      SUITE_CHECK(cnt >= 2, "p margin shorthand: fewer than 2 values");
      auto vi = p->values.begin();
      if (vi != p->values.end()) {
        SUITE_CHECK((*vi)->valueType == CSS::ValueType::PX, "p margin[0] not PX");
        SUITE_CHECK((*vi)->num == 10.0f,                    "p margin[0] != 10");
        ++vi;
      }
      if (vi != p->values.end()) {
        SUITE_CHECK((*vi)->valueType == CSS::ValueType::PX, "p margin[1] not PX");
        SUITE_CHECK((*vi)->num == 20.0f,                    "p margin[1] != 20");
      }
    }
    break;
  }
}

// ── test 14: !important does not corrupt subsequent values ────────────────
static auto testImportant(const CSS::RulesMap &rules) -> void {
  std::printf("  [testImportant]\n");

  for (const auto &[sel, props] : rules) {
    if (sel->selectorNodeList.empty()) continue;
    auto it = sel->selectorNodeList.begin();
    bool match = false;
    for (const auto &c : (*it)->classList) {
      if (c == "important-test") { match = true; break; }
    }
    if (!match) continue;

    CSS::RulesMap single;
    single.insert({const_cast<CSS::Selector*>(sel), props});
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::FONT_WEIGHT);
      SUITE_CHECK(v != nullptr,                              "!important: no font-weight");
      if (v) {
        SUITE_CHECK(v->choice.faceStyle == Fonts::FaceStyle::BOLD,
                    "!important: font-weight value corrupted");
      }
    }
    {
      const CSS::Value *v = firstValue(single, CSS::PropertyId::FONT_SIZE);
      SUITE_CHECK(v != nullptr,                             "!important: no font-size");
      if (v) {
        SUITE_CHECK(v->valueType == CSS::ValueType::PX,    "!important: font-size not PX");
        SUITE_CHECK(v->num == 16.0f,                       "!important: font-size != 16");
      }
    }
    break;
  }
}

// ── test 15: inline-style parse path ─────────────────────────────────────────
static auto testInlineStyle() -> void {
  std::printf("  [testInlineStyle]\n");

  const char *inlineCss = "font-size: 14px; font-weight: bold; color: red;";
  auto css = CSS::Make("inline", DOM::Tag::SPAN,
                       inlineCss, static_cast<int32_t>(strlen(inlineCss)), 0);
  SUITE_CHECK(css != nullptr,               "inline CSS::Make returned nullptr");
  if (!css) return;
  SUITE_CHECK(!css->rulesMap.empty(),        "inline rulesMap is empty");

  const CSS::Value *v = firstValue(css->rulesMap, CSS::PropertyId::FONT_SIZE);
  SUITE_CHECK(v != nullptr,                 "inline: no font-size");
  if (v) {
    SUITE_CHECK(v->valueType == CSS::ValueType::PX, "inline: font-size not PX");
    SUITE_CHECK(v->num == 14.0f,                    "inline: font-size != 14");
  }
  const CSS::Value *w = firstValue(css->rulesMap, CSS::PropertyId::FONT_WEIGHT);
  SUITE_CHECK(w != nullptr,                 "inline: no font-weight");
  if (w) {
    SUITE_CHECK(w->choice.faceStyle == Fonts::FaceStyle::BOLD, "inline: font-weight != BOLD");
  }
}

// ---------------------------------------------------------------------------
// Suite entry point
// ---------------------------------------------------------------------------
auto testCss() -> bool {
  css_pass = 0;
  css_fail = 0;

  std::printf("\n--- CSS PARSER TESTS ---\n");

  // ── load fixture ──────────────────────────────────────────────────────────
  auto css = loadFixture("test/fixtures/test.css");
  if (!css) {
    std::printf("  FATAL: could not load test/fixtures/test.css\n");
    ++css_fail;
    return false;
  }
  SUITE_CHECK(!css->rulesMap.empty(), "rulesMap is empty");

  // Run sub-tests that need the rulesMap.
  testElementSelectors  (css->rulesMap);
  testClassAndIdSelectors(css->rulesMap);
  testFontFace          (css->rulesMap);
  testParagraphProperties(css->rulesMap);
  testClassProperties   (css->rulesMap);
  testEnumValues        (css->rulesMap);
  testUnits             (css->rulesMap);
  testFontFaceProperties(css->rulesMap);
  testSpecificity       (css->rulesMap);
  testCombinators       (css->rulesMap);
  testCommaSelector     (css->rulesMap);
  testMarginShorthand   (css->rulesMap);
  testImportant         (css->rulesMap);

  // Inline-style test (creates its own CSS object).
  testInlineStyle();

  std::printf("\n  CSS tests: %d passed, %d failed\n", css_pass, css_fail);
  return css_fail == 0;
}
