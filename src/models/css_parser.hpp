
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

#include <cmath>
#include <cstring>

#if CSS_PARSER_TEST
  #include <iostream>
#endif

#include "css.hpp"

class CSSParser {
public:
  CSSParser(CSS &theCss, const char *buffer, int32_t size) : css(theCss), skip(0), selNbr(0) {
    parse(buffer, size);
  }

  CSSParser(CSS &theCss, DOM::Tag tag, const char *buffer, int32_t size)
      : css(theCss), skip(0), selNbr(0) {
    parse(tag, buffer, size);
  }

  ~CSSParser() {}

private:
  static constexpr char const *TAG = "CSSParser";

  CSS &css;
  uint8_t skip;
  uint8_t selNbr;

  // ---- Tokenizer ----

  static const int8_t IDENT_SIZE   = 60;
  static const int8_t NAME_SIZE    = 60;
  static const int16_t STRING_SIZE = 128;

  // clang-format off
  enum class Token : uint8_t {
    ERROR,  COMMA,   CDO,        CDC,       INCLUDES,  DASHMATCH,   STRING, BAD_STRING, 
    IDENT,  HASH,    IMPORT_SYM, PAGE_SYM,  MEDIA_SYM, CHARSET_SYM, EQUAL,  IMPORTANT_SYM, 
    LENGTH, ANGLE,   TIME,       FREQ,      DIMENSION, PERCENTAGE,  NUMBER, NAMESPACE_SYM, 
    URI,    BAD_URI, FUNCTION,   SEMICOLON, COLON,     WHITESPACE,  GT,     LT,
    GE,     LE,      MINUS,      PLUS,      DOT,       STAR,        SLASH,  FONT_FACE_SYM,
    LBRACK, RBRACK,  LBRACE,     RBRACE,    LPARENT,   RPARENT,     TILDE,  END_OF_FILE
  };

  const char *tokenNames[48] = {
    "ERROR",  "COMMA",   "CDO",        "CDC",       "INCLUDES",  "DASHMATCH",   "STRING", "BAD_STRING",
    "IDENT",  "HASH",    "IMPORT_SYM", "PAGE_SYM",  "MEDIA_SYM", "CHARSET_SYM", "EQUAL",  "IMPORTANT_SYM",
    "LENGTH", "ANGLE",   "TIME",       "FREQ",      "DIMENSION", "PERCENTAGE",  "NUMBER", "NAMESPACE_SYM",
    "URI",    "BAD_URI", "FUNCTION",   "SEMICOLON", "COLON",     "WHITESPACE",  "GT",     "LT",
    "GE",     "LE",      "MINUS",      "PLUS",      "DOT",       "STAR",        "SLASH",  "FONT_FACE_SYM",
    "LBRACK", "RBRACK",  "LBRACE",     "RBRACE",    "LPARENT",   "RPARENT",     "TILDE",  "END_OF_FILE"};

  // clang-format on

  int32_t remains; // number of bytes remaining in the css buffer
  const char *str; // pointer in the css buffer
  int16_t lineNbr;
  uint8_t ch; // next character to be processed

  char ident[IDENT_SIZE];
  char string[STRING_SIZE];
  char name[NAME_SIZE];

  float num;

  Token token;

  CSS::ValueType valueType;

  auto nextCh() -> void {
    if (remains > 0) {
      remains--;
      ch = *str++;
      if (ch == '\n') lineNbr++;
    } else
      ch = '\0';
  }

 [[nodiscard]] inline auto isWhiteSpace() -> bool {
    return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n') || (ch == '\f');
  }

 [[nodiscard]] inline auto isNmChar() -> bool {
    return ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) ||
           ((ch >= '0') && (ch <= '9')) || (ch == '_') || (ch == '-') || (ch == '\\') ||
           (ch >= 160);
  }

  auto isNmStart(uint8_t c) -> bool {
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || (c == '_') || (c == '-') ||
           (c == '\\') || (c >= 160);
  }

  auto skipSpaces() -> void {
    while (isWhiteSpace()) nextCh();
  }

  auto parseUrl() -> bool {
    int16_t idx = 0;
    while ((ch > ' ') && (ch != ')') && (idx < (STRING_SIZE - 1))) {
      if ((ch == '"') || (ch == '\'') || (ch == '(')) {
        return false;
      }
      if (ch == '\\') {
        nextCh();
        if (ch == '\0') return false;
        if (ch == '\r') {
          string[idx++] = '\n';
          nextCh();
          if (ch == '\n') nextCh();
        } else {
          string[idx++] = ch;
          nextCh();
        }
      } else {
        string[idx++] = ch;
        nextCh();
      }
    }
    string[idx] = 0;
    return true;
  }

  auto parseString() -> bool {
    char delim  = ch;
    int16_t idx = 0;

    nextCh();
    while ((ch != '\0') && (ch != delim) && (idx < (STRING_SIZE - 1))) {
      if (ch == '\\') {
        nextCh();
        if (ch == '\0') return false;
        if (ch == '\r') {
          string[idx++] = '\n';
          nextCh();
          if (ch == '\n') nextCh();
        } else {
          string[idx++] = ch;
          nextCh();
        }
      } else if ((ch >= ' ') || (ch == '\t')) {
        string[idx++] = ch;
        nextCh();
      } else {
        nextCh();
      }
    }
    string[idx] = 0;
    bool res    = ch == delim;
    if (res) nextCh();
    return res;
  }

  auto parseNumber() -> void {
    num      = 0;
    bool neg = ch == '-';
    if (neg)
      nextCh();
    else if (ch == '+')
      nextCh();

    float dec = 0.1;
    while ((ch >= '0') && (ch <= '9')) {
      num = (num * 10) + (ch - '0');
      nextCh();
    }
    if (ch == '.') {
      nextCh();
      while ((ch >= '0') && (ch <= '9')) {
        num = num + (dec * (ch - '0'));
        dec = dec * 0.1;
        nextCh();
      }
    }

    if (neg) num = -num;

    if (((ch == 'e') || (ch == 'E')) && ((str[0] >= '0') && (str[0] <= '9'))) {
      float exp = 0.0;
      nextCh();
      neg = ch == '-';
      if (neg)
        nextCh();
      else if (ch == '+')
        nextCh();
      while ((ch >= '0') && (ch <= '9')) {
        exp = (exp * 10) + (ch - '0');
        nextCh();
      }
      if (exp > 0) {
        if (neg) exp = -exp;
        num = num * pow(10.0f, exp);
      }
    }
  }

  auto parseName() -> void {
    int8_t idx = 0;
    while ((ch != '\0') && isNmChar() && (idx < (NAME_SIZE - 1))) {
      if (ch == '\\') {
        nextCh();
        if (ch >= ' ') {
          name[idx++] = ch;
          nextCh();
        } else
          break;
      } else {
        name[idx++] = ch;
        nextCh();
      }
    }
    if (idx < NAME_SIZE) name[idx] = 0;
  }

  auto parseIdent() -> void {
    int8_t idx = 0;
    while ((ch != '\0') && isNmChar() && (idx < (NAME_SIZE - 1))) {
      if (ch == '\\') {
        nextCh();
        if (ch >= ' ') {
          ident[idx++] = ch;
          nextCh();
        } else
          break;
      } else {
        ident[idx++] = ch;
        nextCh();
      }
    }
    if (idx < IDENT_SIZE) ident[idx] = 0;
    #if CSS_PARSER_TEST
      std::cout << "Ident: " << ident << std::endl;
    #endif
  }

  auto skipComment() -> bool {
    for (;;) {
      if ((ch == '*') && (str[0] == '/')) {
        nextCh();
        nextCh();
        break;
      } else if (ch == '\0')
        return false;
      nextCh();
    }
    return true;
  }

  auto nextToken() -> void {
    bool done = false;
    while (!done) {
      if (ch == '\0')
        token = Token::END_OF_FILE;
      else if (isWhiteSpace()) {
        nextCh();
        token = Token::WHITESPACE;
      } else if (ch == ';') {
        nextCh();
        token = Token::SEMICOLON;
      } else if (ch == ':') {
        nextCh();
        token = Token::COLON;
      } else if (ch == ',') {
        nextCh();
        token = Token::COMMA;
      } else if (ch == '{') {
        nextCh();
        token = Token::LBRACE;
      } else if (ch == '}') {
        nextCh();
        token = Token::RBRACE;
      } else if (ch == '[') {
        nextCh();
        token = Token::LBRACK;
      } else if (ch == ']') {
        nextCh();
        token = Token::RBRACK;
      } else if (ch == '(') {
        nextCh();
        token = Token::LPARENT;
      } else if (ch == ')') {
        nextCh();
        token = Token::RPARENT;
      } else if (ch == '=') {
        nextCh();
        token = Token::EQUAL;
      } else if (ch == '*') {
        nextCh();
        token = Token::STAR;
      } else if ((ch == '\'') || (ch == '\"')) {
        token      = parseString() ? Token::STRING : Token::BAD_STRING;
        valueType = CSS::ValueType::STR;
      } else if ((((ch == '-') || (ch == '+')) &&
                  ((str[0] == '.') || ((str[0] >= '0') && (str[0] <= '9')))) ||
                 ((ch >= '0') && (ch <= '9')) ||
                 ((ch == '.') && ((str[0] >= '0') && (str[0] <= '9')))) {
        parseNumber();
        token      = Token::NUMBER;
        valueType = CSS::ValueType::NO_TYPE;
        if ((ch == 'e') && (str[0] == 'm')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::EM;
        } else if ((ch == 'e') && (str[0] == 'x')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::EX;
        } else if (ch == '%') {
          nextCh();
          token      = Token::PERCENTAGE;
          valueType = CSS::ValueType::PERCENT;
        } else if ((ch == 'p') && (str[0] == 'x')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::PX;
        } else if ((ch == 'p') && (str[0] == 't')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::PT;
        } else if ((ch == 'p') && (str[0] == 'c')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::PC;
        } else if ((ch == 'c') && (str[0] == 'm')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::CM;
        } else if ((ch == 'm') && (str[0] == 'm')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::MM;
        } else if ((ch == 'i') && (str[0] == 'n')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::IN;
        } else if ((ch == 'v') && (str[0] == 'h')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::VH;
        } else if ((ch == 'v') && (str[0] == 'w')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::VW;
        } else if ((ch == 'c') && (str[0] == 'h')) {
          nextCh();
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::CH;
        } else if ((ch == 'm') && (str[0] == 's')) {
          nextCh();
          nextCh();
          token      = Token::TIME;
          valueType = CSS::ValueType::MSEC;
        } else if (ch == 's') {
          nextCh();
          token      = Token::TIME;
          valueType = CSS::ValueType::SEC;
        } else if ((ch == 'h') && (str[0] == 'z')) {
          nextCh();
          nextCh();
          token      = Token::FREQ;
          valueType = CSS::ValueType::HERTZ;
        } else if ((ch == 'k') && (str[0] == 'h') && (str[1] == 'z')) {
          remains -= 2;
          str += 2;
          nextCh();
          token      = Token::FREQ;
          valueType = CSS::ValueType::KHERTZ;
        } else if ((ch == 'd') && (str[0] == 'e') && (str[1] == 'g')) {
          remains -= 2;
          str += 2;
          nextCh();
          token      = Token::ANGLE;
          valueType = CSS::ValueType::DEG;
        } else if ((ch == 'r') && (str[0] == 'a') && (str[1] == 'd')) {
          remains -= 2;
          str += 2;
          nextCh();
          token      = Token::ANGLE;
          valueType = CSS::ValueType::RAD;
        } else if ((ch == 'r') && (str[0] == 'e') && (str[1] == 'm')) {
          remains -= 2;
          str += 2;
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::REM;
        } else if ((ch == 'g') && (str[0] == 'r') && (str[1] == 'a') && (str[2] == 'd')) {
          remains -= 3;
          str += 3;
          nextCh();
          token      = Token::ANGLE;
          valueType = CSS::ValueType::GRAD;
        } else if ((ch == 'v') && (str[0] == 'm') && (str[1] == 'i') && (str[2] == 'n')) {
          remains -= 3;
          str += 3;
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::VMIN;
        } else if ((ch == 'v') && (str[0] == 'm') && (str[1] == 'a') && (str[2] == 'x')) {
          remains -= 3;
          str += 3;
          nextCh();
          token      = Token::LENGTH;
          valueType = CSS::ValueType::VMAX;
        } else if (isNmStart(ch)) {
          parseIdent();
          token      = Token::DIMENSION;
          valueType = CSS::ValueType::DIMENSION;
        }
      } else if ((ch == '-') && (str[0] == '-') && (str[1] == '>')) {
        token = Token::CDC;
        remains -= 2;
        str += 2;
        nextCh();
      } else if (isNmStart(ch) || ((ch == '-') && (isNmStart(str[0])))) {
        parseIdent();
        token = Token::IDENT;
        if (ch == '(') {
          nextCh();
          if (strcmp((char *)ident, "url") == 0) {
            skipSpaces();
            if ((ch == '"') || (ch == '\'')) {
              token = parseString() ? Token::URI : Token::BAD_URI;
            } else {
              token = parseUrl() ? Token::URI : Token::BAD_URI;
            }
            skipSpaces();
            if (ch == ')') {
              nextCh();
            } else {
              token = Token::BAD_URI;
            }
          } else {
            token = Token::FUNCTION;
          }
        }
      } else if ((ch == '<') && (strncmp((char *)str, "!--", 3) == 0)) {
        token = Token::CDO;
        remains -= 3;
        str += 3;
        nextCh();
      } else if ((ch == '>') && (str[0] == '=')) {
        nextCh();
        nextCh();
        token = Token::GE;
      } else if ((ch == '<') && (str[0] == '=')) {
        nextCh();
        nextCh();
        token = Token::LE;
      } else if (ch == '+') {
        nextCh();
        token = Token::PLUS;
      } else if (ch == '-') {
        nextCh();
        token = Token::MINUS;
      } else if (ch == '.') {
        nextCh();
        token = Token::DOT;
      } else if (ch == '>') {
        nextCh();
        token = Token::GT;
      } else if (ch == '<') {
        nextCh();
        token = Token::LT;
      } else if (ch == '#') {
        nextCh();
        parseName();
        token = Token::HASH;
      } else if (ch == '@') {
        if (strncmp((char *)str, "media", 5) == 0) {
          token = Token::MEDIA_SYM;
          remains -= 5;
          str += 5;
          nextCh();
        } else if (strncmp((char *)str, "page", 4) == 0) {
          token = Token::PAGE_SYM;
          remains -= 4;
          str += 4;
          nextCh();
        } else if (strncmp((char *)str, "import", 6) == 0) {
          token = Token::IMPORT_SYM;
          remains -= 6;
          str += 6;
          nextCh();
        } else if (strncmp((char *)str, "charset ", 8) == 0) {
          token = Token::CHARSET_SYM;
          remains -= 8;
          str += 8;
          nextCh();
        } else if (strncmp((char *)str, "font-face", 9) == 0) {
          token = Token::FONT_FACE_SYM;
          remains -= 9;
          str += 9;
          nextCh();
        } else if (strncmp((char *)str, "namespace ", 10) == 0) {
          token = Token::NAMESPACE_SYM;
          remains -= 10;
          str += 10;
          nextCh();
        } else {
          LOG_E("%s[%d]: Sym Token Unknown: @%s", css.getId().c_str(), lineNbr, str);
          token = Token::ERROR;
        }
      } else if (ch == '~') {
        nextCh();
        if (ch == '=') {
          nextCh();
          token = Token::INCLUDES;
        } else {
          token = Token::TILDE;
        }
      } else if (ch == '|') {
        nextCh();
        if (ch == '=') {
          nextCh();
          token = Token::DASHMATCH;
        } else {
          LOG_E("%s[%d]: Unknown usage: %c", css.getId().c_str(), lineNbr, ch);
          token = Token::ERROR;
        }
      } else if ((ch == '/') && (str[0] == '*')) {
        nextCh();
        nextCh();
        if (!skipComment()) {
          LOG_E("%s[%d]: Non terminated comment.", css.getId().c_str(), lineNbr);
          token = Token::ERROR;
        } else
          continue;
      } else if (ch == '/') {
        nextCh();
        token = Token::SLASH;
      } else if (ch == '!') {
        nextCh();
        for (;;) {
          skipSpaces();
          if ((ch == '/') && (str[0] == '*')) {
            nextCh();
            nextCh();
            if (!skipComment()) {
              LOG_E("%s[%d]: Non terminated comment.", css.getId().c_str(), lineNbr);
              token = Token::ERROR;
              done  = true;
              break;
            } else
              continue;
          } else
            break;
        }
        if (!done && (ch == 'i') && (strncmp((char *)str, "mportant", 8) == 0)) {
          remains -= 8;
          str += 8;
          nextCh();
          token = Token::IMPORTANT_SYM;
        } else if (!done) {
          LOG_E("%s[%d]: '!' not followed by 'important'", css.getId().c_str(), lineNbr);
          token = Token::ERROR;
        }
      } else {
        LOG_E("%s[%d]: Unknown usage: %c", css.getId().c_str(), lineNbr, ch);
        token = Token::ERROR;
      }

      done = true;
    }

    #if CSS_PARSER_TEST
      std::cout << '[' << tokenNames[(int)token] << ']' << std::flush;
    #endif
  }

  // ---- Parser ----

  /**
   * Skip all blanks
   *
   * On entry, the current token must point to an already parsed item.
   * On return, the token point at the next valid token passed whitespaces.
   */
  auto skipBlanks() -> void {
    if (token == Token::END_OF_FILE) return;
    do nextToken();
    while (token == Token::WHITESPACE);
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
  auto skipBlock() -> bool {

    while ((token != Token::END_OF_FILE) && (token != Token::SEMICOLON) &&
           (token != Token::LBRACE) && (token != Token::RBRACE))
      nextToken();
    if (token == Token::SEMICOLON) {
      skipBlanks();
    } else if (token == Token::LBRACE) {
      skipBlanks();
      int braceCount = 1;
      while (braceCount > 0) {
        if (token == Token::END_OF_FILE) return false;
        if (token == Token::LBRACE) {
          braceCount += 1;
          skipBlanks();
        } else if (token == Token::RBRACE) {
          braceCount -= 1;
          skipBlanks();
        } else {
          nextToken();
        }
      }
    } else if (token != Token::RBRACE) {
      return false;
    }

    return true;
  }

  auto importStatement() -> bool {
    skipBlanks();
    if ((token == Token::URI) || (token == Token::STRING)) {
      // Import a css file
    }
    skipBlanks();
    if (token == Token::IDENT) {
      skipBlanks();
      while (token == Token::COMMA) {
        skipBlanks();
        if (token != Token::IDENT) return false;
        skipBlanks();
      }
    }
    if (token == Token::SEMICOLON) {
      skipBlanks();
    } else
      return false;
    return true;
  }

    #if 0
    auto function()  -> bool{
      skipBlanks();
      expression();
      if (token == Token::RPARENT) skipBlanks();
      else return false;
      return true;
    }
  #endif

  CSS::Value *term(bool &none, CSS::PropertyId id) {
    none          = false;
    bool neg      = false;
    CSS::Value *v = css.valuePool.newElement();
    bool done     = false;
    while (true) {
      if ((token == Token::PLUS) || (token == Token::MINUS)) {
        neg = token == Token::MINUS;
        skipBlanks();
      }
      if ((token == Token::NUMBER) || (token == Token::PERCENTAGE) || (token == Token::LENGTH) ||
          (token == Token::ANGLE) || (token == Token::TIME) || (token == Token::DIMENSION) ||
          (token == Token::FREQ)) {
        v->valueType = valueType;
        v->num        = neg ? -num : num;
        if (id == CSS::PropertyId::VERTICAL_ALIGN)
          v->choice.verticalAlign = CSS::VerticalAlign::VALUE;
        skipBlanks();
      } else if (token == Token::STRING) {
        v->valueType = CSS::ValueType::STR;
        v->str        = string;
        skipBlanks();
      } else if (token == Token::IDENT) {
        v->str        = ident;
        v->valueType = CSS::ValueType::STR;
        if (id == CSS::PropertyId::TEXT_ALIGN) {
          if (v->str.compare("left") == 0)
            v->choice.align = CSS::Align::LEFT;
          else if (v->str.compare("center") == 0)
            v->choice.align = CSS::Align::CENTER;
          else if (v->str.compare("right") == 0)
            v->choice.align = CSS::Align::RIGHT;
          else if (v->str.compare("justify") == 0)
            v->choice.align = CSS::Align::JUSTIFY;
          else if (v->str.compare("justified") == 0)
            v->choice.align = CSS::Align::JUSTIFY;
          else {
            // LOG_E("text-align not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str -
            // buffer_start));
            break;
          }
        } else if (id == CSS::PropertyId::TEXT_TRANSFORM) {
          if (v->str.compare("lowercase") == 0)
            v->choice.textTransform = CSS::TextTransform::LOWERCASE;
          else if (v->str.compare("uppercase") == 0)
            v->choice.textTransform = CSS::TextTransform::UPPERCASE;
          else if (v->str.compare("capitalize") == 0)
            v->choice.textTransform = CSS::TextTransform::CAPITALIZE;
          else if (v->str.compare("none") == 0)
            v->choice.textTransform = CSS::TextTransform::NONE;
          else {
            // LOG_E("text-transform not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str
            // - buffer_start));
            break;
          }
        } else if (id == CSS::PropertyId::VERTICAL_ALIGN) {
          if (v->str.compare("sub") == 0)
            v->choice.verticalAlign = CSS::VerticalAlign::SUB;
          else if (v->str.compare("super") == 0)
            v->choice.verticalAlign = CSS::VerticalAlign::SUPER;
          else if (v->str.compare("top") == 0)
            v->choice.verticalAlign = CSS::VerticalAlign::SUPER;
          else if (v->str.compare("text-top") == 0)
            v->choice.verticalAlign = CSS::VerticalAlign::SUPER;
          else {
            v->choice.verticalAlign = CSS::VerticalAlign::NORMAL;
          }
        } else if (id == CSS::PropertyId::FONT_WEIGHT) {
          if ((v->str.compare("bold") == 0) || (v->str.compare("bolder") == 0))
            v->choice.faceStyle = Fonts::FaceStyle::BOLD;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0))
            v->choice.faceStyle = Fonts::FaceStyle::NORMAL;
          else {
            // LOG_E("font-weight not decoded: '%s' at offset: %d", v->str.c_str(), (int32_t)(str -
            // buffer_start));
            break;
          }
        } else if (id == CSS::PropertyId::FONT_STYLE) {
          if ((v->str.compare("italic") == 0) || (v->str.compare("oblique") == 0))
            v->choice.faceStyle = Fonts::FaceStyle::ITALIC;
          else if ((v->str.compare("normal") == 0) || (v->str.compare("initial") == 0))
            v->choice.faceStyle = Fonts::FaceStyle::NORMAL;
          else {
            // LOG_E("font-style not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str -
            // buffer_start));
            break;
          }
        } else if (id == CSS::PropertyId::DISPLAY) {
          if (v->str.compare("none") == 0)
            v->choice.display = CSS::Display::NONE;
          else if (v->str.compare("inline") == 0)
            v->choice.display = CSS::Display::INLINE;
          else if (v->str.compare("block") == 0)
            v->choice.display = CSS::Display::BLOCK;
          else if (v->str.compare("inline-block") == 0)
            v->choice.display = CSS::Display::INLINE_BLOCK;
          else {
            // LOG_E("display not decoded: '%s' at offset: %d", w.c_str(), (int32_t)(str -
            // buffer_start));
            break;
          }
        } else if ((id == CSS::PropertyId::FONT_SIZE) && (v->valueType == CSS::ValueType::STR)) {
          v->valueType                  = CSS::ValueType::PT;
          CSS::FontSizeMap::iterator it = css.fontSizeMap.find(static_cast<HimemString>(v->str));
          if (it != css.fontSizeMap.end()) {
            v->num = it->second;
          } else {
            // int8_t fontSize;
            // config.get(Config::Ident::FONT_SIZE, &fontSize);
            // v->num = fontSize;
            v->num = 12;
          }
        } else if (v->str.compare("inherit") == 0) {
          v->valueType = CSS::ValueType::INHERIT;
        }
        skipBlanks();
      } else if (token == Token::URI) {
        v->str        = string;
        v->valueType = CSS::ValueType::URL;
        skipBlanks();
      } else if (token == Token::FUNCTION) {
        // if (!function()) return false; // Not supported yet
        break;
      } else if (token == Token::HASH) {
        // v->str = name;
        skipBlanks();
        break; // Not supported yet
      } else {
        none = true;
        break;
      }

      done = true;
      break;
    }

    if (done) {
      return v;
    } else {
      css.valuePool.deleteElement(v);
      return nullptr;
    }
  }

  auto expression(CSS::Property &prop) -> bool {
    CSS::Value *v;
    bool none;
    if ((((v = term(none, prop.id))) == nullptr) || none) return false;
    prop.addValue(v);
    for (;;) {
      if ((token == Token::SLASH) || (token == Token::COMMA)) {
        skipBlanks();
      }
      if ((((v = term(none, prop.id))) == nullptr) && !none) return false;
      if (none) break;
      prop.addValue(v);
    }
    prop.completed();
    return true;
  }

  CSS::Property *declaration() {
    CSS::Property *prop = css.propertyPool.newElement();
    bool done           = false;
    while (true) {
      // process IDENT property
      CSS::PropertyMap::iterator it = css.propertyMap.find(ident);
      if (it != css.propertyMap.end())
        prop->id = it->second;
      else
        break;
      skipBlanks();

      if (token == Token::COLON)
        skipBlanks();
      else
        break;
      if (!expression(*prop)) break;
      if (token == Token::IMPORTANT_SYM) skipBlanks();
      if (token == Token::SEMICOLON) skipBlanks(); // may not be optional
      done = true;
      break;
    }
    if (done) {
      return prop;
    } else {
      for (auto *v : prop->values) css.valuePool.deleteElement(v);
      prop->values.clear();
      css.propertyPool.deleteElement(prop);
      skipBlock();
      return nullptr;
    }
  }

  auto namespaceStatement() -> bool { // Not implemented yet
    skipBlock();
    return true;
  }

  auto pageStatement() -> bool { // Not implemented yet
    skipBlock();
    return true;

      #if 0
      skipBlanks();
      if (token == Token::COLON) {
        nextToken();
        if (token == Token::IDENT) {
          skipBlanks();
        } else return false;
      }
      if (token == Token::LBRACE) {
        skipBlanks();
        while (token == Token::IDENT) {
          if (!declaration()) return false;
        }
        if (token == Token::RBRACE) skipBlanks();
        else return false;

      } else return false;
      return true;
    #endif
  }

  auto fontFaceStatement() -> bool {
    CSS::Selectors sels;
    CSS::Selector *sel      = css.selectorPool.newElement();
    CSS::SelectorNode *node = css.selectorNodePool.newElement();
    node->tag               = DOM::Tag::FONT_FACE;
    sel->addSelectorNode(node);
    sel->computeSpecificity(css.getPriority());
    sels.push_front(sel);
    skipBlanks();
    if (token == Token::LBRACE) {
      skipBlanks();
      CSS::Properties *props = properties();
      if (token == Token::RBRACE) {
        skipBlanks();
        return addRules(sels, props);
      }
    }
    return false;
  }
    
    #if 0  // Not supported yet
    auto attrib()  -> bool{
      if (token != Token::IDENT) return false;
      skipBlanks();
      if ((token == Token::EQUAL   ) ||
          (token == Token::INCLUDES) ||
          (token == Token::DASHMATCH)) {
        if (token == Token::EQUAL) {

        }
        else if (token == Token::INCLUDES) {

        }
        else { // Token::DASHMATCH

        }
        skipBlanks();
        if (token == Token::STRING) {

        }
        else if (token == Token::IDENT) {

        }
        else return false;
        skipBlanks();
      }
      if (token != Token::RBRACK) return false;
      nextToken();
      return true;
    }
  #endif

  auto pseudo(CSS::SelectorNode &node) -> bool {
    if (token == Token::IDENT) {
      if (strcmp(ident, "first-child") == 0) {
        node.qualifier = CSS::Qualifier::FIRST_CHILD;
      } else
        return false; // other ident not supported
      nextToken();
    } else if (token == Token::FUNCTION) {
      return false; // Not supported
      // skipBlanks();
      // if (token == Token::IDENT) {
      //   skipBlanks();
      // }
      // if (token != Token::RPARENT) return false;
      // nextToken();
    }
    return true;
  }

  auto subSelectorNode(CSS::SelectorNode &node) -> bool {
    for (;;) {
      if (token == Token::HASH) {
        node.addId(std::string(name));
        nextToken();
      } else if (token == Token::DOT) {
        nextToken();
        if (token == Token::IDENT) {
          node.addClass(std::string(ident));
          nextToken();
        }
      } else if (token == Token::LBRACK) {
        return false; // Not supported
        // skipBlanks();
        // if (!attrib()) return false;
      } else if (token == Token::COLON) {
        nextToken();
        if (!pseudo(node)) return false;
      } else
        break;
    }
    return true;
  }

  CSS::SelectorNode *selectorNode() {
    CSS::SelectorNode *node = css.selectorNodePool.newElement();
    bool done               = false;
    while (true) {
      if (token == Token::IDENT) {
        DOM::Tags::const_iterator it = DOM::tags.find(ident);
        if (it != DOM::tags.end()) {
          node->setTag(it->second);
          nextToken();
        } else
          break;
      } else if (token == Token::STAR) {
        node->tag = DOM::Tag::ANY;
        nextToken();
      }
      if ((token == Token::HASH) || (token == Token::DOT) || (token == Token::LBRACK) ||
          (token == Token::COLON)) {
        if (!subSelectorNode(*node)) break;
      }

      done = true;
      break;
    }
    if (done) {
      return node;
    } else {
      css.selectorNodePool.deleteElement(node);
      return nullptr;
    }
  }

  CSS::Selector *selector() {
    CSS::Selector *sel = css.selectorPool.newElement();
    CSS::SelectorNode *node;
    bool done = false;
    while (true) {
      if (((node = selectorNode())) == nullptr) break;
      sel->addSelectorNode(node);
      bool done2 = false;
      while (true) {
        if (token == Token::WHITESPACE) skipBlanks();
        if ((token == Token::PLUS) || (token == Token::GT) || (token == Token::TILDE)) {
          Token t = token;
          skipBlanks();
          if (((node = selectorNode())) == nullptr) break;
          node->op = (t == Token::GT) ? CSS::SelOp::CHILD : CSS::SelOp::ADJACENT;
          sel->addSelectorNode(node);
        } else if ((token == Token::IDENT) || (token == Token::STAR) || (token == Token::HASH) ||
                   (token == Token::DOT) || (token == Token::LBRACK) || (token == Token::COLON)) {
          if (((node = selectorNode())) == nullptr) break;
          node->op = CSS::SelOp::DESCENDANT;
          sel->addSelectorNode(node);
        } else {
          done2 = true;
          break;
        }
      }
      done = done2;
      break;
    }
    if (done) {
      if (!sel->isEmpty()) {
        sel->computeSpecificity(css.getPriority());
        return sel;
      } else {
        css.selectorPool.deleteElement(sel);
        return nullptr;
      }
    } else {
      css.selectorPool.deleteElement(sel);
      return nullptr;
    }
  }

  CSS::Properties *properties() {
    CSS::Properties *props = css.propertiesPool.newElement();
    CSS::Property *property;
    while (token == Token::IDENT) {
      if (((property = declaration())) != nullptr) props->push_front(property);
    }
    return props;
  }

  auto addRules(CSS::Selectors &sels, CSS::Properties *props) -> bool {
    if (props->empty()) {
      for (auto *sel : sels) {
        for (auto *node : sel->selectorNodeList) css.selectorNodePool.deleteElement(node);
      }
      css.propertiesPool.deleteElement(props);
      return false;
    } else {
      for (auto *sel : sels) css.addRule(sel, props);
      css.suites.push_front(props);
    }
    return true;
  }

  auto ruleset() -> bool {
    CSS::Selectors selectors; // multiple selectors can be associated to a property list
    CSS::Selector *sel;
    // skip everything until we get th beginning of a ruleset
    // while ((token != Token::END_OF_FILE) &&
    //        (token != Token::IDENT      ) &&
    //        (token != Token::HASH       ) &&
    //        (token != Token::DOT        ) &&
    //        (token != Token::LBRACK     ) &&
    //        (token != Token::COLON      )) nextToken();
    if (((sel = selector())) != nullptr)
      selectors.push_back(sel);
    else
      while ((token != Token::END_OF_FILE) && (token != Token::COMMA) && (token != Token::LBRACE))
        nextToken();
    while (token == Token::COMMA) {
      skipBlanks();
      if (((sel = selector())) != nullptr)
        selectors.push_back(sel);
      else
        while ((token != Token::END_OF_FILE) && (token != Token::COMMA) && (token != Token::LBRACE))
          nextToken();
    }
    if (selectors.empty()) {
      skipBlock();
    } else if (token == Token::LBRACE) {
      skipBlanks();
      CSS::Properties *props = properties();
      if (token == Token::RBRACE) {
        skipBlanks();
        return addRules(selectors, props);
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

      auto mfValue()  -> bool{
        if (token == Token::NUMBER) {
          skipBlanks();
          if (token == Token::SLASH) {
            // ratio
            skipBlanks();
            if (token != Token::NUMBER) return false;
            skipBlanks();
          }
        }
        else if (token == Token::DIMENSION) {
          skipBlanks();
        }
        else if (token == Token::IDENT) {
          skipBlanks();
        }
        else return false;

        return true;
      }

      auto isLogical()  -> bool{
        return 
          (token == Token::GT) ||
          (token == Token::GE) ||
          (token == Token::LT) ||
          (token == Token::LE);
      }

      auto isLower(Token token)  -> bool{
        return 
          (token == Token::LT) ||
          (token == Token::LE);
      }

      auto isGreater(Token token)  -> bool{
        return 
          (token == Token::GT) ||
          (token == Token::GE);
      }

      auto mfRange()  -> bool{
        if (!mfValue()) return false;
        if (token == Token::EQUAL) {
          skipBlanks();
          if (!mfValue()) return false;
        }
        else if (isLogical()) {
          Token op1 = token;
          skipBlanks();
          if (!mfValue()) return false;
          if (isLogical()) {
            Token op2 = token;
            skipBlanks();
            if (!mfValue()) return false;
            if ((  isLower(op1) && isGreater(op2)) || 
                (isGreater(op1) &&   isLower(op2))) {
              return false;
            }
          } 
        }
        else return false;
        return true;
      }

      auto mediaInParens()  -> bool{
        if (token == Token::LPARENT) {
          skipBlanks();
          bool notTokenPresent = (token == Token::IDENT) && (strcmp((char *)ident, "not" ) == 0);
          if (notTokenPresent) skipBlanks();
          if ((token == Token::LPARENT) || (token == Token::FUNCTION)) {
            bool present;
            if (!mediaCondition(notTokenPresent, &present, true)) return false;
          }
          else { // media-feature
            if (token == Token::IDENT) {
              skipBlanks();
              if (token == Token::RPARENT) {
                // mf-boolean
              }
              else if (token == Token::COLON) {
                // mf-plain
                skipBlanks();
                if (!mfValue()) return false;
              }
              else if (mfRange()) {
              }
              else while ((token != Token::END_OF_FILE) && (token != Token::RPARENT)) {
                // any-values
                skipBlanks();
              }
            }
            else if (!mfRange()) return false;
          }
        }
        else { // Token::FUNCTION
          skipBlanks();
          while ((token != Token::END_OF_FILE) && (token != Token::RPARENT)) {
            // any-values
            skipBlanks();
          }
        }

        if (token == Token::RPARENT) skipBlanks();
        else return false;

        return true;
      }

      auto mediaCondition(bool notTokenPresent, bool * present, bool withOr)  -> bool{
        *present = (token == Token::LPARENT) || (token == Token::FUNCTION);
        if (*present) {
          if (!mediaInParens()) return false;
          if ((token == Token::IDENT) && (strcmp((char *)ident, "and") == 0)) {
            while ((token == Token::IDENT) && (strcmp((char *)ident, "and") == 0)) {
              skipBlanks();
              if ((token != Token::LPARENT) && (token != Token::FUNCTION)) return false;
              if (!mediaInParens()) return false;
            }
          }
          else if (withOr && (token == Token::IDENT) && (strcmp((char *)ident, "or") == 0)) {
            while ((token == Token::IDENT) && (strcmp((char *)ident, "or") == 0)) {
              skipBlanks();
              if ((token != Token::LPARENT) && (token != Token::FUNCTION)) return false;
              if (!mediaInParens()) return false;
            }
          }
        }
        return true;
      }

      auto mediaQuery(bool * queryPresent)  -> bool{
        *queryPresent = false;

        bool mediaTypePresent      = false;
        bool conditionPresent       = false;
        bool mediaInParensPresent = false;
        bool notTokenPresent       = false;
        bool onlyTokenPresent      = false;

        if (token == Token::IDENT) {
          notTokenPresent  = strcmp((char *)ident, "not" ) == 0;
          onlyTokenPresent = strcmp((char *)ident, "only") == 0;

          if (notTokenPresent || onlyTokenPresent) skipBlanks();

          if (token == Token::IDENT) {
            // This is a media type
            mediaTypePresent = true;
            skipBlanks();
            notTokenPresent = false;
            if ((token == Token::IDENT) && 
                (strcmp((char *)ident, "and") == 0)) {
              skipBlanks();
              if ((token == Token::IDENT) && 
                  ((notTokenPresent = strcmp((char *)ident, "not") == 0))) {
                skipBlanks();
              }
              if (!mediaCondition(notTokenPresent, &conditionPresent, false)) return false;
              if (!conditionPresent) return false;
            }
          }
          else {
            if (onlyTokenPresent) return false; 
            if (!mediaCondition(notTokenPresent, &conditionPresent, true)) return false;
          }
        }
        else if (token == Token::LPARENT) {
          if (!mediaInParens()) return false;
          mediaInParensPresent = true;
        }

        *queryPresent = mediaTypePresent || conditionPresent || mediaInParensPresent;
        return true;
      }

      auto mediaStatement()  -> bool{
        skipBlanks();
        bool queryPresent;
        if (!mediaQuery(&queryPresent)) return false;
        if (queryPresent) {
          while (token == Token::COMMA) {
            skipBlanks();
            if (!mediaQuery(&queryPresent)) return false;
          }
        }
  
        if (token == Token::LBRACE) {
          skipBlanks();
          while (token != Token::RBRACE) {
            if (!ruleset()) return false;
            if (token == Token::ERROR) return false;
          }
          skipBlanks();
        } else return false;
        return true;
      }
  #endif

public:
  auto parse(const char *buffer, int32_t size) -> bool {

    str      = buffer;
    remains  = size;
    skip     = 0;
    lineNbr = 1;

    nextCh();
    nextToken();

    if (token == Token::CHARSET_SYM) {
      nextToken();
      if (token == Token::STRING) {
        nextToken();
        if (token == Token::SEMICOLON)
          nextToken();
        else
          return false;
      } else
        return false;
    }

    while ((token == Token::WHITESPACE) || (token == Token::CDO) || (token == Token::CDC)) {
      if (token == Token::CDO)
        skip++;
      else if (token == Token::CDC)
        skip--;
      nextToken();
    }

    while (token == Token::IMPORT_SYM) {
      if (!importStatement()) return false;
      while ((token == Token::CDO) || (token == Token::CDC)) {
        if (token == Token::CDO)
          skip++;
        else if (token == Token::CDC)
          skip--;
        skipBlanks();
      }
    }

    bool done = false;

    while (!done) {
      if (token == Token::MEDIA_SYM) {
        // if (!mediaStatement()) return false;
        if (!skipBlock()) return false;
      } else if (token == Token::PAGE_SYM) {
        if (!pageStatement()) return false;
      } else if (token == Token::FONT_FACE_SYM) {
        if (!fontFaceStatement()) return false;
      } else if (token == Token::NAMESPACE_SYM) {
        if (!namespaceStatement()) return false;
      } else if (token == Token::ERROR) {
        LOG_E("%s[%d]: Token ERROR retrieved!", css.getId().c_str(), lineNbr);
        return false;
      } else {
        ruleset();
      }
      while ((token == Token::CDO) || (token == Token::CDC)) {
        if (token == Token::CDO)
          skip++;
        else if (token == Token::CDC)
          skip--;
        skipBlanks();
      }
      if (token == Token::END_OF_FILE) break;
    }
    return true;
  }

  auto parse(DOM::Tag tag, const char *buffer, int32_t size) -> bool {

    str      = buffer;
    remains  = size;
    skip     = 0;
    lineNbr = 1;

    nextCh();
    nextToken();

    CSS::Properties *props = properties();

    if (props != nullptr) {
      CSS::Selector *sel      = css.selectorPool.newElement();
      CSS::SelectorNode *node = css.selectorNodePool.newElement();
      node->setTag(tag);
      sel->addSelectorNode(node);
      sel->computeSpecificity(css.getPriority());
      css.addRule(sel, props);
    }
    return true;
  }
};
