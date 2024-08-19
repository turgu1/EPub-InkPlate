
// Simple CSS Parser
//
// Guy Turcotte
// January 2021
//
// This is a bare CSS Parser. Only lexical and syntax analysis.
//
// The parser implements the definition as stated in appendix G of
// the CSS V2.1 definition (https://www.w3.org/TR/CSS21/grammar.html)
// with the following differences:
//
// - No support for unicode
// - Supports numbers starting with '.'
// - Supports @font-style 
//
// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <cstring>
#include <cmath>

#if CSS_PARSER_TEST
  #include <iostream>
#endif

#include "css.hpp"

class CSSParser
{
  public:
    CSSParser(CSS & the_css, const char * buffer, int32_t size) 
        : css(the_css), skip(0), sel_nbr(0) { 
      parse(buffer, size); 
    }
    
    CSSParser(CSS & the_css, DOM::Tag tag, const char * buffer, int32_t size) 
        : css(the_css), skip(0), sel_nbr(0) { 
      parse(tag, buffer, size); 
    }

   ~CSSParser() { }

  private:
    static constexpr char const * TAG = "CSSParser";
    
    CSS &   css;
    uint8_t skip;
    uint8_t sel_nbr;

    // ---- Tokenizer ----

    static const int8_t  IDENT_SIZE  =  60;
    static const int8_t  NAME_SIZE   =  60;
    static const int16_t STRING_SIZE = 128;

    enum class Token : uint8_t { 
      ERROR,         WHITESPACE, CDO,        CDC,       INCLUDES,  DASHMATCH,   STRING,        BAD_STRING, 
      IDENT,         HASH,       IMPORT_SYM, PAGE_SYM,  MEDIA_SYM, CHARSET_SYM, FONT_FACE_SYM, IMPORTANT_SYM, 
      NAMESPACE_SYM, LENGTH,     ANGLE,      TIME,      FREQ,      DIMENSION,   PERCENTAGE,    NUMBER,
      URI,           BAD_URI,    FUNCTION,   SEMICOLON, COLON,     COMMA,       GT,            LT,
      GE,            LE,         MINUS,      PLUS,      DOT,       STAR,        SLASH,         EQUAL,
      LBRACK,        RBRACK,     LBRACE,     RBRACE,    LPARENT,   RPARENT,     TILDE,         END_OF_FILE
    };

    const char * token_names[48] = { 
      "ERROR",         "WHITESPACE", "CDO",        "CDC",       "INCLUDES",  "DASHMATCH",   "STRING",        "BAD_STRING", 
      "IDENT",         "HASH",       "IMPORT_SYM", "PAGE_SYM",  "MEDIA_SYM", "CHARSET_SYM", "FONT_FACE_SYM", "IMPORTANT_SYM", 
      "NAMESPACE_SYM", "LENGTH",     "ANGLE",      "TIME",      "FREQ",      "DIMENSION",   "PERCENTAGE",    "NUMBER",        
      "URI",           "BAD_URI",    "FUNCTION",   "SEMICOLON", "COLON",     "COMMA",       "GT",            "LT",
      "GE",            "LE",         "MINUS",      "PLUS",      "DOT",       "STAR",        "SLASH",         "EQUAL",
      "LBRACK",        "RBRACK",     "LBRACE",     "RBRACE",    "LPARENT",    "RPARENT",    "TILDE",         "END_OF_FILE"
    };

    int32_t      remains; // number of bytes remaining in the css buffer
    const char * str;     // pointer in the css buffer
    int16_t      line_nbr;
    uint8_t      ch;      // next character to be processed

    char  ident[ IDENT_SIZE];
    char string[STRING_SIZE];
    char   name[  NAME_SIZE];

    float      num;
    
    Token      token;

    CSS::ValueType value_type;

    void next_ch() { 
      if (remains > 0) { 
        remains--; 
        ch = *str++; 
        if (ch == '\n') line_nbr++;
      } 
      else ch = '\0'; 
    }
    
    inline bool is_white_space() {
      return (ch == ' ' ) ||
             (ch == '\t') ||
             (ch == '\r') ||
             (ch == '\n') ||
             (ch == '\f'); 
    }

    inline bool is_nmchar() {
      return ((ch >= 'a') && (ch <= 'z')) ||
             ((ch >= 'A') && (ch <= 'Z')) ||
             ((ch >= '0') && (ch <= '9')) ||
             (ch == '_')  || 
             (ch == '-')  ||
             (ch == '\\') ||
             (ch >= 160);
    }
    
    bool is_nmstart(uint8_t c) {
      return ((c >= 'a') && (c <= 'z')) ||
             ((c >= 'A') && (c <= 'Z')) ||
             (c == '_' ) || 
             (c == '-')  ||
             (c == '\\') ||
             (c >= 160);
    }
    
    void skip_spaces() {
      while (is_white_space()) next_ch();
    }

    bool parse_url() {
      int16_t idx = 0;
      while ((ch > ' ') && (ch != ')') && (idx < (STRING_SIZE - 1))) {
        if ((ch == '"') || (ch == '\'') || (ch == '(')) {
          return false;
        }
        if (ch == '\\') {
          next_ch();
          if (ch == '\0') return false;
          if (ch == '\r') {
            string[idx++] = '\n';
            next_ch();
            if (ch == '\n') next_ch();
          }
          else {
            string[idx++] = ch;
            next_ch();
          }
        }
        else {
          string[idx++] = ch;
          next_ch();
        }
      }
      string[idx] = 0;
      return true;
    }

    bool parse_string() {
      char    delim = ch;
      int16_t idx   = 0;

      next_ch();
      while ((ch != '\0') && (ch != delim) && (idx < (STRING_SIZE - 1))) {
        if (ch == '\\') {
          next_ch();
          if (ch == '\0') return false;
          if (ch == '\r') {
            string[idx++] = '\n';
            next_ch();
            if (ch == '\n') next_ch();
          }
          else {
            string[idx++] = ch;
            next_ch();
          }
        }
        else if ((ch >= ' ') || (ch == '\t')) {
          string[idx++] = ch;
          next_ch();
        }
        else {
          next_ch();
        }
      }
      string[idx] = 0;
      bool res = ch == delim;
      if (res) next_ch();
      return res;
    }

    void parse_number() {
      num = 0;
      bool neg = ch == '-';
      if (neg) next_ch();
      else if (ch == '+') next_ch();
  
      float dec = 0.1;
      while ((ch >= '0') && (ch <= '9')) {
        num = (num * 10) + (ch - '0');
        next_ch();
      }
      if (ch == '.') {
        next_ch();
        while ((ch >= '0') && (ch <= '9')) {
          num = num + (dec * (ch - '0'));
          dec = dec * 0.1;
          next_ch();
        }
      }

      if (neg) num = -num;

      if (((ch == 'e') || (ch == 'E')) && ((str[0] >= '0') && (str[0] <= '9'))) {
        float exp = 0.0;
        next_ch();
        neg = ch == '-';
        if (neg) next_ch();
        else if (ch == '+') next_ch();
        while ((ch >= '0') && (ch <= '9')) {
          exp = (exp * 10) + (ch - '0');
          next_ch();
        }
        if (exp > 0) {
          if (neg) exp = -exp;
          num = pow(num, exp);
        }
      }
    }

    void parse_name() {
      int8_t idx = 0;
      while ((ch != '\0') && is_nmchar() && (idx < (NAME_SIZE - 1))) {
        if (ch == '\\') {
          next_ch();
          if (ch >= ' ') {
            name[idx++] = ch;
            next_ch();
          }
          else break;
        }
        else {
          name[idx++] = ch;
          next_ch();
        }
      }
      if (idx < NAME_SIZE) string[idx] = 0;
    }

    void parse_ident() {
      int8_t idx = 0;
      while ((ch != '\0') && is_nmchar() && (idx < (NAME_SIZE - 1))) {
        if (ch == '\\') {
          next_ch();
          if (ch >= ' ') {
            ident[idx++] = ch;
            next_ch();
          }
          else break;
        }
        else {
          ident[idx++] = ch;
          next_ch();
        }
      }
      if (idx < IDENT_SIZE) ident[idx] = 0;
      #if CSS_PARSER_TEST
        std::cout << "Ident: " << ident << std::endl;
      #endif
    }

    bool skip_comment() {
      for (;;) {
        if ((ch == '*') && (str[0] == '/')) {
          next_ch(); next_ch();
          break;
        }
        else if (ch == '\0') return false;
        next_ch();
      }
      return true;
    }

    void next_token() {
      bool done = false;
      while (!done) {
        if      (ch == '\0') token = Token::END_OF_FILE;
        else if (is_white_space()) { next_ch(); token = Token::WHITESPACE; }
        else if (ch == ';')        { next_ch(); token = Token::SEMICOLON;  }
        else if (ch == ':')        { next_ch(); token = Token::COLON;      }
        else if (ch == ',')        { next_ch(); token = Token::COMMA;      }
        else if (ch == '{')        { next_ch(); token = Token::LBRACE;     }
        else if (ch == '}')        { next_ch(); token = Token::RBRACE;     }
        else if (ch == '[')        { next_ch(); token = Token::LBRACK;     }
        else if (ch == ']')        { next_ch(); token = Token::RBRACK;     }
        else if (ch == '(')        { next_ch(); token = Token::LPARENT;    }
        else if (ch == ')')        { next_ch(); token = Token::RPARENT;    }
        else if (ch == '=')        { next_ch(); token = Token::EQUAL;      }
        else if (ch == '*')        { next_ch(); token = Token::STAR;       }
        else if ((ch == '\'') || (ch == '\"')) {
          token = parse_string() ? Token::STRING : Token::BAD_STRING;
          value_type = CSS::ValueType::STR;
        }
        else if (
            (((ch == '-') || (ch == '+')) && ((str[0] == '.') || ((str[0] >= '0') && (str[0] <= '9')))) ||
            ((ch >= '0') && (ch <= '9')) || 
            ((ch == '.') && ((str[0] >= '0') && (str[0] <= '9')))) {
          parse_number();
          token      = Token::NUMBER;
          value_type = CSS::ValueType::NO_TYPE;
          if      ((ch == 'e') && (str[0] == 'm')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::EM;      }
          else if ((ch == 'e') && (str[0] == 'x')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::EX;      }
          else if  (ch == '%')                     { next_ch();            token = Token::PERCENTAGE; value_type = CSS::ValueType::PERCENT; }
          else if ((ch == 'p') && (str[0] == 'x')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::PX;      }
          else if ((ch == 'p') && (str[0] == 't')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::PT;      }
          else if ((ch == 'p') && (str[0] == 'c')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::PC;      }
          else if ((ch == 'c') && (str[0] == 'm')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::CM;      }
          else if ((ch == 'm') && (str[0] == 'm')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::MM;      }
          else if ((ch == 'i') && (str[0] == 'n')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::IN;      }
          else if ((ch == 'v') && (str[0] == 'h')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::VH;      }
          else if ((ch == 'v') && (str[0] == 'w')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::VW;      }
          else if ((ch == 'c') && (str[0] == 'h')) { next_ch(); next_ch(); token = Token::LENGTH;     value_type = CSS::ValueType::CH;      }
          else if ((ch == 'm') && (str[0] == 's')) { next_ch(); next_ch(); token = Token::TIME  ;     value_type = CSS::ValueType::MSEC;    }
          else if  (ch == 's')                     { next_ch();            token = Token::TIME  ;     value_type = CSS::ValueType::SEC;     }
          else if ((ch == 'h') && (str[0] == 'z')) { next_ch(); next_ch(); token = Token::FREQ  ;     value_type = CSS::ValueType::HZ;      }
          else if ((ch == 'k') && (str[0] == 'h') && (str[1] == 'z')) {
            remains -= 2; str += 2; next_ch(); token = Token::FREQ; value_type = CSS::ValueType::KHZ;
          }
          else if ((ch == 'd') && (str[0] == 'e') && (str[1] == 'g')) {
            remains -= 2; str += 2; next_ch(); token = Token::ANGLE; value_type = CSS::ValueType::DEG;
          }
          else if ((ch == 'r') && (str[0] == 'a') && (str[1] == 'd')) {
            remains -= 2; str += 2; next_ch(); token = Token::ANGLE; value_type = CSS::ValueType::RAD;
          }
          else if ((ch == 'r') && (str[0] == 'e') && (str[1] == 'm')) {
            remains -= 2; str += 2; next_ch(); token = Token::LENGTH; value_type = CSS::ValueType::REM;
          }
          else if ((ch == 'g') && (str[0] == 'r') && (str[1] == 'a') && (str[2] == 'd')) {
            remains -= 3; str += 3; next_ch(); token = Token::ANGLE; value_type = CSS::ValueType::GRAD;
          }
          else if ((ch == 'v') && (str[0] == 'm') && (str[1] == 'i') && (str[2] == 'n')) {
            remains -= 3; str += 3; next_ch(); token = Token::LENGTH; value_type = CSS::ValueType::VMIN;
          }
          else if ((ch == 'v') && (str[0] == 'm') && (str[1] == 'a') && (str[2] == 'x')) {
            remains -= 3; str += 3; next_ch(); token = Token::LENGTH; value_type = CSS::ValueType::VMAX;
          }
          else if (is_nmstart(ch)) {
            parse_ident();
            token = Token::DIMENSION;
            value_type= CSS::ValueType::DIMENSION;
          }
        }
        else if ((ch == '-') && (str[0] == '-') && (str[1] == '>')) {
          token = Token::CDC;
          remains -= 2; str += 2; next_ch();
        }        
        else if (is_nmstart(ch) || ((ch == '-') && (is_nmstart(str[0])))) {
          parse_ident();
          token = Token::IDENT;
          if (ch == '(') {
            next_ch();
            if (strcmp((char *)ident, "url") == 0) {
              skip_spaces();
              if ((ch == '"') || (ch == '\'')) {
                token = parse_string() ? Token::URI : Token::BAD_URI;
              }
              else {
                token = parse_url() ? Token::URI : Token::BAD_URI;
              }
              skip_spaces();
              if (ch == ')') {
                next_ch();
              }
              else {
                token = Token::BAD_URI;
              }
            }
            else {
              token = Token::FUNCTION;
            }
          } 
        }
        else if ((ch == '<') && (strncmp((char *)str, "!--", 3) == 0)) {
          token = Token::CDO;
          remains -= 3; str += 3; next_ch();
        }
        else if ((ch == '>') && (str[0] == '=')) { next_ch(); next_ch(); token = Token::GE; }
        else if ((ch == '<') && (str[0] == '=')) { next_ch(); next_ch(); token = Token::LE; }
        else if (ch == '+') { next_ch(); token = Token::PLUS;      }
        else if (ch == '-') { next_ch(); token = Token::MINUS;     }
        else if (ch == '.') { next_ch(); token = Token::DOT;       }
        else if (ch == '>') { next_ch(); token = Token::GT;        }
        else if (ch == '<') { next_ch(); token = Token::LT;        }
        else if (ch == '#') {
          next_ch();
          parse_name();
          token = Token::HASH;
        }
        else if (ch == '@') {
          if (strncmp((char *) str, "media", 5) == 0) {
            token = Token::MEDIA_SYM;
            remains -= 5; str += 5; next_ch();
          }
          else if (strncmp((char *) str, "page", 4) == 0) {
            token = Token::PAGE_SYM;
            remains -= 4; str += 4; next_ch();
          }
          else if (strncmp((char *) str, "import", 6) == 0) {
            token = Token::IMPORT_SYM;
            remains -= 6; str += 6; next_ch();
          }
          else if (strncmp((char *) str, "charset ", 8) == 0) {
            token = Token::CHARSET_SYM;
            remains -= 8; str += 8; next_ch();
          }
          else if (strncmp((char *) str, "font-face", 9) == 0) {
            token = Token::FONT_FACE_SYM;
            remains -= 9; str += 9; next_ch();
          }
          else if (strncmp((char *) str, "namespace ", 10) == 0) {
            token = Token::NAMESPACE_SYM;
            remains -= 10; str += 10; next_ch();
          }
          else {
            LOG_E("%s[%d]: Sym Token Unknown: @%s", css.get_id().c_str(), line_nbr, str);
            token = Token::ERROR;
          }
        }  
        else if (ch == '~') {
          next_ch();
          if (ch == '=') {
            next_ch();
            token = Token::INCLUDES;
          }
          else {
            token = Token::TILDE;
          }
        }     
        else if (ch == '|') {
          next_ch();
          if (ch == '=') {
            next_ch();
            token = Token::DASHMATCH;
          }
          else {
            LOG_E("%s[%d]: Unknown usage: %c", css.get_id().c_str(), line_nbr, ch);
            token = Token::ERROR;
          }
        }
        else if ((ch == '/') && (str[0] == '*')) {
          next_ch(); next_ch();
          if (!skip_comment()) {
            LOG_E("%s[%d]: Non terminated comment.", css.get_id().c_str(), line_nbr);
            token = Token::ERROR;
          }
          else continue;
        }
        else if (ch == '/') { next_ch(); token = Token::SLASH; }
        else if (ch == '+') { next_ch(); token = Token::PLUS;  }
        else if (ch == '!') {
          next_ch();
          for (;;) {
            skip_spaces();
            if ((ch == '/') && (str[0] == '*')) {
              next_ch(); next_ch();
              if (!skip_comment()) {
                LOG_E("%s[%d]: Non terminated comment.", css.get_id().c_str(), line_nbr);
                token = Token::ERROR;
                done = true;
                break;
              }
              else continue;
            }
            else break;
          }
          if (!done && (ch == 'i') && (strncmp((char *)str, "mportant", 8) == 0)) {
            remains -= 8; str += 8; next_ch();
            token = Token::IMPORTANT_SYM;
          }
        }
        else {
          LOG_E("%s[%d]: Unknown usage: %c", css.get_id().c_str(), line_nbr, ch);
          token = Token::ERROR;
        }

        done = true;
      }

      #if CSS_PARSER_TEST
        std::cout << '[' << token_names[(int)token] << ']' << std::flush;
      #endif
    }

    // ---- Parser ----

    /**
     * Skip all blanks
     * 
     * On entry, the current token must point to an already parsed item.
     * On return, the token point at the next valid token passed whitespaces.
     */
    void skip_blanks() {
      if (token == Token::END_OF_FILE) return;
      do next_token(); while (token == Token::WHITESPACE);
    }

    /**
     * Skip a block
     * 
     * Called to get rid of a complete block of statements. Two kind of blocks are
     * managed by this method, considering the first occurence of:
     * 1) a semicolon or the end of the file: this end the skip process
     * 2) a left brace: the stream will be skipped until a right brace is encountered. 
     *    Multiple level of braces is supported.
     */
    bool skip_block() {

      while ((token != Token::END_OF_FILE) && 
             (token != Token::SEMICOLON  ) && 
             (token != Token::LBRACE     ) &&
             (token != Token::RBRACE     )) next_token();
      if (token == Token::SEMICOLON) {
        skip_blanks();
      }
      else if (token == Token::LBRACE) {
        skip_blanks();
        int brace_count = 1;
        while (brace_count > 0) {
          if (token == Token::END_OF_FILE) return false;
          if (token == Token::LBRACE) {
            brace_count += 1;
            skip_blanks();
          }
          else if (token == Token::RBRACE) {
            brace_count -= 1;
            skip_blanks();
          }
          else {
            next_token();
          }
        }
      }
      else if (token != Token::RBRACE) {
        return false;
      }

      return true;
    }

    bool import_statement() {
      skip_blanks();
      if ((token == Token::URI) || (token == Token::STRING)) {
        // Import a css file
      }
      skip_blanks();
      if (token == Token::IDENT) {
        skip_blanks();
        while (token == Token::COMMA) {
          skip_blanks();
          if (token != Token::IDENT) return false;
          skip_blanks();
        }
      }
      if (token == Token::SEMICOLON) {
        skip_blanks();
      }
      else return false;
      return true;
    }

    #if 0
    bool function() {
      skip_blanks();
      expression();
      if (token == Token::RPARENT) skip_blanks();
      else return false;
      return true;
    }
    #endif

    CSS::Value * term(bool & none, CSS::PropertyId id) {
      none = false;
      bool neg = false;
      CSS::Value * v = css.value_pool.newElement();
      bool done = false;
      while (true) {
        if ((token == Token::PLUS) || (token == Token::MINUS)) {
          neg = token == Token::MINUS;
          skip_blanks();
        }
        if ((token == Token::NUMBER    ) ||
            (token == Token::PERCENTAGE) ||
            (token == Token::LENGTH    ) ||
            (token == Token::ANGLE     ) ||
            (token == Token::TIME      ) ||
            (token == Token::DIMENSION ) ||
            (token == Token::FREQ)) {
          v->value_type = value_type;
          v->num = neg ? -num : num;
          if (id == CSS::PropertyId::VERTICAL_ALIGN) v->choice.vertical_align = CSS::VerticalAlign::VALUE;
          skip_blanks();
        }
        else if (token == Token::STRING) {
          v->value_type = CSS::ValueType::STR;
          v->str = string;
          skip_blanks();
        }
        else if (token == Token::IDENT) {
          v->str = ident;
          v->value_type = CSS::ValueType::STR;
          if (id == CSS::PropertyId::TEXT_ALIGN) {
            if      (v->str.compare("left"     ) == 0) v->choice.align = CSS::Align::LEFT;
            else if (v->str.compare("center"   ) == 0) v->choice.align = CSS::Align::CENTER;
            else if (v->str.compare("right"    ) == 0) v->choice.align = CSS::Align::RIGHT;
            else if (v->str.compare("justify"  ) == 0) v->choice.align = CSS::Align::JUSTIFY;
            else if (v->str.compare("justified") == 0) v->choice.align = CSS::Align::JUSTIFY;
            else {
              //LOG_E("text-align not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
              break;
            }
          }
          else if (id == CSS::PropertyId::TEXT_TRANSFORM) {
            if      (v->str.compare("lowercase"   ) == 0) v->choice.text_transform = CSS::TextTransform::LOWERCASE;
            else if (v->str.compare("uppercase"   ) == 0) v->choice.text_transform = CSS::TextTransform::UPPERCASE;
            else if (v->str.compare("capitalize"  ) == 0) v->choice.text_transform = CSS::TextTransform::CAPITALIZE;
            else {
              //LOG_E("text-transform not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
              break;
            }
          }
          else if (id == CSS::PropertyId::VERTICAL_ALIGN) {
            if      (v->str.compare("sub"      ) == 0) v->choice.vertical_align = CSS::VerticalAlign::SUB;
            else if (v->str.compare("super"    ) == 0) v->choice.vertical_align = CSS::VerticalAlign::SUPER;
            else if (v->str.compare("top"      ) == 0) v->choice.vertical_align = CSS::VerticalAlign::SUPER;
            else if (v->str.compare("text-top" ) == 0) v->choice.vertical_align = CSS::VerticalAlign::SUPER;
            else {
              v->choice.vertical_align = CSS::VerticalAlign::NORMAL;
            }
          }
          else if (id == CSS::PropertyId::FONT_WEIGHT) {
            if      ((v->str.compare("bold"  ) == 0) || (v->str.compare("bolder" ) == 0)) v->choice.face_style = Fonts::FaceStyle::BOLD;
            else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice.face_style = Fonts::FaceStyle::NORMAL;
            else {
              //LOG_E("font-weight not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str - buffer_start));
              break;
            }
          }
          else if (id == CSS::PropertyId::FONT_STYLE) {
            if      ((v->str.compare("italic") == 0) || (v->str.compare("oblique") == 0)) v->choice.face_style = Fonts::FaceStyle::ITALIC;
            else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0)) v->choice.face_style = Fonts::FaceStyle::NORMAL;
            else {
              //LOG_E("font-style not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str - buffer_start));
              break;
            }
          }
          else if (id == CSS::PropertyId::DISPLAY) {
            if      (v->str.compare("none"        ) == 0) v->choice.display = CSS::Display::NONE;
            else if (v->str.compare("inline"      ) == 0) v->choice.display = CSS::Display::INLINE;
            else if (v->str.compare("block"       ) == 0) v->choice.display = CSS::Display::BLOCK;
            else if (v->str.compare("inline-block") == 0) v->choice.display = CSS::Display::INLINE_BLOCK;
            else {
              //LOG_E("display not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str - buffer_start));
              break;
            }
          }
          else if ((id == CSS::PropertyId::FONT_SIZE) && (v->value_type == CSS::ValueType::STR)) {
            v->value_type = CSS::ValueType::PT;
            CSS::FontSizeMap::iterator it = css.font_size_map.find(v->str);
            if (it != css.font_size_map.end()) {
              v->num = it->second;
            }
            else {
              // int8_t font_size;
              // config.get(Config::Ident::FONT_SIZE, &font_size);
              // v->num = font_size;
              v->num = 12;
            }
          }
          else if (v->str.compare("inherit") == 0) {
            v->value_type = CSS::ValueType::INHERIT;
          } 
          skip_blanks();
        }
        else if (token == Token::URI) {
          v->str = string;
          v->value_type = CSS::ValueType::URL;
          skip_blanks();
        }
        else if (token == Token::FUNCTION) {
          // if (!function()) return false; // Not supported yet
          break;
        } 
        else if (token == Token::HASH) {
          // v->str = name;
          skip_blanks();
          break; // Not supported yet
        }
        else {
          none = true;
          break;
        }

        done = true;
        break;
      }
      
      if (done) {
        return v;
      }
      else {
        css.value_pool.deleteElement(v);
        return nullptr;
      }
    }

    bool expression(CSS::Property & prop) {
      CSS::Value * v;
      bool none;
      if ((((v = term(none, prop.id))) == nullptr) || none) return false;
      prop.add_value(v);
      for (;;) {
        if ((token == Token::SLASH) || (token == Token::COMMA)) {
          skip_blanks();
        }
        if ((((v = term(none, prop.id))) == nullptr) && !none) return false;
        if (none) break;
        prop.add_value(v);
      }
      prop.completed();
      return true;
    }

    CSS::Property * declaration() {
      CSS::Property * prop = css.property_pool.newElement();
      bool done = false;
      while (true) {
        // process IDENT property
        CSS::PropertyMap::iterator it = css.property_map.find(ident);
        if (it != css.property_map.end()) prop->id = it->second; else break;
        skip_blanks();

        if (token == Token::COLON        ) skip_blanks(); else break;
        if (!expression(*prop)) break;
        if (token == Token::IMPORTANT_SYM) skip_blanks();
        if (token == Token::SEMICOLON    ) skip_blanks(); // may not be optional
        done = true;
        break;
      }
      if (done) {
        return prop;
      }
      else {
        for (auto * v : prop->values) css.value_pool.deleteElement(v);
        prop->values.clear();
        css.property_pool.deleteElement(prop);
        skip_block();
        return nullptr;
      }
    }

    bool namespace_statement() {  // Not implemented yet
      skip_block();
      return true;
    }

    bool page_statement() {  // Not implemented yet
      skip_block();
      return true;

      #if 0
      skip_blanks();
      if (token == Token::COLON) {
        next_token();
        if (token == Token::IDENT) {
          skip_blanks();
        } else return false;
      }
      if (token == Token::LBRACE) {
        skip_blanks();
        while (token == Token::IDENT) {
          if (!declaration()) return false;
        }
        if (token == Token::RBRACE) skip_blanks();
        else return false;

      } else return false;
      return true;
      #endif
    }

    bool font_face_statement() {
      CSS::Selectors      sels;
      CSS::Selector     * sel = css.selector_pool.newElement();
      CSS::SelectorNode * node = css.selector_node_pool.newElement();
      node->tag = DOM::Tag::FONT_FACE;
      sel->add_selector_node(node);
      sel->compute_specificity(css.get_priority());
      sels.push_front(sel);
      skip_blanks();
      if (token == Token::LBRACE) {
        skip_blanks();
        CSS::Properties * props = properties();
        if (token == Token::RBRACE) {
          skip_blanks();
          return add_rules(sels, props);
        }
      }
      return false;
    }
    
    #if 0  // Not supported yet
    bool attrib() {
      if (token != Token::IDENT) return false;
      skip_blanks();
      if ((token == Token::EQUAL   ) ||
          (token == Token::INCLUDES) ||
          (token == Token::DASHMATCH)) {
        if (token == Token::EQUAL) {

        }
        else if (token == Token::INCLUDES) {

        }
        else { // Token::DASHMATCH

        }
        skip_blanks();
        if (token == Token::STRING) {

        }
        else if (token == Token::IDENT) {

        }
        else return false;
        skip_blanks();
      }
      if (token != Token::RBRACK) return false;
      next_token();
      return true;
    }
    #endif

    bool pseudo(CSS::SelectorNode & node) {
      if (token == Token::IDENT) {
        if (strcmp(ident, "first_child") == 0) {
          node.qualifier = CSS::Qualifier::FIRST_CHILD;
        }
        else return false;  // other ident not supported
        next_token();
      }
      else if (token == Token::FUNCTION) {
        return false; // Not supported
        // skip_blanks();
        // if (token == Token::IDENT) {
        //   skip_blanks();
        // }
        // if (token != Token::RPARENT) return false;
        // next_token();
      }
      return true;
    }

    bool sub_selector_node(CSS::SelectorNode & node) {
      for (;;) {
        if (token == Token::HASH) {
          node.add_id(std::string(name));
          next_token();
        }
        else if (token == Token::DOT) {
          next_token();
          if (token == Token::IDENT) {
            node.add_class(std::string(ident));
            next_token();
          }
        }
        else if (token == Token::LBRACK) {
          return false; // Not supported
          // skip_blanks();
          // if (!attrib()) return false;
        }
        else if (token == Token::COLON) {
          next_token();
          if (!pseudo(node)) return false;
        }
        else break;
      }
      return true;
    }

    CSS::SelectorNode * selector_node() {
      CSS::SelectorNode * node = css.selector_node_pool.newElement();
      bool done = false;
      while (true) {
        if (token == Token::IDENT) {
          DOM::Tags::const_iterator it = DOM::tags.find(ident);
          if (it != DOM::tags.end()) {
            node->set_tag(it->second);
            next_token();
          }
          else break;
        }
        else if (token == Token::STAR) {
          node->tag = DOM::Tag::ANY;
          next_token();
        }
        if ((token == Token::HASH  ) ||
            (token == Token::DOT   ) ||
            (token == Token::LBRACK) ||
            (token == Token::COLON )) {
          if (!sub_selector_node(*node)) break;
        }

        done = true;
        break;
      }
      if (done) {
        return node;
      }
      else {
        css.selector_node_pool.deleteElement(node);
        return nullptr;
      }
    }

    CSS::Selector * selector() {
      CSS::Selector     * sel = css.selector_pool.newElement();
      CSS::SelectorNode * node;
      bool done = false;
      while (true) {
        if (((node = selector_node())) == nullptr) break;
        sel->add_selector_node(node);
        bool done2 = false;
        while (true) {
          if (token == Token::WHITESPACE) skip_blanks();
          if ((token == Token::PLUS) || (token == Token::GT) || (token == Token::TILDE)) {
            Token t = token;
            skip_blanks();
            if (((node = selector_node())) == nullptr) break;
            node->op = (t == Token::GT) ? CSS::SelOp::CHILD : CSS::SelOp::ADJACENT;
            sel->add_selector_node(node);
          }
          else if ((token == Token::IDENT ) ||
                   (token == Token::STAR  ) ||
                   (token == Token::HASH  ) ||
                   (token == Token::DOT   ) ||
                   (token == Token::LBRACK) ||
                   (token == Token::COLON )) {
            if (((node = selector_node())) == nullptr) break;
            node->op = CSS::SelOp::DESCENDANT;
            sel->add_selector_node(node);
          }
          else {
            done2 = true;
            break;
          }
        }
        done = done2;
        break;
      }
      if (done) {
        if (!sel->is_empty()) {
          sel->compute_specificity(css.get_priority());
          return sel;
        }
        else {
          css.selector_pool.deleteElement(sel);
          return nullptr;
        }
      }
      else {
        css.selector_pool.deleteElement(sel);
        return nullptr;
      }
    }

    CSS::Properties * properties() {
      CSS::Properties * props = css.properties_pool.newElement();
      CSS::Property   * property;
      while (token == Token::IDENT) {
        if (((property = declaration())) != nullptr) props->push_front(property);
      }
      return props;
    }

    bool add_rules(CSS::Selectors & sels, CSS::Properties * props) {
      if (props->empty()) {
        for (auto * sel : sels) {
          for (auto * node : sel->selector_node_list) css.selector_node_pool.deleteElement(node);
        }
        css.properties_pool.deleteElement(props);
        return false;
      }
      else {
        for (auto * sel : sels) css.add_rule(sel, props);
        css.suites.push_front(props);
      }
      return true;
    }
    
    bool ruleset() {
      CSS::Selectors  selectors; // multiple selectors can be associated to a property list 
      CSS::Selector * sel;
      // skip everything until we get th beginning of a ruleset
      // while ((token != Token::END_OF_FILE) && 
      //        (token != Token::IDENT      ) &&
      //        (token != Token::HASH       ) &&
      //        (token != Token::DOT        ) &&
      //        (token != Token::LBRACK     ) &&
      //        (token != Token::COLON      )) next_token();
      if (((sel = selector())) != nullptr) selectors.push_back(sel);
      else while ((token != Token::END_OF_FILE) && 
                  (token != Token::COMMA      ) && 
                  (token != Token::LBRACE     )) next_token();
      while (token == Token::COMMA) {
        skip_blanks();
        if (((sel = selector())) != nullptr) selectors.push_back(sel);
        else while ((token != Token::END_OF_FILE) && 
                    (token != Token::COMMA      ) && 
                    (token != Token::LBRACE     )) next_token();
      }
      if (selectors.empty()) { 
        skip_block();
      }
      else if (token == Token::LBRACE) {
        skip_blanks();
        CSS::Properties * props = properties();
        if (token == Token::RBRACE) {
          skip_blanks();
          return add_rules(selectors, props);
        }
      }
      return false;
    }

    #if 0
      // This is a first cut of the @media statements. Not completed as I don't need it.

      // https://www.w3.org/TR/mediaqueries-4/#typedef-media-query-list
      //
      // @media <media-query-list> {
      //   <group-rule-body>
      // } 
      //                <media-query> = <media-condition>
      //                                | [ not | only ]? <media-type> [ and <media-condition-without-or> ]?
      //                 <media-type> = <ident>
      //            <media-condition> = <media-not> 
      //                                | <media-in-parens> [ <media-and>* | <media-or>* ]
      // <media-condition-without-or> = <media-not> 
      //                                | <media-in-parens> <media-and>*
      //                  <media-not> = not <media-in-parens>
      //                  <media-and> = and <media-in-parens>
      //                   <media-or> = or  <media-in-parens>
      //            <media-in-parens> = ( <media-condition> ) 
      //                                | <media-feature> 
      //                                | <general-enclosed>
      //
      //              <media-feature> = ( [ <mf-plain> | <mf-boolean> | <mf-range> ] )
      //                   <mf-plain> = <mf-name> : <mf-value>
      //                 <mf-boolean> = <mf-name>
      //                   <mf-range> = <mf-name> <mf-comparison> <mf-value>
      //                                | <mf-value> <mf-comparison> <mf-name>
      //                                | <mf-value> <mf-lt> <mf-name> <mf-lt> <mf-value>
      //                                | <mf-value> <mf-gt> <mf-name> <mf-gt> <mf-value>
      //                    <mf-name> = <ident>
      //                   <mf-value> = <number> 
      //                                | <dimension> 
      //                                | <ident> 
      //                                | <ratio>
      //                      <mf-lt> = '<' '='?
      //                      <mf-gt> = '>' '='?
      //                      <mf-eq> = '='
      //              <mf-comparison> = <mf-lt> 
      //                                | <mf-gt> 
      //                                | <mf-eq>
      //
      //           <general-enclosed> = [ <function-token> <any-value> ) ] 
      //                                | ( <ident> <any-value> )

      bool mf_value() {
        if (token == Token::NUMBER) {
          skip_blanks();
          if (token == Token::SLASH) {
            // ratio
            skip_blanks();
            if (token != Token::NUMBER) return false;
            skip_blanks();
          }
        }
        else if (token == Token::DIMENSION) {
          skip_blanks();
        }
        else if (token == Token::IDENT) {
          skip_blanks();
        }
        else return false;

        return true;
      }

      bool is_logical() {
        return 
          (token == Token::GT) ||
          (token == Token::GE) ||
          (token == Token::LT) ||
          (token == Token::LE);
      }

      bool is_lower(Token token) {
        return 
          (token == Token::LT) ||
          (token == Token::LE);
      }

      bool is_greater(Token token) {
        return 
          (token == Token::GT) ||
          (token == Token::GE);
      }

      bool mf_range() {
        if (!mf_value()) return false;
        if (token == Token::EQUAL) {
          skip_blanks();
          if (!mf_value()) return false;
        }
        else if (is_logical()) {
          Token op1 = token;
          skip_blanks();
          if (!mf_value()) return false;
          if (is_logical()) {
            Token op2 = token;
            skip_blanks();
            if (!mf_value()) return false;
            if ((  is_lower(op1) && is_greater(op2)) || 
                (is_greater(op1) &&   is_lower(op2))) {
              return false;
            }
          } 
        }
        else return false;
        return true;
      }

      bool media_in_parens() {
        if (token == Token::LPARENT) {
          skip_blanks();
          bool not_token_present = (token == Token::IDENT) && (strcmp((char *)ident, "not" ) == 0);
          if (not_token_present) skip_blanks();
          if ((token == Token::LPARENT) || (token == Token::FUNCTION)) {
            bool present;
            if (!media_condition(not_token_present, &present, true)) return false;
          }
          else { // media-feature
            if (token == Token::IDENT) {
              skip_blanks();
              if (token == Token::RPARENT) {
                // mf-boolean
              }
              else if (token == Token::COLON) {
                // mf-plain
                skip_blanks();
                if (!mf_value()) return false;
              }
              else if (mf_range()) {
              }
              else while ((token != Token::END_OF_FILE) && (token != Token::RPARENT)) {
                // any-values
                skip_blanks();
              }
            }
            else if (!mf_range()) return false;
          }
        }
        else { // Token::FUNCTION
          skip_blanks();
          while ((token != Token::END_OF_FILE) && (token != Token::RPARENT)) {
            // any-values
            skip_blanks();
          }
        }

        if (token == Token::RPARENT) skip_blanks();
        else return false;

        return true;
      }

      bool media_condition(bool not_token_present, bool * present, bool with_or) {
        *present = (token == Token::LPARENT) || (token == Token::FUNCTION);
        if (*present) {
          if (!media_in_parens()) return false;
          if ((token == Token::IDENT) && (strcmp((char *)ident, "and") == 0)) {
            while ((token == Token::IDENT) && (strcmp((char *)ident, "and") == 0)) {
              skip_blanks();
              if ((token != Token::LPARENT) && (token != Token::FUNCTION)) return false;
              if (!media_in_parens()) return false;
            }
          }
          else if (with_or && (token == Token::IDENT) && (strcmp((char *)ident, "or") == 0)) {
            while ((token == Token::IDENT) && (strcmp((char *)ident, "or") == 0)) {
              skip_blanks();
              if ((token != Token::LPARENT) && (token != Token::FUNCTION)) return false;
              if (!media_in_parens()) return false;
            }
          }
        }
        return true;
      }

      bool media_query(bool * query_present) {
        *query_present = false;

        bool media_type_present      = false;
        bool condition_present       = false;
        bool media_in_parens_present = false;
        bool not_token_present       = false;
        bool only_token_present      = false;

        if (token == Token::IDENT) {
          not_token_present  = strcmp((char *)ident, "not" ) == 0;
          only_token_present = strcmp((char *)ident, "only") == 0;

          if (not_token_present || only_token_present) skip_blanks();

          if (token == Token::IDENT) {
            // This is a media type
            media_type_present = true;
            skip_blanks();
            not_token_present = false;
            if ((token == Token::IDENT) && 
                (strcmp((char *)ident, "and") == 0)) {
              skip_blanks();
              if ((token == Token::IDENT) && 
                  ((not_token_present = strcmp((char *)ident, "not") == 0))) {
                skip_blanks();
              }
              if (!media_condition(not_token_present, &condition_present, false)) return false;
              if (!condition_present) return false;
            }
          }
          else {
            if (only_token_present) return false; 
            if (!media_condition(not_token_present, &condition_present, true)) return false;
          }
        }
        else if (token == Token::LPARENT) {
          if (!media_in_parens()) return false;
          media_in_parens_present = true;
        }

        *query_present = media_type_present || condition_present || media_in_parens_present;
        return true;
      }

      bool media_statement() {
        skip_blanks();
        bool query_present;
        if (!media_query(&query_present)) return false;
        if (query_present) {
          while (token == Token::COMMA) {
            skip_blanks();
            if (!media_query(&query_present)) return false;
          }
        }
  
        if (token == Token::LBRACE) {
          skip_blanks();
          while (token != Token::RBRACE) {
            if (!ruleset()) return false;
            if (token == Token::ERROR) return false;
          }
          skip_blanks();
        } else return false;
        return true;
      }
    #endif

  public:
    bool parse(const char * buffer, int32_t size) {

      str      = buffer;
      remains  = size;
      skip     = 0;
      line_nbr = 1;

      next_ch();
      next_token();

      if (token == Token::CHARSET_SYM) {
        next_token();
        if (token == Token::STRING) {
          next_token();
          if (token == Token::SEMICOLON) next_token();
          else return false;
        } else return false;
      }

      while ((token == Token::WHITESPACE) ||
             (token == Token::CDO       ) ||
             (token == Token::CDC       )) {
        if      (token == Token::CDO) skip++;
        else if (token == Token::CDC) skip--;
        next_token();
      }

      while (token == Token::IMPORT_SYM) {
        if (!import_statement()) return false;
        while ((token == Token::CDO) || 
               (token == Token::CDC)) {
          if      (token == Token::CDO) skip++;
          else if (token == Token::CDC) skip--;
          skip_blanks();
        }
      }

      bool done = false;

      while (!done) {
        if (token == Token::MEDIA_SYM) {
          //if (!media_statement()) return false;
          if (!skip_block()) return false;
        }
        else if (token == Token::PAGE_SYM) {
          if (!page_statement()) return false;
        }
        else if (token == Token::FONT_FACE_SYM) {
          if (!font_face_statement()) return false;
        }
        else if (token == Token::NAMESPACE_SYM) {
          if (!namespace_statement()) return false;
        }
        else if (token == Token::ERROR) {
          LOG_E("%s[%d]: Token ERROR retrieved!", css.get_id().c_str(), line_nbr);
          return false;
        }
        else {
          ruleset();
        }
        while ((token == Token::CDO) || 
               (token == Token::CDC)) {
          if      (token == Token::CDO) skip++;
          else if (token == Token::CDC) skip--;
          skip_blanks();
        }
        if (token == Token::END_OF_FILE) break;
      }
      return true;
    }

    bool parse(DOM::Tag tag, const char * buffer, int32_t size) {

      str      = buffer;
      remains  = size;
      skip     = 0;
      line_nbr = 1;

      next_ch();
      next_token();

      CSS::Properties * props = properties();

      if (props != nullptr) {
        CSS::Selector * sel = css.selector_pool.newElement();
        CSS::SelectorNode * node = css.selector_node_pool.newElement();
        node->set_tag(tag);
        sel->add_selector_node(node);
        sel->compute_specificity(css.get_priority());
        css.add_rule(sel, props);
      }
      return true;
    }
};

