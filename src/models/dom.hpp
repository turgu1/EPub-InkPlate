// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>

#include "memory_pool.hpp"

using DOMPtr = himemUniquePtr<class DOM>;
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
  enum class Tag : uint8_t {
    NONE, BODY, P, LI, BREAK, H1, H2, H3, H4, H5, H6, B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
        BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE, SUB, SUP
  };

  using Tags = std::map<std::string, Tag>;

  static Tags tags;

  class Node {
  public:
    using ClassList = std::forward_list<std::string>;
    using NodeList  = std::forward_list<Node *>;

    Node *father;
    Node *predecessor;
    NodeList children;
    ClassList classList;
    std::string id;
    Tag tag;
    bool firstChild;

    Node(Node *theFather, Tag theTag);
    ~Node();

    auto addClass(std::string theClass) -> Node *;
    auto addClasses(std::string theClasses) -> Node *;
    auto addId(const std::string &theId) -> Node *;
    void showChildren(NodeList::const_iterator nodeIt, int8_t lev) const;
    void show(uint8_t level) const;
  };

private:
  MemoryPool<Node> nodePool;
  DOM() = default;

public:
  ~DOM() = default; // { nodePool.deleteElement(body); }

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make() { return makeUniqueHimem<DOM>(); }

  Node *body{nodePool.newElement(nullptr, Tag::BODY)};

  auto addChild(Node *parentNode, Tag theTag) -> Node * {
    return nodePool.newElement(parentNode, theTag);
  }

  void show() {
    #if DEBUGGING
      std::cout << "DOM:" << std::endl;
      if (body != nullptr) body->show(1);
      std::cout << "[END DOM]" << std::endl;
    #endif
  }
};
