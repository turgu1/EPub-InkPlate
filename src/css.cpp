#define __CSS__ 1
#include "css.hpp"

#include "fonts.hpp"

#include <cstring>
#include <cctype>

CSS::PropertyMap CSS::property_map = {
  { "not-used",       CSS::NOT_USED       },
  { "font-family",    CSS::FONT_FAMILY    }, 
  { "font-size",      CSS::FONT_SIZE      }, 
  { "font-style",     CSS::FONT_STYLE     },
  { "font-weight",    CSS::FONT_WEIGHT    },
  { "text-align",     CSS::TEXT_ALIGN     },
  { "text-indent",    CSS::TEXT_INDENT    },
  { "text-transform", CSS::TEXT_TRANSFORM },
  { "line-height",    CSS::LINE_HEIGHT    },
  { "src",            CSS::SRC            },
  { "margin",         CSS::MARGIN         },
  { "margin-top",     CSS::MARGIN_TOP     },
  { "margin-bottom",  CSS::MARGIN_BOTTOM  },
  { "margin-left",    CSS::MARGIN_LEFT    },
  { "margin-right",   CSS::MARGIN_RIGHT   },
  { "width",          CSS::WIDTH          },
  { "height",         CSS::HEIGHT         }
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
        v->value_type = NOTHING;
      }
      else {
        if (isdigit(*st) || (*st == '-') || (*st == '+') || (*st == '.')) {
          int16_t last_idx = s - st - 1;
          v->num = atof(st);
          if (st[last_idx] == '%') v->value_type = PERCENT;
          else if ((st[last_idx - 1] == 'p') && (st[last_idx] == 't')) v->value_type = PT;
          else if ((st[last_idx - 1] == 'p') && (st[last_idx] == 'x')) v->value_type = PX;
          else if ((st[last_idx - 1] == 'e') && (st[last_idx] == 'm')) v->value_type = EM;
          else if ((st[last_idx - 1] == 'c') && (st[last_idx] == 'm')) v->value_type = CM;
          else if (!isdigit(st[last_idx])) {
            std::string val;
            val.assign(st, s - st);
            LOG_E("Unknown value type: '%s' at offset: %d", val.c_str(), (int32_t)(s - buffer_start));
            v->value_type = NOTHING;
          }
          else v->value_type = NOTYPE;
        }
        else {
          v->str.assign(st, s - st);
          v->value_type = STR;
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
      v->value_type = URL;
    }
    else {
      v->value_type = STR;
    }
  }

  return s;
}

const char *
CSS::parse_property_name(const char * str, std::string & name)
{
  const char * s = str;

  name.clear();
  while (strchr("@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-", *s)) s++;
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

  CSS::Properties * properties = new Properties;

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
    id = (it == property_map.end()) ? NOT_USED : it->second;

    bool skip_property = id == NOT_USED;

    if (!skip_property) {
      property = new Property;
      property->values.clear();
      property->id = id;
    }

    // parse values

    while (str < end) {
      Value * v = skip_property ? nullptr : new Value;

      str = parse_value(str, v, buffer_start);

      if (!skip_property) {
        v->choice = 0;
        if (property->id == TEXT_ALIGN) {
          if      (v->str.compare("left"     ) == 0) v->choice = LEFT_ALIGN;
          else if (v->str.compare("center"   ) == 0) v->choice = CENTER_ALIGN;
          else if (v->str.compare("right"    ) == 0) v->choice = RIGHT_ALIGN;
          else if (v->str.compare("justify"  ) == 0) v->choice = JUSTIFY;
          else if (v->str.compare("justified") == 0) v->choice = JUSTIFY;
          else {
            LOG_E("text-align not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == TEXT_TRANSFORM) {
          if      (v->str.compare("lowercase"   ) == 0) v->choice = LOWERCASE;
          else if (v->str.compare("uppercase"   ) == 0) v->choice = UPPERCASE;
          else if (v->str.compare("capitalize"  ) == 0) v->choice = CAPITALIZE;
          else {
            LOG_E("text-transform not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == FONT_WEIGHT) {
          if      ((v->str.compare("bold"  ) == 0) || (v->str.compare("bolder" ) == 0)) v->choice = Fonts::BOLD;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice = Fonts::NORMAL;
          else {
            LOG_E("font-weight not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if (property->id == FONT_STYLE) {
          if      ((v->str.compare("italic") == 0) || (v->str.compare("oblique") == 0)) v->choice = Fonts::ITALIC;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice = Fonts::NORMAL;
          else {
            LOG_E("font-style not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str - buffer_start));
          }
        }
        else if ((property->id == FONT_SIZE) && (v->value_type == STR)) {
          v->value_type = PT;
          FontSizeMap::iterator it = font_size_map.find(v->str);
          if (it != font_size_map.end()) {
            v->num = it->second;
          }
          else {
            v->num = font_normal_size;
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
        delete properties;
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
    for (auto * property : *properties) {
      for (auto * val : property->values) {
        delete val;
      }
      delete property;
    }
    delete properties;
  }
  suites.clear();
}

void
CSS::show_properties(const Properties & props) const
{

  std::cout << " { ";

  bool first1 = true;
  for (auto & property : props) {
    if (first1) first1 = false; else std::cout << "; ";
    std::cout << property->id << ": ";

    bool first2 = true;
    for (auto * value : property->values) {
      if (first2) first2 = false; else std::cout << ", ";
      if (value->value_type == STR) {
        std::cout << value->str;
      }
      else if (value->value_type == URL) {
        std::cout << "url(" << value->str << ')';
      }
      else {
        std::cout << value->num;
        if (value->value_type == PX) std::cout << "px";
        else if (value->value_type == PT) std::cout << "pt"; 
        else if (value->value_type == EM) std::cout << "em"; 
        else if (value->value_type == PERCENT) std::cout << "%"; 
      } 
    } 
  }   
  std::cout << " }" << std::endl;
};

void
CSS::show() const
{
  std::cout << "CSS Content (Size: " << rules_map.size() << ") :" << std::endl;

  for (auto & rule : rules_map) {

    for (auto & bundle : rule.second) {
      std::cout << rule.first;
      show_properties(*bundle);
    }
  }
  std::cout << "[End]" << std::endl;
}
