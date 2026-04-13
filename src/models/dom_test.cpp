// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "dom_test.hpp"
#include "dom.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp_log.h"
  static const char *const TAG = "dom_test";
  #define DT_LOG(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#else
  #include <cstdio>
  #define DT_LOG(fmt, ...) std::printf("[dom_test] " fmt "\n", ##__VA_ARGS__)
#endif

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
static void test_dom_make() {
  DT_LOG("--- DOM::Make() ---");

  auto dom = DOM::Make();

  DT_CHECK(dom != nullptr,          "DOM::Make() returns a non-null pointer");
  DT_CHECK(dom->body != nullptr,    "body node is initialised after Make()");
  DT_CHECK(dom->body->tag == DOM::Tag::BODY, "body node carries the BODY tag");
  DT_CHECK(dom->body->father == nullptr,    "body node has no parent");
}

// ===========================================================================
// DOM::add_child() — child-node creation
// ===========================================================================
static void test_dom_add_child() {
  DT_LOG("--- DOM::add_child() ---");

  auto dom = DOM::Make();
  if (!dom) { DT_LOG("SKIP add_child tests — Make() returned null"); return; }

  DOM::Node *p = dom->add_child(dom->body, DOM::Tag::P);

  DT_CHECK(p != nullptr,               "add_child returns a non-null node");
  DT_CHECK(p->tag == DOM::Tag::P,      "child node carries the requested tag");
  DT_CHECK(p->father == dom->body,     "child's father points to the parent node");

  DOM::Node *span = dom->add_child(p, DOM::Tag::SPAN);

  DT_CHECK(span != nullptr,            "nested add_child returns a non-null node");
  DT_CHECK(span->tag == DOM::Tag::SPAN, "nested child carries the requested tag");
  DT_CHECK(span->father == p,          "nested child's father points to correct parent");
}

} // namespace

// ===========================================================================
// Public entry point
// ===========================================================================
bool dom_run_tests() {
  s_pass = 0;
  s_fail = 0;

  test_dom_make();
  test_dom_add_child();

  DT_LOG("--- DOM tests complete: %d passed, %d failed ---", s_pass, s_fail);
  return s_fail == 0;
}
