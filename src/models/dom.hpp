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

using DOMPtr = himem_unique_ptr<class DOM>;
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
    ClassList class_list;
    std::string id;
    Tag tag;
    bool first_child;

    Node(Node *the_father, Tag the_tag);
    ~Node();

    auto add_class(std::string the_class) -> Node *;
    auto add_classes(std::string the_classes) -> Node *;
    auto add_id(const std::string &the_id) -> Node *;
    void show_children(NodeList::const_iterator node_it, int8_t lev) const;
    void show(uint8_t level) const;
  };

private:
  MemoryPool<Node> node_pool;
  DOM() = default;

public:
  ~DOM() = default; // { node_pool.deleteElement(body); }

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make() { return make_unique_himem<DOM>(); }

  Node *body{node_pool.newElement(nullptr, Tag::BODY)};

  auto add_child(Node *parent_node, Tag the_tag) -> Node * {
    return node_pool.newElement(parent_node, the_tag);
  }

  void show() {
    #if DEBUGGING
      std::cout << "DOM:" << std::endl;
      if (body != nullptr) body->show(1);
      std::cout << "[END DOM]" << std::endl;
    #endif
  }
};
