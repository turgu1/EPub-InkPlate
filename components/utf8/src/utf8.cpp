#include "utf8.hpp"

#include <frozen/unordered_set.h>
#include <frozen/unordered_map.h>
#include <frozen/string.h>

static constexpr frozen::unordered_map<frozen::string, char32_t, 129> entities = {
  { "nbsp",   0x00A0 }, // no-break space = non-breaking space, U+00A0 ISOnum
  { "iexcl",  0x00A1 }, // inverted exclamation mark, U+00A1 ISOnum
  { "cent",   0x00A2 }, // cent sign, U+00A2 ISOnum
  { "pound",  0x00A3 }, // pound sign, U+00A3 ISOnum
  { "curren", 0x00A4 }, // currency sign, U+00A4 ISOnum
  { "yen",    0x00A5 }, // yen sign = yuan sign, U+00A5 ISOnum
  { "brvbar", 0x00A6 }, // broken bar = broken vertical bar, U+00A6 ISOnum
  { "sect",   0x00A7 }, // section sign, U+00A7 ISOnum
  { "uml",    0x00A8 }, // diaeresis = spacing diaeresis, U+00A8 ISOdia
  { "copy",   0x00A9 }, // copyright sign, U+00A9 ISOnum
  { "ordf",   0x00AA }, // feminine ordinal indicator, U+00AA ISOnum
  { "laquo",  0x00AB }, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
  { "not",    0x00AC }, // not sign = angled dash, U+00AC ISOnum
  { "shy",    0x00AD }, // soft hyphen = discretionary hyphen, U+00AD ISOnum
  { "reg",    0x00AE }, // registered sign = registered trade mark sign, U+00AE ISOnum
  { "macr",   0x00AF }, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
  { "deg",    0x00B0 }, // degree sign, U+00B0 ISOnum
  { "plusmn", 0x00B1 }, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
  { "sup2",   0x00B2 }, // superscript two = superscript digit two = squared, U+00B2 ISOnum
  { "sup3",   0x00B3 }, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
  { "acute",  0x00B4 }, // acute accent = spacing acute, U+00B4 ISOdia
  { "micro",  0x00B5 }, // micro sign, U+00B5 ISOnum
  { "para",   0x00B6 }, // pilcrow sign = paragraph sign, U+00B6 ISOnum
  { "middot", 0x00B7 }, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
  { "cedil",  0x00B8 }, // cedilla = spacing cedilla, U+00B8 ISOdia
  { "sup1",   0x00B9 }, // superscript one = superscript digit one, U+00B9 ISOnum
  { "ordm",   0x00BA }, // masculine ordinal indicator, U+00BA ISOnum
  { "raquo",  0x00BB }, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
  { "frac14", 0x00BC }, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
  { "frac12", 0x00BD }, // vulgar fraction one half = fraction one half, U+00BD ISOnum
  { "frac34", 0x00BE }, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
  { "iquest", 0x00BF }, // inverted question mark = turned question mark, U+00BF ISOnum
  { "Agrave", 0x00C0 }, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
  { "Aacute", 0x00C1 }, // latin capital letter A with acute, U+00C1 ISOlat1
  { "Acirc",  0x00C2 }, // latin capital letter A with circumflex, U+00C2 ISOlat1
  { "Atilde", 0x00C3 }, // latin capital letter A with tilde, U+00C3 ISOlat1
  { "Auml",   0x00C4 }, // latin capital letter A with diaeresis, U+00C4 ISOlat1
  { "Aring",  0x00C5 }, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
  { "AElig",  0x00C6 }, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
  { "Ccedil", 0x00C7 }, // latin capital letter C with cedilla, U+00C7 ISOlat1
  { "Egrave", 0x00C8 }, // latin capital letter E with grave, U+00C8 ISOlat1
  { "Eacute", 0x00C9 }, // latin capital letter E with acute, U+00C9 ISOlat1
  { "Ecirc",  0x00CA }, // latin capital letter E with circumflex, U+00CA ISOlat1
  { "Euml",   0x00CB }, // latin capital letter E with diaeresis, U+00CB ISOlat1
  { "Igrave", 0x00CC }, // latin capital letter I with grave, U+00CC ISOlat1
  { "Iacute", 0x00CD }, // latin capital letter I with acute, U+00CD ISOlat1
  { "Icirc",  0x00CE }, // latin capital letter I with circumflex, U+00CE ISOlat1
  { "Iuml",   0x00CF }, // latin capital letter I with diaeresis, U+00CF ISOlat1
  { "ETH",    0x00D0 }, // latin capital letter ETH, U+00D0 ISOlat1
  { "Ntilde", 0x00D1 }, // latin capital letter N with tilde, U+00D1 ISOlat1
  { "Ograve", 0x00D2 }, // latin capital letter O with grave, U+00D2 ISOlat1
  { "Oacute", 0x00D3 }, // latin capital letter O with acute, U+00D3 ISOlat1
  { "Ocirc",  0x00D4 }, // latin capital letter O with circumflex, U+00D4 ISOlat1
  { "Otilde", 0x00D5 }, // latin capital letter O with tilde, U+00D5 ISOlat1
  { "Ouml",   0x00D6 }, // latin capital letter O with diaeresis, U+00D6 ISOlat1
  { "times",  0x00D7 }, // multiplication sign, U+00D7 ISOnum
  { "Oslash", 0x00D8 }, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
  { "Ugrave", 0x00D9 }, // latin capital letter U with grave, U+00D9 ISOlat1
  { "Uacute", 0x00DA }, // latin capital letter U with acute, U+00DA ISOlat1
  { "Ucirc",  0x00DB }, // latin capital letter U with circumflex, U+00DB ISOlat1
  { "Uuml",   0x00DC }, // latin capital letter U with diaeresis, U+00DC ISOlat1
  { "Yacute", 0x00DD }, // latin capital letter Y with acute, U+00DD ISOlat1
  { "THORN",  0x00DE }, // latin capital letter THORN, U+00DE ISOlat1
  { "szlig",  0x00DF }, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
  { "agrave", 0x00E0 }, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
  { "aacute", 0x00E1 }, // latin small letter a with acute, U+00E1 ISOlat1
  { "acirc",  0x00E2 }, // latin small letter a with circumflex, U+00E2 ISOlat1
  { "atilde", 0x00E3 }, // latin small letter a with tilde, U+00E3 ISOlat1
  { "auml",   0x00E4 }, // latin small letter a with diaeresis, U+00E4 ISOlat1
  { "aring",  0x00E5 }, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
  { "aelig",  0x00E6 }, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
  { "ccedil", 0x00E7 }, // latin small letter c with cedilla, U+00E7 ISOlat1
  { "egrave", 0x00E8 }, // latin small letter e with grave, U+00E8 ISOlat1
  { "eacute", 0x00E9 }, // latin small letter e with acute, U+00E9 ISOlat1
  { "ecirc",  0x00EA }, // latin small letter e with circumflex, U+00EA ISOlat1
  { "euml",   0x00EB }, // latin small letter e with diaeresis, U+00EB ISOlat1
  { "igrave", 0x00EC }, // latin small letter i with grave, U+00EC ISOlat1
  { "iacute", 0x00ED }, // latin small letter i with acute, U+00ED ISOlat1
  { "icirc",  0x00EE }, // latin small letter i with circumflex, U+00EE ISOlat1
  { "iuml",   0x00EF }, // latin small letter i with diaeresis, U+00EF ISOlat1
  { "eth",    0x00F0 }, // latin small letter eth, U+00F0 ISOlat1
  { "ntilde", 0x00F1 }, // latin small letter n with tilde, U+00F1 ISOlat1
  { "ograve", 0x00F2 }, // latin small letter o with grave, U+00F2 ISOlat1
  { "oacute", 0x00F3 }, // latin small letter o with acute, U+00F3 ISOlat1
  { "ocirc",  0x00F4 }, // latin small letter o with circumflex, U+00F4 ISOlat1
  { "otilde", 0x00F5 }, // latin small letter o with tilde, U+00F5 ISOlat1
  { "ouml",   0x00F6 }, // latin small letter o with diaeresis, U+00F6 ISOlat1
  { "divide", 0x00F7 }, // division sign, U+00F7 ISOnum
  { "oslash", 0x00F8 }, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
  { "ugrave", 0x00F9 }, // latin small letter u with grave, U+00F9 ISOlat1
  { "uacute", 0x00FA }, // latin small letter u with acute, U+00FA ISOlat1
  { "ucirc",  0x00FB }, // latin small letter u with circumflex, U+00FB ISOlat1
  { "uuml",   0x00FC }, // latin small letter u with diaeresis, U+00FC ISOlat1
  { "yacute", 0x00FD }, // latin small letter y with acute, U+00FD ISOlat1
  { "thorn",  0x00FE }, // latin small letter thorn, U+00FE ISOlat1
  { "yuml",   0x00FF }, // latin small letter y with diaeresis, U+00FF ISOlat1
  { "quot",   0x0022 }, // quotation mark, U+0022 ISOnum
  { "amp",    0x0026 }, // ampersand, U+0026 ISOnum
  { "lt",     0x003C }, // less-than sign, U+003C ISOnum
  { "gt",     0x003E }, // greater-than sign, U+003E ISOnum
  { "apos",   0x0027 }, // apostrophe = APL quote, U+0027 ISOnum
  { "OElig",  0x0152 }, // latin capital ligature OE, U+0152 ISOlat2
  { "oelig",  0x0153 }, // latin small ligature oe, U+0153 ISOlat2
  { "Scaron", 0x0160 }, // latin capital letter S with caron, U+0160 ISOlat2
  { "scaron", 0x0161 }, // latin small letter s with caron, U+0161 ISOlat2
  { "Yuml",   0x0178 }, // latin capital letter Y with diaeresis, U+0178 ISOlat2
  { "circ",   0x02C6 }, // modifier letter circumflex accent, U+02C6 ISOpub
  { "tilde",  0x02DC }, // small tilde, U+02DC ISOdia
  { "ensp",   0x2002 }, // en space, U+2002 ISOpub
  { "emsp",   0x2003 }, // em space, U+2003 ISOpub
  { "thinsp", 0x2009 }, // thin space, U+2009 ISOpub
  { "zwnj",   0x200C }, // zero width non-joiner, U+200C NEW RFC 2070
  { "zwj",    0x200D }, // zero width joiner, U+200D NEW RFC 2070
  { "lrm",    0x200E }, // left-to-right mark, U+200E NEW RFC 2070
  { "rlm",    0x200F }, // right-to-left mark, U+200F NEW RFC 2070
  { "ndash",  0x2013 }, // en dash, U+2013 ISOpub
  { "mdash",  0x2014 }, // em dash, U+2014 ISOpub
  { "lsquo",  0x2018 }, // left single quotation mark, U+2018 ISOnum
  { "rsquo",  0x2019 }, // right single quotation mark, U+2019 ISOnum
  { "sbquo",  0x201A }, // single low-9 quotation mark, U+201A NEW
  { "ldquo",  0x201C }, // left double quotation mark,  U+201C ISOnum
  { "rdquo",  0x201D }, // right double quotation mark, U+201D ISOnum
  { "bdquo",  0x201E }, // double low-9 quotation mark, U+201E NEW
  { "dagger", 0x2020 }, // dagger, U+2020 ISOpub
  { "Dagger", 0x2021 }, // double dagger, U+2021 ISOpub
  { "permil", 0x2030 }, // per mille sign, U+2030 ISOtech
  { "lsaquo", 0x2039 }, // single left-pointing angle quotation mark, U+2039 ISO proposed
  { "rsaquo", 0x203A }, // single right-pointing angle quotation mark, U+203A ISO proposed
  { "euro",   0x20AC }, // euro sign, U+20AC NEW
};


static constexpr frozen::unordered_set<char32_t, 22> shouldTrim {
  0x0027, 0x0022, // ' and "
  0x002C, 0x002E, // , and .
  0x0021, 0x003F, // ! and ?
  0x003A, 0x003B, // : and ;
  0x003C, 0x003E, // < and >
  0x0028, 0x0029, // ( and )
  0x005B, 0x005D, // [ and ]
  0x007B, 0x007D, // { and }
  0x00AB, 0x00BB, // « and »
  0x2018, 0x2019, // ‘ and ’
  0x201C, 0x201D, // “ and ”
};

static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static auto toCodePoint(const uint8_t *str) -> std::pair<char32_t, const uint8_t*> {
    uint32_t state = 0;
    char32_t codePoint = 0;

    while (true) {
        uint8_t byte = *str++;
        uint8_t type = utf8d[byte];

        codePoint = (state != 0) ? 
            (byte & 0x3fu) | (codePoint << 6) : 
            (0xff >> type) & byte;

        state = utf8d[256 + (state << 4) + type];

        if (state == 0) return {codePoint, str};        // Success!
        if (state == 1) {                // Invalid sequence detected
            codePoint = 0xFFFD;
            return {codePoint, str}; 
        }
    }
}

static auto hexValue(uint8_t c) -> char32_t {
  if ((c >= '0') && (c <= '9')) { return c - '0'; }
  if ((c >= 'A') && (c <= 'F')) { return c - 'A' + 10; }
  if ((c >= 'a') && (c <= 'f')) { return c - 'a' + 10; }
  return 0;
}

// 00000000 -- 0000007F:  0xxxxxxx
// 00000080 -- 000007FF:  110xxxxx 10xxxxxx
// 00000800 -- 0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
// 00010000 -- 001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

auto UTF8::toUnicode(const char *str, TextTransform transform, bool first) -> std::pair<char32_t, const char *> {

  const uint8_t *c    = reinterpret_cast<const uint8_t *>(str);
  char32_t       u    = ' ';

  if (*c == '&') {
    if (c[1] == '#') {
      // Numeric character reference
      u = 0;
      c += 2;
      if (*c == 'x' || *c == 'X') {
        // Hexadecimal
        c++;
        while (*c != ';' && *c != 0) {
          u = (u << 4) + hexValue(*c);
          c++;
        }
      } else {
        // Decimal
        while (*c != ';' && *c != 0) {
          u = (u * 10) + (*c - '0');
          c++;
        }
      }
      if (*c == ';') { c++; }
    } else {
      // Named character reference
      const uint8_t *name = c + 1;
      const uint8_t *end  = name;
      uint8_t        len         = 0;
      while ((len < 7) && (*end != 0) && (*end != ';')) {
        ++end;
        ++len;
      }

      if (*end == ';') {
        auto theStr = frozen::string((char *)(name), (size_t)(end - name));
        auto search = entities.find(theStr);
        if (search != entities.end()) {
          u = search->second;
        }
      }

      if (u == ' ') {
        // Unknown or malformed entity: consume '&' and return it as-is.
        u = '&';
        ++c;
      } else {
        c = end + 1;
      }
    }
  } else {
    std::tie(u, c) = toCodePoint(c);
  }

  if (u <= 0x7F) {
    switch (transform) {
    case TextTransform::NONE:
      break;
    case TextTransform::UPPERCASE:
      u = toupper(static_cast<unsigned char>(u));
      break;
    case TextTransform::LOWERCASE:
      u = tolower(static_cast<unsigned char>(u));
      break;
    case TextTransform::CAPITALIZE:
      if (first) { u = toupper(static_cast<unsigned char>(u)); }
      break;
    }
  }

  return { u, reinterpret_cast<const char *>(c) };
}

auto UTF8::toUTF8(char32_t codePoint, std::string &concatTo) -> int {
    // 1-byte ASCII sequence: 0xxxxxxx
    if (codePoint <= 0x7F) {
        concatTo.push_back(static_cast<char>(codePoint));
        return 1;
    }
    // 2-byte sequence: 110xxxxx 10xxxxxx
    else if (codePoint <= 0x7FF) {
        concatTo.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        concatTo.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return 2;
    }
    // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
    else if (codePoint <= 0xFFFF) {
        concatTo.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        concatTo.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        concatTo.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return 3;
    }
    // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    else if (codePoint <= 0x10FFFF) {
        concatTo.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
        concatTo.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        concatTo.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        concatTo.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return 4;
    }
    // Fallback for invalid Unicode code points (outside the valid 21-bit range)
    else {
        // Option 1: Append nothing or handle error
        // Option 2: Append the official Unicode replacement character U+FFFD
        return toUTF8(0xFFFD, concatTo);
    }
}

auto UTF8::extractWord(const char* word, int16_t idx) -> std::tuple<std::string, std::vector<uint16_t>, int16_t> {
  if (!word) {
      return { "", {}, 0 };
  }

  // Structure to represent a decoded UTF-8 character with its source details
  struct Utf8Char {
      char32_t codePoint;
      const char * start;
      const char * end;
  };

  std::vector<Utf8Char> tokens;

  const char * str = word;

  while (*str) {
    auto [codePoint, str2] = toUnicode(str, TextTransform::LOWERCASE, false);

    if (codePoint != static_cast<char32_t>(0x00AD)) { tokens.push_back(Utf8Char{codePoint, str, str2}); }

    str = str2;
  }

  // Find internal boundaries for trimming
  size_t start = 0;
  size_t end = tokens.size();

  while (start < end && shouldTrim.contains(tokens[start].codePoint)) { start++; }
  while (end > start && shouldTrim.contains(tokens[end - 1].codePoint)) { end--; }

  // Build final string and assign duplicate starting indices
  std::string resultString;
  std::vector<uint16_t> resultIndices;
  int16_t k = -1;

  for (size_t i = start; i < end; ++i) {
    int16_t firstByteIdx = static_cast<int16_t>(tokens[i].start - word);
    if ((idx >= firstByteIdx) && (idx < (tokens[i].end - word))) {
      k = resultIndices.size();
    }
    
    auto length = toUTF8(tokens[i].codePoint, resultString);

    for (int j = 0; j < length; j++) {
      resultIndices.push_back(static_cast<uint16_t>(firstByteIdx));
    }
  }

  return { resultString, resultIndices, k };
}
