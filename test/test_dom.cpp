// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// DOM test suite
//
// Full implementation lives here (Linux test build).  The ESP32 build uses
// the copy in src/models/dom_test.cpp which is kept in sync.
// ---------------------------------------------------------------------------

#include "dom.hpp"
#include "test_stats.hpp"

#include <cstdio>

#define DT_LOG(fmt, ...) std::printf("[dom_test] " fmt "\n", ##__VA_ARGS__)

// ===========================================================================
// Internal helpers
// ===========================================================================

namespace {

static int s_pass = 0;
static int s_fail = 0;

#define DT_CHECK(cond, msg)                                                                        \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      DT_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                                             \
      ++s_fail;                                                                                    \
    } else {                                                                                       \
      DT_LOG("PASS " msg);                                                                         \
      ++s_pass;                                                                                    \
    }                                                                                              \
  } while (0)

// ===========================================================================
// DOM::Make() — instantiation and basic structure
// ===========================================================================
static void testDomMake() {
  DT_LOG("--- DOM::Make() ---");

  auto dom = DOM::Make();

  DT_CHECK(dom != nullptr, "DOM::Make() returns a non-null pointer");
  DT_CHECK(dom->body != nullptr, "body node is initialised after Make()");
  DT_CHECK(dom->body->tag == DOM::Tag::BODY, "body node carries the BODY tag");
  DT_CHECK(dom->body->father == nullptr, "body node has no parent");
}

// ===========================================================================
// DOM::addChild() — child-node creation
// ===========================================================================
static void testDomAddChild() {
  DT_LOG("--- DOM::addChild() ---");

  auto dom = DOM::Make();
  if (!dom) {
    DT_LOG("SKIP addChild tests — Make() returned null");
    return;
  }

  DOM::Node *p = dom->addChild(dom->body, DOM::Tag::P);

  DT_CHECK(p != nullptr, "addChild returns a non-null node");
  DT_CHECK(p->tag == DOM::Tag::P, "child node carries the requested tag");
  DT_CHECK(p->father == dom->body, "child's father points to the parent node");

  DOM::Node *span = dom->addChild(p, DOM::Tag::SPAN);

  DT_CHECK(span != nullptr, "nested addChild returns a non-null node");
  DT_CHECK(span->tag == DOM::Tag::SPAN, "nested child carries the requested tag");
  DT_CHECK(span->father == p, "nested child's father points to correct parent");
}

// ===========================================================================
// DOM tag lookup — tags map
// ===========================================================================
static void testDomTags() {
  DT_LOG("--- DOM::tags map ---");

  DT_CHECK(DOM::tags.count("p") && DOM::tags.at("p") == DOM::Tag::P, "\"p\" maps to Tag::P");
  DT_CHECK(DOM::tags.count("div") && DOM::tags.at("div") == DOM::Tag::DIV,
           "\"div\" maps to Tag::DIV");
  DT_CHECK(DOM::tags.count("span") && DOM::tags.at("span") == DOM::Tag::SPAN,
           "\"span\" maps to Tag::SPAN");
  DT_CHECK(DOM::tags.count("body") && DOM::tags.at("body") == DOM::Tag::BODY,
           "\"body\" maps to Tag::BODY");
  DT_CHECK(DOM::tags.count("img") && DOM::tags.at("img") == DOM::Tag::IMG,
           "\"img\" maps to Tag::IMG");
  DT_CHECK(!DOM::tags.count("unknown"), "unknown tag not present in map");
}

// ===========================================================================
// DOM multi-child tree — sibling and depth relationships
// ===========================================================================
static void testDomTree() {
  DT_LOG("--- DOM tree structure ---");

  auto dom = DOM::Make();

  DOM::Node *div  = dom->addChild(dom->body, DOM::Tag::DIV);
  DOM::Node *p1   = dom->addChild(div, DOM::Tag::P);
  DOM::Node *p2   = dom->addChild(div, DOM::Tag::P);
  DOM::Node *span = dom->addChild(p1, DOM::Tag::SPAN);

  DT_CHECK(div->father == dom->body, "div parent is body");
  DT_CHECK(p1->father == div, "p1 parent is div");
  DT_CHECK(p2->father == div, "p2 parent is div");
  DT_CHECK(span->father == p1, "span parent is p1");

  // div's children list contains both p1 and p2.
  int childCount = 0;
  for (auto *c : div->children) {
    (void)c;
    ++childCount;
  }
  DT_CHECK(childCount == 2, "div has exactly 2 children");

  // p1 has one child (span).
  childCount = 0;
  for (auto *c : p1->children) {
    (void)c;
    ++childCount;
  }
  DT_CHECK(childCount == 1, "p1 has exactly 1 child");
}

// ===========================================================================
// DOM::Node::addClass / classList
// ===========================================================================
static void testDomClasses() {
  DT_LOG("--- Node::addClass / classList ---");

  auto dom = DOM::Make();
  auto *p  = dom->addChild(dom->body, DOM::Tag::P);

  p->addClass("intro");
  p->addClass("highlight");

  bool hasIntro     = false;
  bool hasHighlight = false;
  for (const auto &cls : p->classList) {
    if (cls == "intro") hasIntro = true;
    if (cls == "highlight") hasHighlight = true;
  }
  DT_CHECK(hasIntro, "classList contains \"intro\"");
  DT_CHECK(hasHighlight, "classList contains \"highlight\"");
}

// ===========================================================================
// DOM::Node::addId
// ===========================================================================
static void testDomId() {
  DT_LOG("--- Node::addId ---");

  auto dom = DOM::Make();
  auto *p  = dom->addChild(dom->body, DOM::Tag::P);

  p->addId("main-paragraph");
  DT_CHECK(p->id == "main-paragraph", "id set correctly via addId()");
}

} // namespace

// ===========================================================================
// Public entry point
// ===========================================================================
auto testDom() -> TestStats {
  s_pass = 0;
  s_fail = 0;

  testDomMake();
  testDomAddChild();
  testDomTags();
  testDomTree();
  testDomClasses();
  testDomId();

  DT_LOG("--- DOM tests complete: %d passed, %d failed ---", s_pass, s_fail);
  return TestStats{s_pass, s_fail};
}
