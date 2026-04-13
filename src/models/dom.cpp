// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/dom.hpp"

// clang-format off
DOM::Tags DOM::tags = 
  {{"p",           Tag::P}, {"div",               Tag::DIV}, {"span", Tag::SPAN}, {"br",  Tag::BREAK}, {"h1",   Tag::H1},  
   {"h2",         Tag::H2}, {"h3",                 Tag::H3}, {"h4",     Tag::H4}, {"h5",     Tag::H5}, {"h6",   Tag::H6}, 
   {"b",           Tag::B}, {"i",                   Tag::I}, {"em",     Tag::EM}, {"body", Tag::BODY}, {"a",     Tag::A},
   {"img",       Tag::IMG}, {"image",           Tag::IMAGE}, {"li",     Tag::LI}, {"pre",   Tag::PRE}, {"sub", Tag::SUB},
   {"strong", Tag::STRONG}, {"blockquote", Tag::BLOCKQUOTE}, {"sup",   Tag::SUP}, {"none", Tag::NONE}, {"*",   Tag::ANY}, 
   {"@page",    Tag::PAGE}, {"@font-face",  Tag::FONT_FACE},
  };
// clang-format on

DOM::Node::Node(Node *the_father, Tag the_tag) {
  father = the_father;
  tag    = the_tag;
  if (father != nullptr) {
    first_child = father->children.empty();
    predecessor = first_child ? nullptr : father->children.front();
    father->children.push_front(this);
  } else
    first_child = true;
};

DOM::Node::~Node() { children.clear(); }

auto DOM::Node::add_class(std::string the_class) -> Node * {
  class_list.push_front(the_class);
  return this;
}

auto DOM::Node::add_classes(std::string the_classes) -> Node * {
  std::istringstream iss(the_classes);
  std::string item;
  while (std::getline(iss, item, ' ')) {
    if (!item.empty()) class_list.push_front(item);
  }
  return this;
}

auto DOM::Node::add_id(const std::string &the_id) -> Node * {
  id = the_id;
  return this;
}

void DOM::Node::show_children(NodeList::const_iterator node_it, int8_t lev) const {
  #if DEBUGGING
    if (node_it != children.end()) {
      NodeList::const_iterator next_node_it = node_it;
      show_children(++next_node_it, lev);
      (*node_it)->show(lev);
    }
  #endif
}

void DOM::Node::show(uint8_t level) const {
  #if DEBUGGING
    std::cout << std::string(level * 2, ' ');
    for (auto &t : tags) {
      if (t.second == tag) {
        std::cout << t.first;
        break;
      }
    }
    std::cout << " ";
    if (!id.empty()) std::cout << "#" << id;
    for (auto &c : class_list) std::cout << '.' << c;
    if (first_child) std::cout << ":first_child";
    std::cout << std::endl;

    show_children(children.cbegin(), level + 1);
  #endif
}