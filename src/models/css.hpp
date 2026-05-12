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
#include "simple_list.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

#include "dom.hpp"
#include "fonts.hpp"
#include "himem_pool.hpp"

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
  // Tag used to isolate CSS allocator bindings from other subsystems.
  struct ScopedListPoolTag;

public:
  class CSSPools;
  class PoolsGuard;

private:
  /**
   * @brief A SimpleList backed by a per-instance bound PSRAM pool.
   *
   * The allocator no longer uses a process-global static pool. Instead, it
   * captures thread-local bindings activated by a CSS-owned PoolsGuard so all
   * allocations are scoped to the EPub/CSS pool set currently in use.
   */
  template <typename T, std::size_t BlockSize>
  using SimpleListSinglePool =
      SimpleList<T, ScopedHimemPoolAllocator<T, ScopedListPoolTag, BlockSize>>;

  CSSPools *pools;
  PoolsGuard *guard;

  HimemString id;         // Unique identifier (filename) for this CSS instance
  HimemString folderPath; // Path used for all other files access (relative)
  bool ghost;             // True if this instance rules content came from other instances
  uint8_t priority;

  CSS(const char *cssId, const char *fileFolderPath, const char *buffer, int32_t size, uint8_t prio,
      CSSPools &poolsRef);

  CSS(const char *cssId, CSSPools &poolsRef);

  CSS(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size, uint8_t prio,
      CSSPools &poolsRef);

public:
  ~CSS();

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make(const char *cssId, const char *fileFolderPath, const char *buffer,
                          int32_t size, uint8_t prio, CSSPools &poolsRef) {
    return makeUniqueHimem<CSS>(cssId, fileFolderPath, buffer, size, prio, poolsRef);
  }
  static inline auto Make(const char *cssId, CSSPools &poolsRef) {
    return makeUniqueHimem<CSS>(cssId, poolsRef);
  }

  static inline auto Make(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size,
                          uint8_t prio, CSSPools &poolsRef) {
    return makeUniqueHimem<CSS>(cssId, tag, buffer, size, prio, poolsRef);
  }

  // Compatibility entry points for non-EPub call sites.
  static inline auto Make(const char *cssId, const char *fileFolderPath, const char *buffer,
                          int32_t size, uint8_t prio) {
    return Make(cssId, fileFolderPath, buffer, size, prio, defaultPools());
  }
  static inline auto Make(const char *cssId) { return Make(cssId, defaultPools()); }
  static inline auto Make(const char *cssId, DOM::Tag tag, const char *buffer, int32_t size,
                          uint8_t prio) {
    return Make(cssId, tag, buffer, size, prio, defaultPools());
  }

  auto getId() const -> const HimemString & { return id; }
  auto getFolderPath() const -> const HimemString & { return folderPath; }
  auto getPriority() const -> uint8_t { return priority; }
  auto getPools() -> CSSPools & { return *pools; }

  static auto defaultPools() -> CSSPools &;

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

  using ClassList = SimpleListSinglePool<HimemString, 256>;

  #pragma pack(push, 1)
  // The following is OK in a little endian context.
  union Specificity {
    uint32_t value;
    struct {
      uint8_t tagCount, classCount, idCount, priority;
    } spec;
    auto show() -> void const {
      #if DEBUGGING
        std::cout << "[" << +spec.priority << "," << +spec.idCount << "," << +spec.classCount << ","
                  << +spec.tagCount << "](" << value << ") ";
      #endif
    }
  };
  #pragma pack(pop)

  struct SelectorNode {
    HimemString id;
    ClassList classList;
    Qualifier qualifier;
    uint8_t classCount, idCount;
    SelOp op;
    DOM::Tag tag;
    SelectorNode() {
      op         = SelOp::NONE;
      tag        = DOM::Tag::NONE;
      qualifier  = Qualifier::NONE;
      classCount = 0;
      idCount    = 0;
    }
    ~SelectorNode() { classList.clear(); }

    SelectorNode(SelectorNode &&) noexcept            = default;
    SelectorNode &operator=(SelectorNode &&) noexcept = default;
    SelectorNode(const SelectorNode &)                = default;
    SelectorNode &operator=(const SelectorNode &)     = default;

    auto addClass(HimemString className) -> void {
      classList.pushBack(std::move(className));
      classCount += 1;
    }
    auto addId(const HimemString &theId) -> void {
      id = theId;
      idCount += 1;
    }
    auto setTag(DOM::Tag theTag) -> void { tag = theTag; }
    auto setQualifier(Qualifier q) -> void { qualifier = q; }
    auto show() const -> void const {
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

  using SelectorNodeList = SimpleListSinglePool<SelectorNode, 512>;

  struct Selector {
    Specificity specificity;
    SelectorNodeList selectorNodeList;
    Selector() { specificity.value = 0; }

    Selector(Selector &&) noexcept            = default;
    Selector &operator=(Selector &&) noexcept = default;
    Selector(const Selector &)                = default;
    Selector &operator=(const Selector &)     = default;

    ~Selector() {
      // for (auto *selectorNode : selectorNodeList) {
      //   selectorNodePool.deleteElement(selectorNode);
      // }
      selectorNodeList.clear();
    }
    auto addSelectorNode(SelectorNode node) -> bool {
      size_t oldSize = selectorNodeList.size();
      selectorNodeList.pushFront(std::move(node));
      return selectorNodeList.size() == oldSize + 1;
    }
    auto computeSpecificity(uint8_t prio) -> void {
      specificity.spec.priority = prio;
      for (auto &node : selectorNodeList) {
        specificity.spec.tagCount +=
            (((node.tag == DOM::Tag::NONE) || (node.tag == DOM::Tag::ANY)) ? 0 : 1);
        specificity.spec.classCount += node.classCount;
        specificity.spec.idCount += node.idCount;
      }
    }
    auto isEmpty() -> bool { return selectorNodeList.empty(); }
    auto showSelector(SelectorNodeList::ConstIterator nodeIt, int8_t lev) -> void const {
      #if DEBUGGING
        if (nodeIt != selectorNodeList.end()) {
          SelectorNodeList::ConstIterator nextNodeIt = nodeIt;
          showSelector(++nextNodeIt, lev + 1);
          nodeIt->show();
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
    HimemString str;
    ValueType valueType;
    union {
      Display display;
      Align align;
      TextTransform textTransform;
      FaceStyle faceStyle;
      VerticalAlign verticalAlign;
    } choice;
    Value() {
      valueType = ValueType::NO_TYPE;
      num       = 0.0;
    }

    Value(Value &&) noexcept            = default;
    Value &operator=(Value &&) noexcept = default;
    Value(const Value &)                = default;
    Value &operator=(const Value &)     = default;

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

  using Values = SimpleListSinglePool<Value, 512>;

  struct Property {
    PropertyId id;
    Values values;

    Property() = default;

    Property(Property &&) noexcept            = default;
    Property &operator=(Property &&) noexcept = default;
    Property(const Property &)                = default;
    Property &operator=(const Property &)     = default;

    ~Property() {
      // for (auto *value : values) {
      //   valuePool.deleteElement(value);
      // }
      values.clear();
    }
    auto addValue(Value v) -> bool {
      size_t oldSize = values.size();
      values.pushBack(std::move(v));
      return values.size() == oldSize + 1;
    }
    auto completed() -> void {}
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
        for (auto &v : values) {
          if (!first) std::cout << ", ";
          v.show();
          first = false;
        }
        std::cout << ';' << std::endl;
      #endif
    }
  };

  // Sorted from the less specific to the most specific
  struct ruleCompare {
    auto operator()(const Selector *s1, const Selector *s2) const -> bool {
      return s1->specificity.value < s2->specificity.value;
    }
  };

  using Selectors  = SimpleListSinglePool<Selector, 256>;
  using Properties = SimpleListSinglePool<Property, 256>;

  // using PropertySuite = std::list<Properties *>;
  using PropertySuiteList  = SimpleListSinglePool<Properties, 128>;
  using SelectorSuiteList  = SimpleListSinglePool<Selectors, 128>;
  using SelectorSingleList = SimpleListSinglePool<Selector, 128>;

  using RulesMap = std::multimap<Selector *, Properties *, ruleCompare>;

  RulesMap rulesMap;

  // Owning lists — element destructors fire automatically on clear() / destruction.
  PropertySuiteList propertySuites;
  SelectorSuiteList selectorSuites;
  SelectorSingleList selectorSingles;

  class CSSPools {
  public:
    using ClassListNode          = ClassList::Node;
    using SelectorNodeListNode   = SelectorNodeList::Node;
    using ValuesNode             = Values::Node;
    using SelectorsNode          = Selectors::Node;
    using PropertiesNode         = Properties::Node;
    using PropertySuiteListNode  = PropertySuiteList::Node;
    using SelectorSuiteListNode  = SelectorSuiteList::Node;
    using SelectorSingleListNode = SelectorSingleList::Node;

    HimemPool<ClassListNode> classListPool{256};
    HimemPool<SelectorNodeListNode> selectorNodeListPool{512};
    HimemPool<ValuesNode> valuesPool{512};
    HimemPool<SelectorsNode> selectorsPool{256};
    HimemPool<PropertiesNode> propertiesPool{256};
    HimemPool<PropertySuiteListNode> propertySuiteListPool{128};
    HimemPool<SelectorSuiteListNode> selectorSuiteListPool{128};
    HimemPool<SelectorSingleListNode> selectorSingleListPool{128};
  };

  class PoolsGuard {
  public:
    explicit PoolsGuard(CSSPools &poolsRef);
    ~PoolsGuard();

  private:
    CSSPools &poolsRef;
  };

  auto match(DOM::Node *node, RulesMap &toRules) -> void;
  auto show(RulesMap &theRulesMap) -> void;

  auto addRule(Selector *sel, Properties *props) -> void {
    rulesMap.insert(std::pair<Selector *, Properties *>(sel, props));
  }

  static auto getValuesFromRules(const RulesMap &rules, PropertyId id) -> const Values * {
    Values *vals = nullptr;
    for (auto &rule : rules) {
      for (auto &prop : *(rule.second)) {
        if (id == prop.id) vals = &prop.values;
      }
    }
    return vals;
  }

  auto getValuesFromProps(const Properties &props, PropertyId id) const -> const Values * {
    const Values *vals = nullptr;
    for (auto &prop : props) {
      if (id == prop.id) vals = &prop.values;
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
  static auto bindPools(CSSPools &poolsRef) -> void;
  static auto unbindPools() -> void;

  auto matchSimpleSelector(DOM::Node &node, SelectorNode &simpleSel) -> bool;
  auto matchSelector(DOM::Node *node, Selector &sel) -> bool;
};
