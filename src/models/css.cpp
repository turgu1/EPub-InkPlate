// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/css.hpp"
#include "models/css_parser.hpp"

// clang-format off
CSS::PropertyMap CSS::propertyMap = {
  {"not-used",       CSS::PropertyId::NOT_USED},
  {"font-family",    CSS::PropertyId::FONT_FAMILY},
  {"font-size",      CSS::PropertyId::FONT_SIZE},
  {"font-style",     CSS::PropertyId::FONT_STYLE},
  {"font-weight",    CSS::PropertyId::FONT_WEIGHT},
  {"text-align",     CSS::PropertyId::TEXT_ALIGN},
  {"text-indent",    CSS::PropertyId::TEXT_INDENT},
  {"text-transform", CSS::PropertyId::TEXT_TRANSFORM},
  {"line-height",    CSS::PropertyId::LINE_HEIGHT},
  {"src",            CSS::PropertyId::SRC},
  {"margin",         CSS::PropertyId::MARGIN},
  {"margin-top",     CSS::PropertyId::MARGIN_TOP},
  {"margin-bottom",  CSS::PropertyId::MARGIN_BOTTOM},
  {"margin-left",    CSS::PropertyId::MARGIN_LEFT},
  {"margin-right",   CSS::PropertyId::MARGIN_RIGHT},
  {"width",          CSS::PropertyId::WIDTH},
  {"height",         CSS::PropertyId::HEIGHT},
  {"display",        CSS::PropertyId::DISPLAY},
  {"border",         CSS::PropertyId::BORDER},
  {"vertical-align", CSS::PropertyId::VERTICAL_ALIGN}};

CSS::FontSizeMap CSS::fontSizeMap = {
  {"xx-small",  6}, 
  {"x-small",   7},  
  {"smaller",   9},
  {"small",    10},   
  {"medium",   12},  
  {"large",    14},
  {"larger",   15},  
  {"x-large",  18}, 
  {"xx-large", 24}};

// clang-format on

const char *CSS::valueTypeStr[25] = {
    "",    "em", "ex",   "%",    "",    "px",  "cm",   "mm",   "in",  "pt", "pc",  "vh", "vw",
    "rem", "ch", "vmin", "vmax", "deg", "rad", "grad", "msec", "sec", "hz", "khz", "url"};

auto CSS::defaultPools() -> CSSPools & {
  static CSSPools pools;
  return pools;
}

auto CSS::bindPools(CSSPools &poolsRef) -> void {
  using Tag                                                       = ScopedListPoolTag;
  ScopedHimemPoolBinding<CSSPools::ClassListNode, Tag, 256>::pool = &poolsRef.classListPool;
  ScopedHimemPoolBinding<CSSPools::SelectorNodeListNode, Tag, 512>::pool =
      &poolsRef.selectorNodeListPool;
  ScopedHimemPoolBinding<CSSPools::ValuesNode, Tag, 512>::pool     = &poolsRef.valuesPool;
  ScopedHimemPoolBinding<CSSPools::SelectorsNode, Tag, 256>::pool  = &poolsRef.selectorsPool;
  ScopedHimemPoolBinding<CSSPools::PropertiesNode, Tag, 256>::pool = &poolsRef.propertiesPool;
  ScopedHimemPoolBinding<CSSPools::PropertySuiteListNode, Tag, 128>::pool =
      &poolsRef.propertySuiteListPool;
  ScopedHimemPoolBinding<CSSPools::SelectorSuiteListNode, Tag, 128>::pool =
      &poolsRef.selectorSuiteListPool;
  ScopedHimemPoolBinding<CSSPools::SelectorSingleListNode, Tag, 128>::pool =
      &poolsRef.selectorSingleListPool;
}

auto CSS::unbindPools() -> void {
  using Tag                                                                = ScopedListPoolTag;
  ScopedHimemPoolBinding<CSSPools::ClassListNode, Tag, 256>::pool          = nullptr;
  ScopedHimemPoolBinding<CSSPools::SelectorNodeListNode, Tag, 512>::pool   = nullptr;
  ScopedHimemPoolBinding<CSSPools::ValuesNode, Tag, 512>::pool             = nullptr;
  ScopedHimemPoolBinding<CSSPools::SelectorsNode, Tag, 256>::pool          = nullptr;
  ScopedHimemPoolBinding<CSSPools::PropertiesNode, Tag, 256>::pool         = nullptr;
  ScopedHimemPoolBinding<CSSPools::PropertySuiteListNode, Tag, 128>::pool  = nullptr;
  ScopedHimemPoolBinding<CSSPools::SelectorSuiteListNode, Tag, 128>::pool  = nullptr;
  ScopedHimemPoolBinding<CSSPools::SelectorSingleListNode, Tag, 128>::pool = nullptr;
}

CSS::PoolsGuard::PoolsGuard(CSSPools &poolsRef) : poolsRef(poolsRef) { bindPools(poolsRef); }

CSS::PoolsGuard::~PoolsGuard() { unbindPools(); }

CSS::CSS(const char *cssId, const char *fileFolderPath, const char *buffer, int32_t size,
         uint8_t prio, CSSPools &poolsRef)
    : pools(&poolsRef), guard(new PoolsGuard(poolsRef)) {

  id         = cssId;
  folderPath = fileFolderPath;
  ghost      = false;
  priority   = prio;

  CSSParser parser(*this, buffer, size);
}

CSS::CSS(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size, uint8_t prio,
         CSSPools &poolsRef)
    : pools(&poolsRef), guard(new PoolsGuard(poolsRef)) {

  id         = cssId;
  folderPath = "";
  ghost      = false;
  priority   = prio;

  CSSParser parser(*this, tag, buffer, size);
}

CSS::CSS(const char *cssId, CSSPools &poolsRef)
    : pools(&poolsRef), guard(new PoolsGuard(poolsRef)) {

  id         = cssId;
  folderPath = "";
  ghost      = true;
  priority   = 0;
}

CSS::~CSS() {
  rulesMap.clear();
  selectorSuites.clear();
  selectorSingles.clear();
  propertySuites.clear();

  delete guard;
  guard = nullptr;
}

auto CSS::matchSimpleSelector(DOM::Node &node, SelectorNode &simpleSel) -> bool {
  if (simpleSel.classCount > 0) {
    for (auto &selClass : simpleSel.classList) {
      bool found = false;
      for (auto &nodeClass : node.classList) {
        if (selClass.compare(nodeClass.c_str()) == 0) {
          found = true;
          break;
        }
      }
      if (!found) return false;
    }
  }
  if ((simpleSel.tag != DOM::Tag::NONE) && (simpleSel.tag != DOM::Tag::ANY) &&
      (simpleSel.tag != node.tag))
    return false;
  if ((simpleSel.idCount > 0) && (simpleSel.id.compare(node.id.c_str()) != 0)) return false;
  if ((simpleSel.qualifier == Qualifier::FIRST_CHILD) && !node.firstChild) return false;
  return true;
}

auto CSS::matchSelector(DOM::Node *node, Selector &sel) -> bool {
  SelOp op           = SelOp::NONE;
  DOM::Node *theNode = node;
  // The selectorNodeList is already in a reverse order, so we process selectors from right to
  // left
  for (auto &selNode : sel.selectorNodeList) {
    switch (op) {
    case SelOp::NONE:
      if (!matchSimpleSelector(*theNode, selNode)) return false;
      break;
    case SelOp::ADJACENT:
      theNode = theNode->predecessor;
      if ((theNode == nullptr) || !matchSimpleSelector(*theNode, selNode)) return false;
      break;
    case SelOp::CHILD:
      theNode = theNode->father;
      if ((theNode == nullptr) || !matchSimpleSelector(*theNode, selNode)) return false;
      break;
    case SelOp::DESCENDANT:
      theNode = theNode->father;
      while (theNode != nullptr) {
        if (matchSimpleSelector(*theNode, selNode)) break;
        theNode = theNode->father;
      }
      if (theNode == nullptr) return false;
      break;
    }
    op = selNode.op;
  }
  return true;
}

auto CSS::match(DOM::Node *node, RulesMap &toRules) -> void {
  for (auto &rule : rulesMap) {
    if (matchSelector(node, *rule.first)) {
      toRules.insert(std::pair<Selector *, Properties *>(rule.first, rule.second));
    }
  }
}

auto CSS::show(RulesMap &theRulesMap) -> void {
  #if DEBUGGING
    std::cout << "------ Rules Map: -----" << std::endl;
    for (auto &rule : theRulesMap) {
      rule.first->show();
      std::cout << " {" << std::endl;
      for (auto &prop : *rule.second) {
        prop.show();
      }
      std::cout << "}" << std::endl;
    }
    std::cout << "[END]" << std::endl;
  #endif
}