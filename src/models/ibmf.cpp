// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define _IBMF_ 1
#include "models/ibmf.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"
#include "alloc.hpp"

#include <iostream>
#include <ostream>
#include <sys/stat.h>

static constexpr uint16_t translation_latin_1[] = {
  /* 0xA1 */ 0x3C,    // ¡
  /* 0xA2 */ 0x20,    // ¢
  /* 0xA3 */ 0x20,    // £
  /* 0xA4 */ 0x20,    // ¤
  /* 0xA5 */ 0x20,    // ¥
  /* 0xA6 */ 0x20,    // ¦
  /* 0xA7 */ 0x20,    // §
  /* 0xA8 */ 0x7F,    // ¨
  /* 0xA9 */ 0x20,    // ©
  /* 0xAA */ 0x20,    // Ordfeminine
  /* 0xAB */ 0x20,    // «
  /* 0xAC */ 0x20,    // ¬
  /* 0xAD */ 0x2D,    // Soft hyphen
  /* 0xAE */ 0x20,    // ®
  /* 0xAF */ 0x20,    // Macron
  /* 0xB0 */ 0x17,    // ° Degree
  /* 0xB1 */ 0x20,    // Plus Minus
  /* 0xB2 */ 0x20,    // ²
  /* 0xB3 */ 0x20,    // ³
  /* 0xB4 */ 0x13,    // accute accent
  /* 0xB5 */ 0x20,    // µ
  /* 0xB6 */ 0x20,    // ¶ 
  /* 0xB7 */ 0x20,    // middle dot
  /* 0xB8 */ 0x18,    // cedilla
  /* 0xB9 */ 0x20,    // ¹
  /* 0xBA */ 0x20,    // 0 superscript
  /* 0xBB */ 0x20,    // »
  /* 0xBC */ 0x1E,    // Œ
  /* 0xBD */ 0x1B,    // œ
  /* 0xBE */ 0x7F59,  // Ÿ
  /* 0xBF */ 0x3E,    // ¿
  /* 0xC0 */ 0x1241,  // À
  /* 0xC1 */ 0x1341,  // Á
  /* 0xC2 */ 0x5E41,  // Â
  /* 0xC3 */ 0x7E41,  // Ã
  /* 0xC4 */ 0x7F41,  // Ä
  /* 0xC5 */ 0x1741,  // Å
  /* 0xC6 */ 0x1D,    // Æ
  /* 0xC7 */ 0x1843,  // Ç
  /* 0xC8 */ 0x1245,  // È
  /* 0xC9 */ 0x1345,  // É
  /* 0xCA */ 0x5E45,  // Ê
  /* 0xCB */ 0x7F45,  // Ë
  /* 0xCC */ 0x1249,  // Ì
  /* 0xCD */ 0x1349,  // Í
  /* 0xCE */ 0x5E49,  // Î
  /* 0xCF */ 0x7F49,  // Ï
  /* 0xD0 */ 0x44,    // Ð
  /* 0xD1 */ 0x7E4E,  // Ñ
  /* 0xD2 */ 0x124F,  // Ò
  /* 0xD3 */ 0x134F,  // Ó
  /* 0xD4 */ 0x5E4F,  // Ô
  /* 0xD5 */ 0x7E4F,  // Õ
  /* 0xD6 */ 0x7F4F,  // Ö
  /* 0xD7 */ 0x20,    // ×
  /* 0xD8 */ 0x1F,    // Ø
  /* 0xD9 */ 0x1255,  // Ù
  /* 0xDA */ 0x1355,  // Ú
  /* 0xDB */ 0x5E55,  // Û
  /* 0xDC */ 0x7F55,  // Ü
  /* 0xDD */ 0x1359,  // Ý
  /* 0xDE */ 0x20,    // Þ
  /* 0xDF */ 0x59,    // ß
  /* 0xE0 */ 0x1261,  // à
  /* 0xE1 */ 0x1361,  // á
  /* 0xE2 */ 0x5E61,  // â
  /* 0xE3 */ 0x7E61,  // ã
  /* 0xE4 */ 0x7F61,  // ä
  /* 0xE5 */ 0x1761,  // å
  /* 0xE6 */ 0x1A,    // æ
  /* 0xE7 */ 0x1863,  // ç
  /* 0xE8 */ 0x1265,  // è
  /* 0xE9 */ 0x1365,  // é
  /* 0xEA */ 0x5E65,  // ê
  /* 0xEB */ 0x7F65,  // ë
  /* 0xEC */ 0x1210,  // ì
  /* 0xED */ 0x1310,  // í
  /* 0xEE */ 0x5E10,  // î
  /* 0xEF */ 0x7F10,  // ï
  /* 0xF0 */ 0x20,    // ð
  /* 0xF1 */ 0x7E6E,  // ñ
  /* 0xF2 */ 0x126F,  // ò
  /* 0xF3 */ 0x136F,  // ó
  /* 0xF4 */ 0x5E6F,  // ô
  /* 0xF5 */ 0x7E6F,  // õ
  /* 0xF6 */ 0x7F6F,  // ö
  /* 0xF7 */ 0x20,    // ÷
  /* 0xF8 */ 0x1C,    // ø
  /* 0xF9 */ 0x1275,  // ù
  /* 0xFA */ 0x1375,  // ú
  /* 0xFB */ 0x5E75,  // û
  /* 0xFC */ 0x7F75,  // ü
  /* 0xFD */ 0x1379,  // ý
  /* 0xFE */ 0x20,    // þ
  /* 0xFF */ 0x7F79   // ÿ
};

static constexpr uint16_t translation_latin_A[] = {
  /* 0x100 */ 0x1641,  // Ā
  /* 0x101 */ 0x1661,  // ā
  /* 0x102 */ 0x1541,  // Ă
  /* 0x103 */ 0x1561,  // ă
  /* 0x104 */ 0x41,    // Ą
  /* 0x105 */ 0x61,    // ą
  /* 0x106 */ 0x1343,  // Ć
  /* 0x107 */ 0x1363,  // ć
  /* 0x108 */ 0x5E43,  // Ĉ
  /* 0x109 */ 0x5E63,  // ĉ
  /* 0x10A */ 0x5F43,  // Ċ
  /* 0x10B */ 0x5F63,  // ċ
  /* 0x10C */ 0x1443,  // Č
  /* 0x10D */ 0x1463,  // č
  /* 0x10E */ 0x1444,  // Ď
  /* 0x10F */ 0x64,    // ď

  /* 0x110 */ 0x44,    // Đ
  /* 0x111 */ 0x64,    // đ
  /* 0x112 */ 0x1645,  // Ē
  /* 0x113 */ 0x1665,  // ē
  /* 0x114 */ 0x1545,  // Ĕ
  /* 0x115 */ 0x1565,  // ĕ
  /* 0x116 */ 0x5F45,  // Ė 
  /* 0x117 */ 0x5F65,  // ė
  /* 0x118 */ 0x45,    // Ę
  /* 0x119 */ 0x65,    // ę
  /* 0x11A */ 0x1445,  // Ě
  /* 0x11B */ 0x1465,  // ě
  /* 0x11C */ 0x5E47,  // Ĝ
  /* 0x11D */ 0x5E67,  // ĝ
  /* 0x11E */ 0x1547,  // Ğ
  /* 0x11F */ 0x1567,  // ğ

  /* 0x120 */ 0x5F47,  // Ġ
  /* 0x121 */ 0x5F67,  // ġ
  /* 0x122 */ 0x1847,  // Ģ
  /* 0x123 */ 0x67,    // ģ
  /* 0x124 */ 0x5E48,  // Ĥ
  /* 0x125 */ 0x5E68,  // ĥ
  /* 0x126 */ 0x48,    // Ħ
  /* 0x127 */ 0x68,    // ħ
  /* 0x128 */ 0x7E49,  // Ĩ
  /* 0x129 */ 0x7E10,  // ĩ
  /* 0x12A */ 0x1649,  // Ī
  /* 0x12B */ 0x1610,  // ī
  /* 0x12C */ 0x1549,  // Ĭ
  /* 0x12D */ 0x1510,  // ĭ
  /* 0x12E */ 0x49,    // Į
  /* 0x12F */ 0x69,    // į

  /* 0x130 */ 0x5F49,  // İ
  /* 0x131 */ 0x10,    // ı
  /* 0x132 */ 0x20,    // Ĳ
  /* 0x133 */ 0x20,    // ĳ
  /* 0x134 */ 0x5E4A,  // Ĵ
  /* 0x135 */ 0x5E11,  // ĵ
  /* 0x136 */ 0x184B,  // Ķ
  /* 0x137 */ 0x186B,  // ķ
  /* 0x138 */ 0x6B,    // ĸ
  /* 0x139 */ 0x134C,  // Ĺ
  /* 0x13A */ 0x136C,  // ĺ
  /* 0x13B */ 0x184C,  // Ļ
  /* 0x13C */ 0x186C,  // ļ
  /* 0x13D */ 0x4C,    // Ľ
  /* 0x13E */ 0x6C,    // ľ
  /* 0x13F */ 0x4C,    // Ŀ

  /* 0x140 */ 0x6C,    // ŀ
  /* 0x141 */ 0x4C,    // Ł
  /* 0x142 */ 0x6C,    // ł
  /* 0x143 */ 0x134E,  // Ń
  /* 0x144 */ 0x136E,  // ń
  /* 0x145 */ 0x184E,  // Ņ
  /* 0x146 */ 0x186E,  // ņ
  /* 0x147 */ 0x144E,  // Ň
  /* 0x148 */ 0x146E,  // ň
  /* 0x149 */ 0x6E,    // ŉ
  /* 0x14A */ 0x4E,    // Ŋ
  /* 0x14B */ 0x6E,    // ŋ
  /* 0x14C */ 0x164F,  // Ō
  /* 0x14D */ 0x166F,  // ō
  /* 0x14E */ 0x154F,  // Ŏ
  /* 0x14F */ 0x156F,  // ŏ

  /* 0x150 */ 0x7D4F,  // Ő
  /* 0x151 */ 0x7D6F,  // ő
  /* 0x152 */ 0x1E,    // Œ
  /* 0x153 */ 0x1B,    // œ
  /* 0x154 */ 0x1352,  // Ŕ
  /* 0x155 */ 0x1372,  // ŕ
  /* 0x156 */ 0x1852,  // Ŗ
  /* 0x157 */ 0x1872,  // ŗ
  /* 0x158 */ 0x1452,  // Ř
  /* 0x159 */ 0x1472,  // ř
  /* 0x15A */ 0x1353,  // Ś
  /* 0x15B */ 0x1373,  // ś
  /* 0x15C */ 0x5E53,  // Ŝ
  /* 0x15D */ 0x5E73,  // ŝ
  /* 0x15E */ 0x1853,  // Ş
  /* 0x15F */ 0x1873,  // ş

  /* 0x160 */ 0x1453,  // Š
  /* 0x161 */ 0x1473,  // š
  /* 0x162 */ 0x1854,  // Ţ
  /* 0x163 */ 0x1874,  // ţ
  /* 0x164 */ 0x1454,  // Ť
  /* 0x165 */ 0x74,    // ť
  /* 0x166 */ 0x54,    // Ŧ
  /* 0x167 */ 0x74,    // ŧ
  /* 0x168 */ 0x7E55,  // Ũ
  /* 0x169 */ 0x7E75,  // ũ
  /* 0x16A */ 0x1655,  // Ū
  /* 0x16B */ 0x1675,  // ū
  /* 0x16C */ 0x1555,  // Ŭ
  /* 0x16D */ 0x1575,  // ŭ
  /* 0x16E */ 0x1755,  // Ů
  /* 0x16F */ 0x1775,  // ů

  /* 0x170 */ 0x7D55,  // Ű
  /* 0x171 */ 0x7D75,  // ű
  /* 0x172 */ 0x55,    // Ų
  /* 0x173 */ 0x75,    // ų
  /* 0x174 */ 0x5E57,  // Ŵ
  /* 0x175 */ 0x5E77,  // ŵ
  /* 0x176 */ 0x5E59,  // Ŷ
  /* 0x177 */ 0x5E79,  // ŷ
  /* 0x178 */ 0x7F59,  // Ÿ
  /* 0x179 */ 0x135A,  // Ź
  /* 0x17A */ 0x137A,  // ź
  /* 0x17B */ 0x5F5A,  // Ż
  /* 0x17C */ 0x5F7A,  // ż
  /* 0x17D */ 0x145A,  // Ž
  /* 0x17E */ 0x147A,  // ž
  /* 0x17F */ 0x20     // ſ
};

IBMF::IBMF(const std::string & filename) : Font()
{
  face  = nullptr;

  set_font_face_from_file(filename);
}

IBMF::IBMF(unsigned char * buffer, int32_t buffer_size) : Font()
{
  face  = nullptr;
 
  set_font_face_from_memory(buffer, buffer_size);
}

IBMF::~IBMF()
{
  ready = false;
  if (face != nullptr) clear_face();
}

void
IBMF::clear_face()
{
  clear_cache();
  if (face != nullptr) {
    delete face;
    face = nullptr;
  }
  if (memory_font != nullptr) {
    free(memory_font);
    memory_font = nullptr;
  }
  
  ready             = false;
  current_font_size = -1;
}

// uint32_t IBMF::translate(uint32_t charcode)
// {
//   uint32_t glyph_code = charcode;

//   if (face->get_char_set() == 0) {
//     if ((charcode >= 0x20) && (charcode <= 0x7E)) {
//       switch (charcode) {
//         case '<':
//         case '>':
//         case '\\':
//         case '_':
//         case '{':
//         case '|':
//         case '}':
//           glyph_code = ' '; break;
//         case '`':
//           glyph_code = 0x12; break;
//         default:
//           glyph_code = charcode;
//       }
//     } 
//     else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
//       glyph_code = translation_latin_1[charcode - 0xA1];
//     }
//     else if ((charcode >= 0x100) && (charcode <= 0x17F)) {
//       glyph_code = translation_latin_A[charcode - 0x100];
//     }
//     else {
//       switch (charcode) {
//         case 0x2013: // endash
//           glyph_code = 0x7B; break;
//         case 0x2014: // emdash
//           glyph_code = 0x7C; break;
//         case 0x2018: // quote left
//         case 0x02BB: // reverse apostrophe
//           glyph_code = 0x60; break;
//         case 0x2019: // quote right
//         case 0x02BC: // apostrophe
//           glyph_code = 0x27; break;
//         case 0x201C: glyph_code = 0x5C; break; // quoted left "
//         case 0x201D: glyph_code = 0x22; break; // quoted right
//         case 0x02C6: glyph_code = 0x5E; break; // circumflex
//         case 0x02DA: glyph_code = 0x17; break; // ring
//         case 0x02DC: glyph_code = 0x7E; break; // tilde ~
//         case 0x201A: glyph_code = 0x2C; break; // comma like ,
//         case 0x2032: glyph_code = 0x0C; break; // minute '
//         case 0x2033: glyph_code = 0x22; break; // second "
//         case 0x2044: glyph_code = 0x2F; break; // fraction /
//         default:
//           glyph_code = ' ';
//       }
//     }
//   }
//   else {  // char_set == 1 : Typewriter
//     if ((charcode >= ' ') && (charcode <= '~')) {
//       if (charcode == '\\') {
//         glyph_code = 0x22;
//       }
//       else glyph_code = charcode;
//     } else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
//       if       (charcode == 0xA1) glyph_code = 0x0E; // ¡
//       else if  (charcode == 0xBF) glyph_code = 0x0F; // ¿
//       else glyph_code = translation_latin_1[charcode - 0xA1];
//     }
//     else if ((charcode >= 0x100) && (charcode <= 0x17F)) {
//       glyph_code = translation_latin_A[charcode - 0x100];
//     }
//     else {
//       switch (charcode) {
//         case 0x2018: // quote left
//         case 0x02BB: // reverse apostrophe
//           glyph_code = 0x60; break;
//         case 0x2019: // quote right
//         case 0x02BC: // apostrophe
//           glyph_code = 0x27; break;
//         case 0x201C: // quoted left "
//         case 0x201D: // quoted right
//           glyph_code = 0x22; break;
//         case 0x02C6: glyph_code = 0x5E; break; // circumflex
//         case 0x02DA: glyph_code = 0x17; break; // ring
//         case 0x02DC: glyph_code = 0x7E; break; // tilde ~
//         case 0x201A: glyph_code = 0x2C; break; // comma like ,
//         case 0x2032: glyph_code = 0x0C; break; // minute '
//         case 0x2033: glyph_code = 0x22; break; // second "
//         case 0x2044: glyph_code = 0x2F; break; // fraction /
//         default:
//           glyph_code = ' ';
//       }
//     }
//   }

//   return glyph_code;
// }

Font::Glyph *
IBMF::get_glyph(uint32_t charcode, uint32_t next_charcode, int16_t glyph_size, int16_t & kern, bool & ignore_next)
{
  std::scoped_lock guard(mutex);

  uint32_t glyph_code = translate(charcode);

  ignore_next = false;
  Glyph * glyph = get_glyph_internal(glyph_code, glyph_size);

  if (glyph != nullptr) {
    if (glyph->ligature_and_kern_pgm_index >= 0) {
      IBMFFont::FIX16 k;
      glyph = adjust_ligature_and_kern(glyph, glyph_size, next_charcode, k, ignore_next);
      if (glyph == nullptr) return nullptr;
      kern = (k == 0) ? glyph->advance : ((glyph_data->advance + k) >> 6);
    }
    else {
      kern =  glyph->advance;
    }
  }

  return glyph;
}

Font::Glyph *
IBMF::get_glyph(uint32_t charcode, int16_t glyph_size)
{
  std::scoped_lock guard(mutex);
  
  uint32_t glyph_code = translate(charcode);

  return get_glyph_internal(glyph_code, glyph_size);
}

Font::Glyph *
IBMF::get_glyph_internal(uint32_t glyph_code, int16_t glyph_size)
{
  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  if (current_font_size != glyph_size) set_font_size(glyph_size);

  GlyphsCache::iterator cache_it = cache.find(glyph_size);

  bool found = (cache_it != cache.end()) &&
               ((git = cache_it->second.find(glyph_code)) != cache_it->second.end());

  if (found) {
    glyph_data = face->get_glyph_info(glyph_code & 0x000000FF);
    return git->second;
  }
  else {
    Glyph * glyph = bitmap_glyph_pool.newElement();

    if (glyph == nullptr) {
      LOG_E("Unable to allocate memory for glyph.");
      msg_viewer.out_of_memory("glyph allocation");
    }

    glyph_data = nullptr;

    if ((glyph_code == ' ') || (glyph_code == 160)) {
      glyph->dim.width   =  0;
      glyph->dim.height  =  0;
      glyph->line_height =  face->get_line_height();
      glyph->pitch       =  0;
      glyph->xoff        =  0;
      glyph->yoff        =  0;
      glyph->advance     =  8;
      glyph->ligature_and_kern_pgm_index = -1;
    }
    else if (!face->get_glyph(glyph_code, *glyph, &glyph_data, true)) {
      bitmap_glyph_pool.deallocate(glyph);
      LOG_E("Unable to render glyph for glyph_code: %d", glyph_code);
      return nullptr;
    }

    // std::cout << "Glyph: " <<
    //   " w:"  << glyph->dim.width <<
    //   " bw:" << slot->bitmap.width <<
    //   " h:"  << glyph->dim.height <<
    //   " br:" << slot->bitmap.rows <<
    //   " p:"  << glyph->pitch <<
    //   " x:"  << glyph->xoff <<
    //   " y:"  << glyph->yoff <<
    //   " a:"  << glyph->advance << std::endl;

    cache[current_font_size][glyph_code] = glyph;
    return glyph;
  }
}

bool 
IBMF::set_font_size(int16_t size)
{
  if (face->set_font_size(size)) {  
    current_font_size = size;
    return true;
  }
  else {
    current_font_size = -1;
    return false;
  }
}

bool 
IBMF::set_font_face_from_memory(unsigned char * buffer, int32_t buffer_size)
{
  if (face != nullptr) clear_face();

  face = new IBMFFont(buffer, buffer_size, *this);

  if (face == nullptr) {
    LOG_E("The memory font format is unsupported or is broken.");
    if (buffer != nullptr) free(buffer);
    return false;
  }

  ready       = true;
  memory_font = buffer;
  return true;
}

Font::Glyph * 
IBMF::adjust_ligature_and_kern(Glyph   * glyph, 
                               uint16_t  glyph_size, 
                               uint32_t  next_charcode, 
                               int16_t & kern,
                               bool    & ignore_next)
{
  ignore_next = false;
  kern = 0;

  if ((next_charcode != 0) && (glyph->ligature_and_kern_pgm_index != 255)) {
    const IBMFFont::LigKernStep * step = face->get_lig_kern(glyph->ligature_and_kern_pgm_index);
    while (true) {
      if (step->tag == 1) {
        if (step->next_char_code == next_charcode) {
          kern = face->get_kern(step->u.kern_idx);
          LOG_D("Kerning between %c and %c: %d", (char) glyph_data->char_code, (char) next_charcode, kern);
          break;
        }
      }
      else {
        if (step->next_char_code == next_charcode) {
          LOG_D("Ligature between %c and %c", (char) glyph_data->char_code, (char) next_charcode);
          glyph = get_glyph_internal(step->u.char_code | 0xFF00, glyph_size);
          ignore_next = true;
          break;
        }
      }
      if (step->stop == 1) break;
      step++;
    }
  }
  return glyph;
}