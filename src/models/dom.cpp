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

DOM::Node::Node(Node *theFather, Tag theTag) {
  father = theFather;
  tag    = theTag;
  if (father != nullptr) {
    firstChild  = father->children.empty();
    predecessor = firstChild ? nullptr : father->children.front();
    father->children.push_front(this);
  } else
    firstChild = true;
};

DOM::Node::~Node() { children.clear(); }

auto DOM::Node::addClass(std::string theClass) -> Node * {
  classList.push_front(theClass);
  return this;
}

auto DOM::Node::addClasses(std::string theClasses) -> Node * {
  std::istringstream iss(theClasses);
  std::string item;
  while (std::getline(iss, item, ' ')) {
    if (!item.empty()) classList.push_front(item);
  }
  return this;
}

auto DOM::Node::addId(const std::string &theId) -> Node * {
  id = theId;
  return this;
}

auto DOM::Node::showChildren(NodeList::const_iterator nodeIt, int8_t lev) -> void const {
  #if DEBUGGING
    if (nodeIt != children.end()) {
      NodeList::const_iterator nextNodeIt = nodeIt;
      showChildren(++nextNodeIt, lev);
      (*nodeIt)->show(lev);
    }
  #endif
}

auto DOM::Node::show(uint8_t level) -> void const {
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
    for (auto &c : classList) std::cout << '.' << c;
    if (firstChild) std::cout << ":first_child";
    std::cout << std::endl;

    showChildren(children.cbegin(), level + 1);
  #endif
}