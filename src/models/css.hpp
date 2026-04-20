// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

/**
 * CSS Class
 *
 * The content of this class is an internal representation of a css file once
 * it has been parsed using a CSSParser instance.
 *
 * It supplies the tools to search for a pattern matching of rules and the proper
 * sequencing of search through the specificity sorted list of rules.
 */

#pragma once

#include "global.hpp"
#include "himem.hpp"

#include <forward_list>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <map>

#include "dom.hpp"
#include "fonts.hpp"
#include "memory_pool.hpp"

/**
 * List of supported CSS properties
 *
 * This is the list of all css properties. The ones
 * in use are marked with a star.
 *
 *      background
 *      background-attachment
 *      background-color
 *      background-image
 *      background-position
 *      background-repeat
 *      border
 *      border-bottom
 *      border-bottom-color
 *      border-bottom-style
 *      border-bottom-width
 *      border-collapse
 *      border-color
 *      border-left
 *      border-left-color
 *      border-left-style
 *      border-left-width
 *      border-right
 *      border-right-color
 *      border-right-style
 *      border-right-width
 *      border-spacing
 *      border-style
 *      border-top
 *      border-top-color
 *      border-top-style
 *      border-top-width
 *      border-width
 *      bottom
 *      caption-side
 *      clear
 *      clip
 *      color
 *      content
 *      counter-increment
 *      counter-reset
 *      cursor
 *      direction
 *   *  display           not inherited  (inline)
 *      empty-cells
 *      float
 *      font
 *   *  font-family       yes              ?
 *   *  font-size         yes            (medium)
 *   *  font-style        yes            (normal)
 *      font-variant
 *   *  font-weight       yes            (normal)
 *   *  height            not inherited  (auto)
 *      left
 *      letter-spacing
 *   *  line-height       yes            (normal)
 *      list-style
 *      list-style-image
 *      list-style-position
 *      list-style-type
 *   *  margin            not inherited  (0 0 0 0)
 *   *  margin-bottom     not inherited  (0)
 *   *  margin-left       not inherited  (0)
 *   *  margin-right      not inherited  (0)
 *   *  margin-top        not inherited  (0)
 *      max-height
 *      max-width
 *      min-height
 *      min-width
 *      outline
 *      outline-color
 *      outline-style
 *      outline-width
 *      overflow
 *      padding
 *      padding-bottom
 *      padding-left
 *      padding-right
 *      padding-top
 *      page-break-after
 *      page-break-before
 *      page-break-inside
 *      position
 *      quotes
 *      right
 *   *  src
 *      table-layout
 *   *  text-align           yes           (left)
 *      text-decoration
 *   *  text-indent          yes           (0)
 *   *  text-transform       yes           (none)
 *      top
 *   *  vertical-align
 *      visibility
 *      white-space
 *   *  width                not inherited (auto)
 *      word-spacing
 *      z-index
 */

using CSSPtr = HimemUniquePtr<class CSS>;
class CSS {
private:
  std::string id;          // Unique identifier (filename) for this CSS instance
  std::string folderPath; // Path used for all other files access (relative)
  bool ghost;              // True if this instance rules content came from other instances
  uint8_t priority;

  CSS(const char *cssId, const char *fileFolderPath, const char *buffer, int32_t size,
      uint8_t prio);

  CSS(const char *cssId) {
    id          = cssId;
    folderPath = "";
    ghost       = true;
    priority    = 0;
  }

  CSS(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size, uint8_t prio);

public:
  ~CSS();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make(const char *cssId, const char *fileFolderPath, const char *buffer,
                          int32_t size, uint8_t prio) {
    return makeUniqueHimem<CSS>(cssId, fileFolderPath, buffer, size, prio);
  }
  static inline auto Make(const char *cssId) { return makeUniqueHimem<CSS>(cssId); }

  static inline auto Make(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size,
                          uint8_t prio) {
    return makeUniqueHimem<CSS>(cssId, tag, buffer, size, prio);
  }

  auto getId() const -> const std::string & { return id; }
  auto getFolderPath() const -> const std::string & { return folderPath; }
  auto getPriority() const -> uint8_t { return priority; }

  enum class ValueType : uint8_t {
    NO_TYPE, EM, EX, PERCENT, STR, PX, CM, MM, IN, PT, PC, VH, VW, REM, CH, VMIN, VMAX, DEG, RAD,
        GRAD, MSEC, SEC, HERTZ, KHERTZ, URL, INHERIT, DIMENSION
  };

  enum class Align : uint8_t {
    LEFT, CENTER, RIGHT, JUSTIFY
  };
  enum class VerticalAlign : uint8_t {
    NORMAL, SUB, SUPER, VALUE
  };
  enum class TextTransform : uint8_t {
    NONE, UPPERCASE, LOWERCASE, CAPITALIZE
  };
  enum class Display : uint8_t {
    NONE, INLINE, BLOCK, INLINE_BLOCK
  };
  enum class PropertyId : uint8_t {
    NOT_USED, FONT_FAMILY, FONT_SIZE, FONT_STYLE, FONT_WEIGHT, TEXT_ALIGN, TEXT_INDENT,
        TEXT_TRANSFORM, LINE_HEIGHT, SRC, MARGIN, MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP,
        MARGIN_BOTTOM, WIDTH, HEIGHT, DISPLAY, BORDER, VERTICAL_ALIGN
  };

  static const char *valueTypeStr[25];

  using PropertyMap = HimemMap<HimemString, PropertyId>;
  using FontSizeMap = HimemMap<HimemString, int16_t>;

  static PropertyMap propertyMap;
  static FontSizeMap fontSizeMap;

  // ---- Selector definition ----

  enum class SelOp : uint8_t {
    NONE, DESCENDANT, CHILD, ADJACENT
  };
  enum class Qualifier : uint8_t {
    NONE, FIRST_CHILD
  };

  using ClassList = std::forward_list<std::string>;

  #pragma pack(push, 1)
  // The following is OK in a little endian context.
  union Specificity {
    uint32_t value;
    struct {
      uint8_t tagCount, classCount, idCount, priority;
    } spec;
    auto show() -> void const {
      #if DEBUGGING
        std::cout << "[" << +spec.priority << "," << +spec.idCount << "," << +spec.classCount
                  << "," << +spec.tagCount << "](" << value << ") ";
      #endif
    }
  };

  struct SelectorNode {
    std::string id;
    ClassList classList;
    Qualifier qualifier;
    uint8_t classCount, idCount;
    SelOp op;
    DOM::Tag tag;
    SelectorNode() {
      op          = SelOp::NONE;
      tag         = DOM::Tag::NONE;
      qualifier   = Qualifier::NONE;
      classCount = 0;
      idCount    = 0;
    }
    ~SelectorNode() { classList.clear(); }
    auto addClass(std::string className) -> void {
      classList.push_front(className);
      classCount += 1;
    }
    auto addId(const std::string &theId) -> void {
      id = theId;
      idCount += 1;
    }
    auto setTag(DOM::Tag theTag) -> void { tag = theTag; }
    auto setQualifier(Qualifier q) -> void { qualifier = q; }
    auto show() -> void const {
      #if DEBUGGING
        if (op == SelOp::CHILD) std::cout << " > ";
        if (op == SelOp::ADJACENT) std::cout << " + ";
        if (op == SelOp::DESCENDANT) std::cout << " ";
        if (tag != DOM::Tag::NONE) {
          for (const auto &[key, value] : DOM::tags) {
            if (value == tag) {
              std::cout << key;
              break;
            }
          }
        }
        if (idCount > 0) std::cout << "#" << id;
        for (auto &cl : classList) std::cout << "." << cl;
        if (qualifier == Qualifier::FIRST_CHILD) std::cout << ":first_child";
      #endif
    }
  };

  using SelectorNodeList = std::forward_list<SelectorNode *>;

  struct Selector {
    Specificity specificity;
    SelectorNodeList selectorNodeList;
    Selector() { specificity.value = 0; }
    ~Selector() {
      for (auto *selectorNode : selectorNodeList) {
        selectorNodePool.deleteElement(selectorNode);
      }
      selectorNodeList.clear();
    }
    auto addSelectorNode(SelectorNode *node) -> void { selectorNodeList.push_front(node); }
    auto computeSpecificity(uint8_t prio) -> void {
      specificity.spec.priority = prio;
      for (auto *node : selectorNodeList) {
        specificity.spec.tagCount +=
            (((node->tag == DOM::Tag::NONE) || (node->tag == DOM::Tag::ANY)) ? 0 : 1);
        specificity.spec.classCount += node->classCount;
        specificity.spec.idCount += node->idCount;
      }
    }
    auto isEmpty() -> bool { return selectorNodeList.empty(); }
    auto showSelector(SelectorNodeList::const_iterator nodeIt, int8_t lev) -> void const {
      #if DEBUGGING
        if (nodeIt != selectorNodeList.end()) {
          SelectorNodeList::const_iterator nextNodeIt = nodeIt;
          showSelector(++nextNodeIt, lev + 1);
          (*nodeIt)->show();
        }
      #endif
    }
    auto show() -> void const {
      #if DEBUGGING
        specificity.show();
        showSelector(selectorNodeList.cbegin(), 0);
      #endif
    }
  };

  struct Value {
    float num;
    std::string str;
    ValueType valueType;
    union {
      Display display;
      Align align;
      TextTransform textTransform;
      Fonts::FaceStyle faceStyle;
      VerticalAlign verticalAlign;
    } choice;
    Value() {
      valueType = ValueType::NO_TYPE;
      num        = 0.0;
    }
    auto show() -> void {
      #if DEBUGGING
        if (valueType == ValueType::STR) {
          std::cout << str;
        } else if (valueType == ValueType::URL) {
          std::cout << "url(" << str << ')';
        } else {
          std::cout << num << valueTypeStr[static_cast<uint8_t>(valueType)];
        }
      #endif
    }
  };

  using Values = std::forward_list<Value *>;

  struct Property {
    PropertyId id;
    Values values;
    ~Property() {
      for (auto *value : values) {
        valuePool.deleteElement(value);
      }
      values.clear();
    }
    auto addValue(Value *v) -> void { values.push_front(v); }
    auto completed() -> void { values.reverse(); }
    auto show() -> void {
      #if DEBUGGING
        std::cout << "  ";
        for (const auto &[key, value] : propertyMap) {
          if (value == id) {
            std::cout << key;
            break;
          }
        }
        std::cout << ": ";
        bool first = true;
        for (auto *v : values) {
          if (!first) std::cout << ", ";
          v->show();
          first = false;
        }
        std::cout << ';' << std::endl;
      #endif
    }
  };
  #pragma pack(pop)

  // Sorted from the less specific to the most specific
  struct ruleCompare {
    auto operator()(const Selector *s1, const Selector *s2) const -> bool {
      return s1->specificity.value < s2->specificity.value;
    }
  };

  using Selectors  = std::list<Selector *>;
  using Properties = std::forward_list<Property *>;

  // using PropertySuite = std::list<Properties *>;
  using PropertySuiteList = std::forward_list<Properties *>;

  using RulesMap = std::multimap<Selector *, Properties *, ruleCompare>;

  RulesMap rulesMap;
  PropertySuiteList
      suites; // Linear list of suites to be deleted when the instance will be destroyed.

  static MemoryPool<Value> valuePool;
  static MemoryPool<Property> propertyPool;
  static MemoryPool<Properties> propertiesPool;
  static MemoryPool<SelectorNode> selectorNodePool;
  static MemoryPool<Selector> selectorPool;

  auto match(DOM::Node *node, RulesMap &toRules) -> void;
  auto show(RulesMap &theRulesMap) -> void;

  auto addRule(Selector *sel, Properties *props) -> void {
    rulesMap.insert(std::pair<Selector *, Properties *>(sel, props));
  }

  static auto getValuesFromRules(const RulesMap &rules, PropertyId id) -> const Values * {
    Values *vals = nullptr;
    for (auto &rule : rules) {
      for (auto *prop : *(rule.second)) {
        if (id == prop->id) vals = &prop->values;
      }
    }
    return vals;
  }

  auto getValuesFromProps(const Properties &props, PropertyId id) const -> const Values * {
    const Values *vals = nullptr;
    for (auto &prop : props) {
      if (id == prop->id) vals = &prop->values;
    }
    return vals;
  }

  auto retrieveDataFromCss(CSS &css) -> void {
    for (auto &rule : css.rulesMap) {
      rulesMap.insert(rule);
    }
  }

  auto show() -> void {
    #if DEBUGGING
      show(rulesMap);
    #endif
  }

private:
  auto matchSimpleSelector(DOM::Node &node, SelectorNode &simpleSel) -> bool;
  auto matchSelector(DOM::Node *node, Selector &sel) -> bool;
};
