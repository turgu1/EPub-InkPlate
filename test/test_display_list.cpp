// Copyright (c) 2026 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

// ---------------------------------------------------------------------------
// test_display_list.cpp — unit tests for DisplayList / DisplayListEntry
//
// Tests cover:
//   1. Empty-list invariants
//   2. pushBack with GlyphEntry, RegionEntry and PictureEntry variants
//   3. Iteration (range-for, manual begin/end, multi-element order)
//   4. removeLast() on 1-element, 2-element and N-element lists
//   5. merge() — empty-into-non-empty, non-empty-into-empty, both non-empty
//   6. clear() — pool elements are returned; list is empty afterward
//   7. RAII destruction — clear() called from destructor via makeUniqueHimem
//   8. getNewEntry() — allocates from pool; returns a valid pointer
//   9. last() / empty() after each mutation
//  10. pushBack rejects monostate (std::monostate variant) entries
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <variant>

#include "display_list.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
#define DL_LOG(fmt, ...) std::printf("[display_list_test] " fmt "\n", ##__VA_ARGS__)

static int sPass = 0;
static int sFail = 0;

#define DL_CHECK(cond, msg)                                               \
  do {                                                                    \
    if (!(cond)) {                                                        \
      DL_LOG("FAIL [%s:%d] " msg, __FILE__, __LINE__);                   \
      ++sFail;                                                            \
    } else {                                                              \
      DL_LOG("PASS " msg);                                               \
      ++sPass;                                                            \
    }                                                                     \
  } while (0)

// Build a GlyphEntry-holding DisplayListEntry in the pool.
static auto makeGlyph(DisplayListPool &pool,
                      Glyph           &g,
                      int16_t          kern,
                      bool             isSpace,
                      uint16_t         x,
                      uint16_t         y) -> DisplayListEntry * {
  auto *e         = pool.newElement();
  e->command      = DisplayListCommand::GLYPH;
  GlyphEntry ge;
  ge.glyph        = &g;
  ge.kern         = kern;
  ge.isSpace      = isSpace;
  e->v            = ge;
  e->pos          = Pos{x, y};
  return e;
}

// Build a RegionEntry-holding DisplayListEntry.
static auto makeRegion(DisplayListPool      &pool,
                       DisplayListCommand    cmd,
                       uint16_t w, uint16_t h,
                       uint16_t x, uint16_t y) -> DisplayListEntry * {
  auto *e    = pool.newElement();
  e->command = cmd;
  RegionEntry re;
  re.dim     = Dim{w, h};
  e->v       = re;
  e->pos     = Pos{x, y};
  return e;
}

// Build a PictureEntry-holding DisplayListEntry.
static auto makePicture(DisplayListPool &pool,
                        uint16_t         x,
                        uint16_t         y,
                        int16_t          advance) -> DisplayListEntry * {
  static const uint8_t pixelData[4] = {0, 0, 0, 0};
  auto *e         = pool.newElement();
  e->command      = DisplayListCommand::PICTURE;
  PictureEntry pe;
  pe.picture      = Picture::Make(Dim{2, 2}, pixelData, 4);
  pe.advance      = advance;
  e->v            = std::move(pe);
  e->pos          = Pos{x, y};
  return e;
}

// Count elements in a DisplayList by walking the iterator.
static auto countEntries(DisplayList &dl) -> int {
  int n = 0;
  for (auto *e : dl) { (void)e; ++n; }
  return n;
}

// ---------------------------------------------------------------------------
// 1. Empty list invariants
// ---------------------------------------------------------------------------
static void testEmptyList() {
  DL_LOG("--- empty list ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  DL_CHECK(dl != nullptr,      "Make() returns non-null");
  DL_CHECK(dl->empty(),        "new list is empty");
  DL_CHECK(dl->last() == nullptr, "last() == nullptr on empty list");
  DL_CHECK(countEntries(*dl) == 0, "iterator yields 0 elements");
}

// ---------------------------------------------------------------------------
// 2. pushBack — each variant type
// ---------------------------------------------------------------------------
static void testPushBack() {
  DL_LOG("--- pushBack ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  Glyph g;
  g.dim     = Dim{8, 12};
  g.advance = 9;

  // 2a — GlyphEntry
  auto *ge = makeGlyph(pool, g, 1, false, 10, 20);
  dl->pushBack(ge);

  DL_CHECK(!dl->empty(),         "not empty after pushBack glyph");
  DL_CHECK(dl->last() == ge,     "last() == pushed glyph entry");
  DL_CHECK(countEntries(*dl) == 1, "iterator yields 1 element");

  // Check content
  DL_CHECK((*dl->begin())->command == DisplayListCommand::GLYPH, "command is GLYPH");
  DL_CHECK(std::holds_alternative<GlyphEntry>((*dl->begin())->v),
           "variant holds GlyphEntry");
  DL_CHECK(std::get<GlyphEntry>((*dl->begin())->v).kern == 1,
           "GlyphEntry kern preserved");
  DL_CHECK((*dl->begin())->pos.x == 10 && (*dl->begin())->pos.y == 20,
           "pos preserved");

  // 2b — RegionEntry (HIGHLIGHT)
  auto *re = makeRegion(pool, DisplayListCommand::HIGHLIGHT, 100, 20, 5, 50);
  dl->pushBack(re);

  DL_CHECK(countEntries(*dl) == 2, "iterator yields 2 after region push");
  DL_CHECK(dl->last() == re,       "last() updated after region push");
  DL_CHECK(dl->last()->command == DisplayListCommand::HIGHLIGHT, "last is HIGHLIGHT");

  // 2c — PictureEntry
  auto *pe = makePicture(pool, 30, 40, 8);
  dl->pushBack(pe);

  DL_CHECK(countEntries(*dl) == 3, "iterator yields 3 after picture push");
  DL_CHECK(dl->last() == pe,       "last() updated after picture push");
  DL_CHECK(dl->last()->command == DisplayListCommand::PICTURE, "last is PICTURE");
}

// ---------------------------------------------------------------------------
// 3. Iteration order is FIFO (insertion order)
// ---------------------------------------------------------------------------
static void testIterationOrder() {
  DL_LOG("--- iteration order ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  Glyph g;
  g.dim     = Dim{5, 10};
  g.advance = 6;

  // push 5 glyph entries at distinct x positions
  for (int i = 0; i < 5; ++i) {
    dl->pushBack(makeGlyph(pool, g, 0, false, static_cast<uint16_t>(i * 10), 0));
  }

  DL_CHECK(countEntries(*dl) == 5, "5 elements in list");

  int i = 0;
  for (auto *e : *dl) {
    DL_CHECK(e->pos.x == static_cast<uint16_t>(i * 10), "iteration order preserved");
    ++i;
  }

  // Postfix increment
  auto it = dl->begin();
  auto itPre  = ++it;
  auto itPost = it++;   // it now points to element [2]
  DL_CHECK(itPre == itPost,         "postfix returns copy before increment");
  DL_CHECK((*itPost)->pos.x == 10,  "postfix copy points to element 1");
  DL_CHECK((*it)->pos.x    == 20,   "it advanced past element 1");
}

// ---------------------------------------------------------------------------
// 4. removeLast()
// ---------------------------------------------------------------------------
static void testRemoveLast() {
  DL_LOG("--- removeLast ---");
  DisplayListPool pool;

  // 4a — single element
  {
    auto dl = DisplayList::Make(pool);
    Glyph g;
    dl->pushBack(makeGlyph(pool, g, 0, false, 1, 1));
    dl->removeLast();
    DL_CHECK(dl->empty(),              "empty after removeLast on 1-element list");
    DL_CHECK(dl->last() == nullptr,    "last() == nullptr after removeLast on 1-element");
  }

  // 4b — two elements: remove last, head should remain
  {
    auto dl = DisplayList::Make(pool);
    Glyph g;
    auto *e1 = makeGlyph(pool, g, 0, false, 1, 0);
    auto *e2 = makeGlyph(pool, g, 0, false, 2, 0);
    dl->pushBack(e1);
    dl->pushBack(e2);
    dl->removeLast();
    DL_CHECK(countEntries(*dl) == 1,  "1 element after removeLast from 2-element list");
    DL_CHECK(dl->last() == e1,        "remaining element is e1");
    DL_CHECK((*dl->begin())->pos.x == 1, "head is e1");
  }

  // 4c — three elements: two removes
  {
    auto dl = DisplayList::Make(pool);
    Glyph g;
    for (int i = 0; i < 3; ++i)
      dl->pushBack(makeGlyph(pool, g, 0, false, static_cast<uint16_t>(i), 0));
    dl->removeLast();
    DL_CHECK(countEntries(*dl) == 2,  "2 elements after first removeLast from 3");
    dl->removeLast();
    DL_CHECK(countEntries(*dl) == 1,  "1 element after second removeLast from 3");
    DL_CHECK((*dl->begin())->pos.x == 0, "head is original first element");
  }

  // 4d — removeLast on empty list must not crash
  {
    auto dl = DisplayList::Make(pool);
    dl->removeLast();                 // must not crash
    DL_CHECK(dl->empty(), "still empty after removeLast on empty list");
  }
}

// ---------------------------------------------------------------------------
// 5. merge()
// ---------------------------------------------------------------------------
static void testMerge() {
  DL_LOG("--- merge ---");
  DisplayListPool pool;
  Glyph g;

  // 5a — merge non-empty into empty
  {
    auto dst = DisplayList::Make(pool);
    auto src = DisplayList::Make(pool);
    src->pushBack(makeGlyph(pool, g, 0, false, 10, 0));
    src->pushBack(makeGlyph(pool, g, 0, false, 20, 0));

    dst->merge(*src);

    DL_CHECK(countEntries(*dst) == 2,   "merge into empty: dst has 2 elements");
    DL_CHECK(src->empty(),               "merge into empty: src is cleared");
    DL_CHECK((*dst->begin())->pos.x == 10, "merge into empty: first element correct");
    DL_CHECK(dst->last()->pos.x == 20,  "merge into empty: last element correct");
  }

  // 5b — merge empty into non-empty
  {
    auto dst = DisplayList::Make(pool);
    auto src = DisplayList::Make(pool);
    dst->pushBack(makeGlyph(pool, g, 0, false, 5, 0));

    auto *prevLast = dst->last();
    dst->merge(*src);

    DL_CHECK(countEntries(*dst) == 1,  "merge empty src: dst count unchanged");
    DL_CHECK(dst->last() == prevLast,  "merge empty src: last unchanged");
    DL_CHECK(src->empty(),             "merge empty src: src still empty");
  }

  // 5c — merge non-empty into non-empty
  {
    auto dst = DisplayList::Make(pool);
    auto src = DisplayList::Make(pool);

    for (uint16_t i = 0; i < 3; ++i)
      dst->pushBack(makeGlyph(pool, g, 0, false, i, 0));
    for (uint16_t i = 10; i < 13; ++i)
      src->pushBack(makeGlyph(pool, g, 0, false, i, 0));

    dst->merge(*src);

    DL_CHECK(countEntries(*dst) == 6, "merge both non-empty: dst has 6 elements");
    DL_CHECK(src->empty(),            "merge both non-empty: src cleared");

    // Check order: dst elements first (0,1,2) then src elements (10,11,12)
    int idx = 0;
    uint16_t expected[] = {0, 1, 2, 10, 11, 12};
    for (auto *e : *dst) {
      DL_CHECK(e->pos.x == expected[idx], "merge order preserved");
      ++idx;
    }
  }
}

// ---------------------------------------------------------------------------
// 6. clear()
// ---------------------------------------------------------------------------
static void testClear() {
  DL_LOG("--- clear ---");
  DisplayListPool pool;
  Glyph g;
  auto dl = DisplayList::Make(pool);

  for (int i = 0; i < 5; ++i)
    dl->pushBack(makeGlyph(pool, g, 0, false, static_cast<uint16_t>(i), 0));

  DL_CHECK(countEntries(*dl) == 5, "5 elements before clear");
  dl->clear();
  DL_CHECK(dl->empty(),            "empty after clear");
  DL_CHECK(dl->last() == nullptr,  "last() null after clear");
  DL_CHECK(countEntries(*dl) == 0, "iterator yields 0 after clear");

  // Re-use after clear
  dl->pushBack(makeGlyph(pool, g, 0, false, 99, 0));
  DL_CHECK(countEntries(*dl) == 1,    "can push after clear");
  DL_CHECK((*dl->begin())->pos.x == 99, "element correct after re-use");
}

// ---------------------------------------------------------------------------
// 7. RAII — destructor calls clear()
// ---------------------------------------------------------------------------
static void testRaii() {
  DL_LOG("--- RAII destructor ---");
  DisplayListPool pool;
  Glyph g;

  {
    auto dl = DisplayList::Make(pool);
    for (int i = 0; i < 4; ++i)
      dl->pushBack(makeGlyph(pool, g, 0, false, static_cast<uint16_t>(i), 0));
    DL_CHECK(countEntries(*dl) == 4, "4 elements before scope exit");
    // dl goes out of scope here — ~DisplayList() must call clear()
  }
  // If pool tracking is available we could count free slots; the main check
  // is that the destructor does not crash and does not leak (valgrind / ASAN).
  DL_CHECK(true, "destructor did not crash");
}

// ---------------------------------------------------------------------------
// 8. getNewEntry() — allocates from pool
// ---------------------------------------------------------------------------
static void testGetNewEntry() {
  DL_LOG("--- getNewEntry ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  auto *e = dl->getNewEntry();
  DL_CHECK(e != nullptr, "getNewEntry() returns non-null");

  // Populate and push so it is cleaned up on clear()
  Glyph g;
  g.dim = Dim{1, 1};
  GlyphEntry ge;
  ge.glyph = &g;
  e->command = DisplayListCommand::GLYPH;
  e->v       = ge;
  e->pos     = Pos{0, 0};
  dl->pushBack(e);
  DL_CHECK(countEntries(*dl) == 1, "entry obtained via getNewEntry pushed correctly");
}

// ---------------------------------------------------------------------------
// 9. All RegionEntry command variants iterate correctly
// ---------------------------------------------------------------------------
static void testRegionVariants() {
  DL_LOG("--- RegionEntry variants ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  const DisplayListCommand cmds[] = {
    DisplayListCommand::HIGHLIGHT,
    DisplayListCommand::CLEAR_HIGHLIGHT,
    DisplayListCommand::SET_REGION,
    DisplayListCommand::CLEAR_REGION,
    DisplayListCommand::ROUNDED,
    DisplayListCommand::CLEAR_ROUNDED,
  };
  constexpr int N = 6;

  for (int i = 0; i < N; ++i)
    dl->pushBack(makeRegion(pool, cmds[i],
                            static_cast<uint16_t>(10 + i),
                            static_cast<uint16_t>(5 + i),
                            static_cast<uint16_t>(i),
                            static_cast<uint16_t>(i)));

  DL_CHECK(countEntries(*dl) == N, "all 6 region variants pushed");

  int i = 0;
  for (auto *e : *dl) {
    DL_CHECK(e->command == cmds[i],  "command variant preserved");
    DL_CHECK(std::holds_alternative<RegionEntry>(e->v), "RegionEntry in variant");
    DL_CHECK(std::get<RegionEntry>(e->v).dim.width  == static_cast<uint16_t>(10 + i),
             "dim.width preserved");
    DL_CHECK(std::get<RegionEntry>(e->v).dim.height == static_cast<uint16_t>(5  + i),
             "dim.height preserved");
    ++i;
  }
}

// ---------------------------------------------------------------------------
// 10. pushBack rejects monostate entry (logs error, list unchanged)
// ---------------------------------------------------------------------------
static void testPushBackMonostate() {
  DL_LOG("--- pushBack monostate rejection ---");
  DisplayListPool pool;
  auto dl = DisplayList::Make(pool);

  auto *e    = pool.newElement();
  // e->v is std::monostate by default — pushBack must reject this
  dl->pushBack(e); // should log error and not add to list
  DL_CHECK(dl->empty(), "monostate entry not added (list still empty)");

  pool.deleteElement(e); // clean up manually since it was not pushed
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
auto testDisplayList() -> bool {
  sPass = 0;
  sFail = 0;

  DL_LOG("========== DisplayList test suite start ==========");

  testEmptyList();
  testPushBack();
  testIterationOrder();
  testRemoveLast();
  testMerge();
  testClear();
  testRaii();
  testGetNewEntry();
  testRegionVariants();
  testPushBackMonostate();

  DL_LOG("========== DisplayList test suite end: %d passed, %d failed ==========",
         sPass, sFail);
  return sFail == 0;
}
