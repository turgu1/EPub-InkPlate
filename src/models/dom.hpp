// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

#include "himem_pool.hpp"
#include "simple_list.hpp"

using DOMPtr = HimemUniquePtr<class DOM>;
class DOM {
public:
  /**
   * @brief HTML tags supported by the application
   *
   * These are the tags required by the application. They will be used to limit the retrieval of
   * rules to the one that uses only these tags.
   *
   * The pseudo tags NONE and ANY are for selector definition, when no tag is identified
   * in the selector or the '*' selector is used. The pseudo tag FONT_FACE is to grab
   * @font-face definitions in the rules-set. Same for @page vs PAGE.
   */
  // Tag used to isolate DOM allocator bindings from other subsystems.
  struct ScopedListPoolTag;

  class DOMPools;
  class PoolsGuard;

  /**
   * @brief A SimpleList backed by a per-instance bound PSRAM pool.
   */
  template <typename T, std::size_t BlockSize>
  using SimpleListSinglePool =
      SimpleList<T, ScopedHimemPoolAllocator<T, ScopedListPoolTag, BlockSize>>;

  // clang-format off
  enum class Tag : uint8_t {
    NONE, BODY, P, LI, BREAK, H1, H2, H3, H4, H5, H6, B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
        BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE, SUB, SUP, TABLE, COLGROUP, TR, TD, TH, TFOOT, 
        THEAD, COL, CAPTION, TBODY
  };
  // clang-format on

  using Tags = std::map<std::string, Tag>;

  static Tags tags;

  class Node {
  public:
    using ClassList = SimpleListSinglePool<HimemString, 256>;
    using NodeList  = SimpleListSinglePool<Node *, 512>;

    Node *father;
    Node *predecessor;
    NodeList children;
    ClassList classList;
    HimemString id;
    Tag tag;
    bool firstChild;

    Node(Node *theFather, Tag theTag);
    ~Node();

    auto addClass(const char *theClass) -> Node *;
    auto addClasses(const char *theClasses) -> Node *;
    auto addId(const char *theId) -> Node *;
    auto showChildren(NodeList::ConstIterator nodeIt, int8_t lev) -> void const;
    auto show(uint8_t level) -> void const;
  };

private:
  DOMPools *pools{nullptr};
  PoolsGuard *guard{nullptr};

  SimpleListSinglePool<Node, 100> nodeList{};

  DOM(DOMPools &poolsRef);

public:
  class DOMPools {
  public:
    using NodeClassListNode = Node::ClassList::Node;
    using NodeNodeListNode  = Node::NodeList::Node;
    using DomNodeListNode   = SimpleListSinglePool<Node, 100>::Node;

    HimemPool<NodeClassListNode> nodeClassListPool{256};
    HimemPool<NodeNodeListNode> nodeNodeListPool{512};
    HimemPool<DomNodeListNode> domNodeListPool{100};
  };

  class PoolsGuard {
  public:
    explicit PoolsGuard(DOMPools &poolsRef);
    ~PoolsGuard();

  private:
    DOMPools &poolsRef;
  };

  ~DOM();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend HimemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(DOMPools &poolsRef) { return makeUniqueHimem<DOM>(poolsRef); }
  static inline auto Make() { return makeUniqueHimem<DOM>(defaultPools()); }

  static auto defaultPools() -> DOMPools &;

  Node *body{nullptr};

  auto addChild(Node *parentNode, Tag theTag) -> Node * {
    return nodeList.emplaceFront(parentNode, theTag);
  }

  auto show() -> void {
#if DEBUGGING
    std::cout << "DOM:" << std::endl;
    if (body != nullptr) body->show(1);
    std::cout << "[END DOM]" << std::endl;
#endif
  }

private:
  static auto bindPools(DOMPools &poolsRef) -> void;
  static auto unbindPools() -> void;
};
