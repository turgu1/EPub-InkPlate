// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/dom.hpp"

#include <cstring>

DOM::Tags DOM::tags = {
  { "p",      Tag::P      },  { "div",        Tag::DIV        },  { "span",     Tag::SPAN     },  { "br",      Tag::BREAK   },
  { "h1",     Tag::H1     },  { "h2",         Tag::H2         },  { "h3",       Tag::H3       },  { "h4",      Tag::H4      },
  { "h5",     Tag::H5     },  { "h6",         Tag::H6         },  { "b",        Tag::B        },  { "i",       Tag::I       },
  { "em",     Tag::EM     },  { "body",       Tag::BODY       },  { "a",        Tag::A        },  { "img",     Tag::IMG     },
  { "image",  Tag::IMAGE  },  { "li",         Tag::LI         },  { "pre",      Tag::PRE      },  { "sub",     Tag::SUB     },
  { "strong", Tag::STRONG },  { "blockquote", Tag::BLOCKQUOTE },  { "sup",      Tag::SUP      },  { "none",    Tag::NONE    },
  { "*",      Tag::ANY    },  { "table",      Tag::TABLE      },  { "colgroup", Tag::COLGROUP },  { "tr",      Tag::TR      },
  { "td",     Tag::TD     },  { "th",         Tag::TH         },  { "tfoot",    Tag::TFOOT    },  { "thead",   Tag::THEAD   },
  { "col",    Tag::COL    },  { "caption",    Tag::CAPTION    },  { "tbody",    Tag::TBODY    },  { "@page",   Tag::PAGE    },
  { "@font-face", Tag::FONT_FACE  },
};

auto DOM::defaultPools() -> DOMPools & {
  static DOMPools pools;
  return pools;
}

auto DOM::bindPools(DOMPools &poolsRef) -> void {
  using Tag                                                           = ScopedListPoolTag;
  ScopedHimemPoolBinding<DOMPools::NodeClassListNode, Tag, 256>::pool = &poolsRef.nodeClassListPool;
  ScopedHimemPoolBinding<DOMPools::NodeNodeListNode, Tag, 512>::pool  = &poolsRef.nodeNodeListPool;
  ScopedHimemPoolBinding<DOMPools::DomNodeListNode, Tag, 100>::pool   = &poolsRef.domNodeListPool;
}

auto DOM::unbindPools() -> void {
  using Tag                                                           = ScopedListPoolTag;
  ScopedHimemPoolBinding<DOMPools::NodeClassListNode, Tag, 256>::pool = nullptr;
  ScopedHimemPoolBinding<DOMPools::NodeNodeListNode, Tag, 512>::pool  = nullptr;
  ScopedHimemPoolBinding<DOMPools::DomNodeListNode, Tag, 100>::pool   = nullptr;
}

DOM::PoolsGuard::PoolsGuard(DOMPools &poolsRef) : poolsRef(poolsRef) { bindPools(poolsRef); }

DOM::PoolsGuard::~PoolsGuard() { unbindPools(); }

DOM::DOM(DOMPools &poolsRef) : pools(&poolsRef), guard(new PoolsGuard(poolsRef)), nodeList{} {
  body = nodeList.emplaceFront(nullptr, Tag::BODY);
}

DOM::~DOM() {
  nodeList.clear();
  delete guard;
  guard = nullptr;
}

DOM::Node::Node(Node *theFather, Tag theTag) {
  father = theFather;
  tag    = theTag;
  if (father != nullptr) {
    firstChild  = father->children.empty();
    predecessor = firstChild ? nullptr : father->children.front();
    father->children.pushFront(this);
  } else {
    firstChild = true;
  }
};

DOM::Node::~Node() { children.clear(); }

auto DOM::Node::addClass(const char *theClass) -> Node * {
  if ((theClass == nullptr) || (*theClass == '\0')) { return this; }
  classList.pushFront(HimemString(theClass));
  return this;
}

auto DOM::Node::addClasses(const char *theClasses) -> Node * {
  if (theClasses == nullptr) { return this; }

  const char *p = theClasses;
  while (*p != '\0') {
    while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r')) ++p;
    if (*p == '\0') { break; }

    const char *start = p;
    while ((*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != '\n') && (*p != '\r')) ++p;

    const size_t len = static_cast<size_t>(p - start);
    if (len > 0) {
      HimemString cls;
      cls.assign(start, len);
      classList.pushFront(std::move(cls));
    }
  }
  return this;
}

auto DOM::Node::addId(const char *theId) -> Node * {
  if (theId == nullptr) { return this; }
  id = theId;
  return this;
}

auto DOM::Node::showChildren(NodeList::ConstIterator nodeIt, int8_t lev) -> void const {
  #if DEBUGGING
    if (nodeIt != children.end()) {
      NodeList::ConstIterator nextNodeIt = nodeIt;
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
    if (!id.empty()) { std::cout << "#" << id; }
    for (auto &c : classList) std::cout << '.' << c;
    if (firstChild) { std::cout << ":first_child"; }
    std::cout << std::endl;

    showChildren(children.cbegin(), level + 1);
  #endif
}