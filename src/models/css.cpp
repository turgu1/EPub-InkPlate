// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __CSS__ 1
#include "models/css.hpp"

#include "models/fonts.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"

#include <cstring>
#include <cctype>

MemoryPool<CSS::Value> CSS::value_pool;
MemoryPool<CSS::Property> CSS::property_pool;
MemoryPool<CSS::Properties> CSS::properties_pool;

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
  { "display",        CSS::PropertyId::DISPLAY        }
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

void
CSS::clear()
{
  rules_map.clear();
}

static const char *
skip_comment(const char * str)
{
  const char * s = str;
  if (*s && (s[0] == '/')) {
    if (s[1] && (s[1] == '*')) {
      s += 2;
      while (*s) {
        if ((*s == '*') && s[1] && (s[1] == '/')) {
          s += 2;
          break;
        }
        s++;
      }
    }
  }
  return s;
}

const char *
CSS::parse_value(const char * str, Value * v, const char * buffer_start)
{
  const char * st = str;
  const char * s  = str;
  bool is_string  = false;
  bool skip_value = v == nullptr;

  if (!skip_value) v->str.clear();
  if (*s == '"') {
    is_string = true;
    s++; 
    st = s;
    while (*s && (*s != '"') && (*s != '\r') && (*s != '\n')) s++;
    if (!skip_value) v->str.assign(st, s - st);
    if (*s != '"') {
      LOG_E("parse_value(): Expected '\"'");
    }
    else {
      s++;
    }
  }
  else if (isalpha(*s) || (*s == '!')) {
    is_string = true;
    while (*s && (strchr(",;\r\n}", *s) == nullptr)) s++;
    if (!skip_value) v->str.assign(st, s - st);
  }
  else {
    while (*s && (strchr(" ,;\r\n\t}", *s) == nullptr)) s++;
    if (!skip_value) {
      if (s == st) {
        LOG_E("parse_value(): nothing to parse");
        v->value_type = ValueType::NOTHING;
      }
      else {
        if (isdigit(*st) || (*st == '-') || (*st == '+') || (*st == '.')) {
          int16_t last_idx = s - st - 1;
          v->num = atof(st);
          if (st[last_idx] == '%') v->value_type = ValueType::PERCENT;
          else if ((st[last_idx - 1] == 'p') && (st[last_idx] == 't')) v->value_type = ValueType::PT;
          else if ((st[last_idx - 1] == 'p') && (st[last_idx] == 'x')) v->value_type = ValueType::PX;
          else if ((st[last_idx - 1] == 'e') && (st[last_idx] == 'm')) v->value_type = ValueType::EM;
          else if ((st[last_idx - 1] == 'c') && (st[last_idx] == 'm')) v->value_type = ValueType::CM;
          else if ((st[last_idx - 1] == 'v') && (st[last_idx] == 'h')) v->value_type = ValueType::VH;
          else if ((st[last_idx - 1] == 'v') && (st[last_idx] == 'w')) v->value_type = ValueType::VW;
          else if (!isdigit(st[last_idx])) {
            std::string val;
            val.assign(st, s - st);
            LOG_E("Unknown value type: '%s' at offset: %d", val.c_str(), (int32_t)(s - buffer_start));
            v->value_type = ValueType::NOTHING;
          }
          else v->value_type = ValueType::NOTYPE;
        }
        else {
          v->str.assign(st, s - st);
          v->value_type = ValueType::STR;
        }
      }
    }
  }

  if (!skip_value && is_string) {
    if (v->str.compare(0, 4, "url(") == 0) {
      if ((v->str[4] == '"') || (v->str[4] == '\'')) {
        v->str.assign(v->str.substr(5, v->str.length() - 7));
      }
      else {
        v->str.assign(v->str.substr(4, v->str.length() - 5));
      }
      v->value_type = ValueType::URL;
    }
    else {
      v->value_type = ValueType::STR;
    }
  }

  return s;
}

const char *
CSS::parse_property_name(const char * str, std::string & name)
{
  const char * s = str;

  name.clear();
  // while (strchr("@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-", *s)) s++;
  while (isalpha(*s) || (*s == '-') || (*s == '@')) s++;
  if (s != str) {
    name.assign(str, s - str);
  }
  else {
    LOG_E("parse_property_name(): No property name present!");
    return nullptr;
  }
  
  return s;
}

#define SKIP_BLANKS { str = skip_comment(str); while ((str < end) && ((*str == ' ') || (*str == '\t') || (*str == '\r') || (*str == '\n') || (*str == '\f'))) { str++; str = skip_comment(str); } }

CSS::Properties *
CSS::parse_properties(const char **buffer, const char * end, const char * buffer_start)
{
  const char * str = *buffer;
  std::string w;

  Property * property;

  CSS::Properties * properties = properties_pool.newElement();
  if (properties == nullptr) msg_viewer.out_of_memory("properties pool allocation");

  while (str < end) {
    str = parse_property_name(str, w);

    if (str == nullptr) break;

    SKIP_BLANKS;
    
    if (*str != ':')  { 
      LOG_E("css error: ':' expected at offset %d", (int32_t)(str - buffer_start));
      return nullptr; 
    }
    str++; SKIP_BLANKS;

    PropertyId id;
    PropertyMap::iterator it = property_map.find(w);
    id = (it == property_map.end()) ? PropertyId::NOT_USED : it->second;

    bool skip_property = id == PropertyId::NOT_USED;

    if (!skip_property) {
      if ((property = property_pool.newElement()) == nullptr) {
        msg_viewer.out_of_memory("property pool allocation");
      }
      property->values.clear();
      property->id = id;
    }

    // parse values

    while (str < end) {
      Value * v = skip_property ? nullptr : value_pool.newElement();

      str = parse_value(str, v, buffer_start);

      if (!skip_property) {
        if (v == nullptr) msg_viewer.out_of_memory("css pool allocation");
        if (property->id == PropertyId::TEXT_ALIGN) {
          if      (v->str.compare("left"     ) == 0) v->choice.align = Align::LEFT;
          else if (v->str.compare("center"   ) == 0) v->choice.align = Align::CENTER;
          else if (v->str.compare("right"    ) == 0) v->choice.align = Align::RIGHT;
          else if (v->str.compare("justify"  ) == 0) v->choice.align = Align::JUSTIFY;
          else if (v->str.compare("justified") == 0) v->choice.align = Align::JUSTIFY;
          else {
            LOG_E("text-align not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == PropertyId::TEXT_TRANSFORM) {
          if      (v->str.compare("lowercase"   ) == 0) v->choice.text_transform = TextTransform::LOWERCASE;
          else if (v->str.compare("uppercase"   ) == 0) v->choice.text_transform = TextTransform::UPPERCASE;
          else if (v->str.compare("capitalize"  ) == 0) v->choice.text_transform = TextTransform::CAPITALIZE;
          else {
            LOG_E("text-transform not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == PropertyId::FONT_WEIGHT) {
          if      ((v->str.compare("bold"  ) == 0) || (v->str.compare("bolder" ) == 0)) v->choice.face_style = Fonts::FaceStyle::BOLD;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice.face_style = Fonts::FaceStyle::NORMAL;
          else {
            LOG_E("font-weight not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == PropertyId::FONT_STYLE) {
          if      ((v->str.compare("italic") == 0) || (v->str.compare("oblique") == 0)) v->choice.face_style = Fonts::FaceStyle::ITALIC;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice.face_style = Fonts::FaceStyle::NORMAL;
          else {
            LOG_E("font-style not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == PropertyId::DISPLAY) {
          if      (v->str.compare("none"        ) == 0) v->choice.display = Display::NONE;
          else if (v->str.compare("inline"      ) == 0) v->choice.display = Display::INLINE;
          else if (v->str.compare("block"       ) == 0) v->choice.display = Display::BLOCK;
          else if (v->str.compare("inline-block") == 0) v->choice.display = Display::INLINE_BLOCK;
          else {
            LOG_E("display not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if ((property->id == PropertyId::FONT_SIZE) && (v->value_type == ValueType::STR)) {
          v->value_type = ValueType::PT;
          FontSizeMap::iterator it = font_size_map.find(v->str);
          if (it != font_size_map.end()) {
            v->num = it->second;
          }
          else {
            int8_t font_size;
            config.get(Config::Ident::FONT_SIZE, &font_size);
            v->num = font_size;
          }
        }

        property->values.push_front(v);
      }

      SKIP_BLANKS;
      if (*str == ',') { str++; SKIP_BLANKS; }
      if ((str >= end) || (*str == ';') || (*str == '}')) break;
    }

    if (!skip_property) {
      property->values.reverse();
      properties->push_front(property);
    }

    if (*str != ';') break;
    str++; SKIP_BLANKS;
    if (*str == '}') break;
  }

  *buffer = str;

  return properties;
}

const char *
CSS::parse_selector(const char * str, std::string & selector)
{
  const char * start = str;
  const char * s     = str;

  selector.clear();

  while (strchr("@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+#*>:.~^=_[]()|$\" ", *s)) s++;
  if (s == start) {
    LOG_E("parse_selector(): There is no selector!");
  }
  else {
    s--;
    while ((s > start) && (*s == ' ')) s--;
    if ((s == start) && (*s == ' ')) {
      LOG_E("parse_selector(): There is no selector!");
    }
    else {
      s++;
      selector.assign(start, s - start);
    }
  }

  return s;
}

const char * skip_struct(const char * str, const char * end) 
{
  const char * s = str;  

  while ((s < end) && (*s != '{')) s++;
  if (s < end) {
    int brace_count = 1;
    s++;
    while ((s < end) && (brace_count > 0)) {
      if (*s == '{') brace_count++;
      else if (*s == '}') brace_count--;
      s++;
    }
  }
  return s;
}

CSS::CSS(std::string folder_path, 
         const char * buffer, 
         int32_t size, 
         const std::string & css_id)
{
  // LOG_D("Adding new CSS: %d", css_id);
  
  id = css_id;
  file_folder_path = folder_path;

  buffer_start = buffer;

  if ((ghost = (buffer == nullptr))) return;
  
  Properties * properties;
  Selectors selectors;

  const char * str = buffer;
  const char * end = buffer + size;
  std::string w;

  // LOG_E("CSS file before analysis: %s", buffer);
  // bool found = false;

  while (str < end) {
    SKIP_BLANKS;

    // parse selectors

    selectors.clear();  

    while ((str < end) && (str = parse_selector(str, w))) {
      if ((w.compare(0, 8, "@charset") == 0) || (w.compare(0, 10, "@namespace") == 0)) {
        while (*str && (*str != ';')) str++;
        if (*str == ';') str++;
        SKIP_BLANKS;
        continue;
      }
      else if (w.compare(0, 6, "@media") == 0) {
        str = skip_struct(str, end);
        SKIP_BLANKS;
        continue;
      }

      // if (w.compare(".indent") == 0) {
      //   LOG_D("Found it");
      //   found = true;
      // }
      selectors.push_back(w);
      SKIP_BLANKS;
      if (*str != ',') break;
      str++; SKIP_BLANKS;
    }
    if (str >= end) break;
    SKIP_BLANKS;
    if (*str != '{') { 
      LOG_E("css error: '{' expected at offset %d", (int32_t)(str - buffer)); 
      return; 
    }
    str++; SKIP_BLANKS;

    // Parse properties

    if (*str != '}') {
      properties = parse_properties(&str, end, str);

      if (properties && !properties->empty()) {
        for (auto & sel : selectors) {
          add_properties_to_selector(sel, properties);
        }
        if (!ghost) suites.push_front(properties);
      }
      else if (properties) { // Is empty, but takes some bytes...
        properties_pool.deleteElement(properties);
      }
    }

    // if (found) {
    //   found = false;
    //   show_properties(*properties);
    // }
    SKIP_BLANKS;

    if (*str != '}')  { 
      LOG_E("css error: '}' expected at offset %d", (int32_t)(str - buffer)); 
      return; 
    }
    str++; SKIP_BLANKS;
  }

  return;
}

CSS::~CSS()
{
  for (auto * properties : suites) {
    clear_properties(properties);
  }
  suites.clear();
}

void
CSS::show_properties(const Properties & props) const
{
  #if DEBUGGING
    std::cout << " { ";

    bool first1 = true;
    for (auto & property : props) {
      if (first1) first1 = false; else std::cout << "; ";
      std::cout << (int)property->id << ": ";

      bool first2 = true;
      for (auto * value : property->values) {
        if (first2) first2 = false; else std::cout << ", ";
        if (value->value_type == ValueType::STR) {
          std::cout << value->str;
        }
        else if (value->value_type == ValueType::URL) {
          std::cout << "url(" << value->str << ')';
        }
        else {
          std::cout << value->num;
          if      (value->value_type == ValueType::PX     ) std::cout << "px";
          else if (value->value_type == ValueType::PT     ) std::cout << "pt"; 
          else if (value->value_type == ValueType::EM     ) std::cout << "em"; 
          else if (value->value_type == ValueType::PERCENT) std::cout << "%"; 
        } 
      } 
    }   
    std::cout << " }" << std::endl;
  #endif
};

void
CSS::show() const
{
  #if DEBUGGING
    std::cout << "CSS Content (Size: " << rules_map.size() << ") :" << std::endl;

    for (auto & rule : rules_map) {

      for (auto & bundle : rule.second) {
        std::cout << rule.first;
        show_properties(*bundle);
      }
    }
    std::cout << "[End]" << std::endl;
  #endif
}
