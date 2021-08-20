#include "models/css.hpp"
#include "models/css_parser.hpp"

MemoryPool<CSS::Value>        CSS::value_pool;
MemoryPool<CSS::Property>     CSS::property_pool;
MemoryPool<CSS::Properties>   CSS::properties_pool;
MemoryPool<CSS::SelectorNode> CSS::selector_node_pool;
MemoryPool<CSS::Selector>     CSS::selector_pool;

CSS::PropertyMap CSS::property_map = {
  { "not-used",       CSS::PropertyId::NOT_USED       },
  { "font-family",    CSS::PropertyId::FONT_FAMILY    }, 
  { "font-size",      CSS::PropertyId::FONT_SIZE      }, 
  { "font-style",     CSS::PropertyId::FONT_STYLE     },
  { "font-weight",    CSS::PropertyId::FONT_WEIGHT    },
  { "text-align",     CSS::PropertyId::TEXT_ALIGN     },
  { "text-indent",    CSS::PropertyId::TEXT_INDENT    },
  { "text-transform", CSS::PropertyId::TEXT_TRANSFORM },
  { "line-height",    CSS::PropertyId::LINE_HEIGHT    },
  { "src",            CSS::PropertyId::SRC            },
  { "margin",         CSS::PropertyId::MARGIN         },
  { "margin-top",     CSS::PropertyId::MARGIN_TOP     },
  { "margin-bottom",  CSS::PropertyId::MARGIN_BOTTOM  },
  { "margin-left",    CSS::PropertyId::MARGIN_LEFT    },
  { "margin-right",   CSS::PropertyId::MARGIN_RIGHT   },
  { "width",          CSS::PropertyId::WIDTH          },
  { "height",         CSS::PropertyId::HEIGHT         },
  { "display",        CSS::PropertyId::DISPLAY        },
  { "border",         CSS::PropertyId::BORDER         },
  { "vertical-align", CSS::PropertyId::VERTICAL_ALIGN }
};

CSS::FontSizeMap CSS::font_size_map = {
  { "xx-small",  6 },
  { "x-small",   7 },
  { "smaller",   9 },
  { "small",    10 },
  { "medium",   12 },
  { "large",    14 },
  { "larger",   15 },
  { "x-large",  18 },
  { "xx-large", 24 }
};

const char * CSS::value_type_str[25] = {
  "",     "em",  "ex", "%",   "",   "px",   "cm",   "mm",  "in",  "pt",
  "pc",   "vh",  "vw", "rem", "ch", "vmin", "vmax", "deg", "rad", "grad",
  "msec", "sec", "hz", "khz", "url"
};

CSS::CSS(const std::string & css_id,
         const std::string & file_folder_path, 
         const char *        buffer, 
         int32_t             size,
         uint8_t             prio)  
  : id(css_id), folder_path(file_folder_path), ghost(false), priority(prio)
{
  CSSParser * parser = new CSSParser(*this, buffer, size);
  delete parser;
}

CSS::CSS(const std::string & css_id,
         DOM::Tag            tag,
         const char *        buffer, 
         int32_t             size,
         uint8_t             prio)
    : id(css_id), folder_path(""), ghost(false), priority(prio)
{
  CSSParser * parser = new CSSParser(*this, tag, buffer, size);
  delete parser;
}

CSS::~CSS()
{
  if (ghost) {
    rules_map.clear();
  }
  else {
    for (auto * props : suites) {
      for (auto * prop : *props) {
        property_pool.deleteElement(prop);
      }
      properties_pool.deleteElement(props);
    }
    suites.clear();

    for (auto & rule : rules_map) {
      selector_pool.deleteElement(rule.first);
    }
    rules_map.clear();
  }
}

bool 
CSS::match_simple_selector(DOM::Node & node, SelectorNode & simple_sel) 
{
  if (simple_sel.class_count > 0) {
    for (auto & sel_class : simple_sel.class_list) {
      bool found = false;
      for (auto & node_class : node.class_list) {
        if (sel_class.compare(node_class) == 0) {
          found = true;
          break;
        }
      }
      if (!found) return false;
    }
  }
  if ((simple_sel.tag != DOM::Tag::NONE) && (simple_sel.tag != DOM::Tag::ANY) && (simple_sel.tag != node.tag)) return false;
  if ((simple_sel.id_count > 0) && (simple_sel.id.compare(node.id) != 0)) return false;
  if ((simple_sel.qualifier == Qualifier::FIRST_CHILD) && !node.first_child) return false;
  return true;
}

bool 
CSS::match_selector(DOM::Node * node, Selector & sel) 
{
  SelOp op = SelOp::NONE;
  DOM::Node * the_node = node;
  // The selector_node_list is already in a reverse order, so we process selectors from right to left
  for (auto * sel_node : sel.selector_node_list) {
    switch (op) {
      case SelOp::NONE:
        if (!match_simple_selector(*the_node, *sel_node)) return false;
        break;
      case SelOp::ADJACENT:
        the_node = the_node->predecessor;
        if ((the_node == nullptr) || !match_simple_selector(*the_node, *sel_node)) return false;
        break;
      case SelOp::CHILD:
        the_node = the_node->father;
        if ((the_node == nullptr) || !match_simple_selector(*the_node, *sel_node)) return false;
        break;
      case SelOp::DESCENDANT:
        the_node = the_node->father;
        while (the_node != nullptr) {
          if (match_simple_selector(*the_node, *sel_node)) break;
          the_node = the_node->father;
        }
        if (the_node == nullptr) return false;
        break;
    }
    op = sel_node->op;
  }
  return true;
}

void 
CSS::match(DOM::Node * node, RulesMap & to_rules) 
{
  for (auto & rule : rules_map) {
    if (match_selector(node, *rule.first)) {
      to_rules.insert(std::pair<Selector *, Properties *>(rule.first, rule.second));
    }
  }
}

void
CSS::show(RulesMap & the_rules_map) 
{
  #if DEBUGGING
    std::cout << "------ Rules Map: -----" << std::endl;
    for (auto & rule : the_rules_map) {
      rule.first->show();
      std::cout << " {" << std::endl;
      for (auto * prop : *rule.second) {
        prop->show();
      }
      std::cout << "}" << std::endl;
    }
    std::cout << "[END]" << std::endl;
  #endif
}