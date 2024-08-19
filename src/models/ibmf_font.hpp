#pragma once

#include <iostream>
#include <fstream>
#include <cstring>

#include "global.hpp"
#include "models/font.hpp"
#include "screen.hpp"

#include "sys/stat.h"

#if 0
static constexpr uint16_t translation_latin_1[] = {
  /* 0xA1 */ 0xFF3C,  // ¡
  /* 0xA2 */ 0xFFFF,  // ¢
  /* 0xA3 */ 0xFFFF,  // £
  /* 0xA4 */ 0xFFFF,  // ¤
  /* 0xA5 */ 0xFFFF,  // ¥
  /* 0xA6 */ 0xFFFF,  // ¦
  /* 0xA7 */ 0xFFFF,  // §
  /* 0xA8 */ 0xFF7F,  // ¨
  /* 0xA9 */ 0xFFFF,  // ©
  /* 0xAA */ 0xFFFF,  // Ordfeminine
  /* 0xAB */ 0xFFFF,  // «
  /* 0xAC */ 0xFFFF,  // ¬
  /* 0xAD */ 0xFF2D,  // Soft hyphen
  /* 0xAE */ 0xFFFF,  // ®
  /* 0xAF */ 0xFFFF,  // Macron
  /* 0xB0 */ 0xFF17,  // ° Degree
  /* 0xB1 */ 0xFFFF,  // Plus Minus
  /* 0xB2 */ 0xFFFF,  // ²
  /* 0xB3 */ 0xFFFF,  // ³
  /* 0xB4 */ 0xFF13,  // accute accent
  /* 0xB5 */ 0xFFFF,  // µ
  /* 0xB6 */ 0xFFFF,  // ¶ 
  /* 0xB7 */ 0xFFFF,  // middle dot
  /* 0xB8 */ 0xFF18,  // cedilla
  /* 0xB9 */ 0xFFFF,  // ¹
  /* 0xBA */ 0xFFFF,  // 0 superscript
  /* 0xBB */ 0xFFFF,  // »
  /* 0xBC */ 0xFF1E,  // Œ
  /* 0xBD */ 0xFF1B,  // œ
  /* 0xBE */ 0x7F59,  // Ÿ
  /* 0xBF */ 0xFF3E,  // ¿
  /* 0xC0 */ 0x1241,  // À
  /* 0xC1 */ 0x1341,  // Á
  /* 0xC2 */ 0x5E41,  // Â
  /* 0xC3 */ 0x7E41,  // Ã
  /* 0xC4 */ 0x7F41,  // Ä
  /* 0xC5 */ 0x1741,  // Å
  /* 0xC6 */ 0xFF1D,  // Æ
  /* 0xC7 */ 0x1843,  // Ç
  /* 0xC8 */ 0x1245,  // È
  /* 0xC9 */ 0x1345,  // É
  /* 0xCA */ 0x5E45,  // Ê
  /* 0xCB */ 0x7F45,  // Ë
  /* 0xCC */ 0x1249,  // Ì
  /* 0xCD */ 0x1349,  // Í
  /* 0xCE */ 0x5E49,  // Î
  /* 0xCF */ 0x7F49,  // Ï
  /* 0xD0 */ 0xFF44,  // Ð
  /* 0xD1 */ 0x7E4E,  // Ñ
  /* 0xD2 */ 0x124F,  // Ò
  /* 0xD3 */ 0x134F,  // Ó
  /* 0xD4 */ 0x5E4F,  // Ô
  /* 0xD5 */ 0x7E4F,  // Õ
  /* 0xD6 */ 0x7F4F,  // Ö
  /* 0xD7 */ 0xFFFF,  // ×
  /* 0xD8 */ 0xFF1F,  // Ø
  /* 0xD9 */ 0x1255,  // Ù
  /* 0xDA */ 0x1355,  // Ú
  /* 0xDB */ 0x5E55,  // Û
  /* 0xDC */ 0x7F55,  // Ü
  /* 0xDD */ 0x1359,  // Ý
  /* 0xDE */ 0xFFFF,  // Þ
  /* 0xDF */ 0xFF59,  // ß
  /* 0xE0 */ 0x1261,  // à
  /* 0xE1 */ 0x1361,  // á
  /* 0xE2 */ 0x5E61,  // â
  /* 0xE3 */ 0x7E61,  // ã
  /* 0xE4 */ 0x7F61,  // ä
  /* 0xE5 */ 0x1761,  // å
  /* 0xE6 */ 0xFF1A,  // æ
  /* 0xE7 */ 0x1863,  // ç
  /* 0xE8 */ 0x1265,  // è
  /* 0xE9 */ 0x1365,  // é
  /* 0xEA */ 0x5E65,  // ê
  /* 0xEB */ 0x7F65,  // ë
  /* 0xEC */ 0x1210,  // ì
  /* 0xED */ 0x1310,  // í
  /* 0xEE */ 0x5E10,  // î
  /* 0xEF */ 0x7F10,  // ï
  /* 0xF0 */ 0xFFFF,  // ð
  /* 0xF1 */ 0x7E6E,  // ñ
  /* 0xF2 */ 0x126F,  // ò
  /* 0xF3 */ 0x136F,  // ó
  /* 0xF4 */ 0x5E6F,  // ô
  /* 0xF5 */ 0x7E6F,  // õ
  /* 0xF6 */ 0x7F6F,  // ö
  /* 0xF7 */ 0xFFFF,  // ÷
  /* 0xF8 */ 0xFF1C,  // ø
  /* 0xF9 */ 0x1275,  // ù
  /* 0xFA */ 0x1375,  // ú
  /* 0xFB */ 0x5E75,  // û
  /* 0xFC */ 0x7F75,  // ü
  /* 0xFD */ 0x1379,  // ý
  /* 0xFE */ 0xFFFF,  // þ
  /* 0xFF */ 0x7F79   // ÿ
};

static constexpr uint16_t translation_latin_A[] = {
  /* 0x100 */ 0x1641,  // Ā
  /* 0x101 */ 0x1661,  // ā
  /* 0x102 */ 0x1541,  // Ă
  /* 0x103 */ 0x1561,  // ă
  /* 0x104 */ 0xFF41,  // Ą
  /* 0x105 */ 0xFF61,  // ą
  /* 0x106 */ 0x1343,  // Ć
  /* 0x107 */ 0x1363,  // ć
  /* 0x108 */ 0x5E43,  // Ĉ
  /* 0x109 */ 0x5E63,  // ĉ
  /* 0x10A */ 0x5F43,  // Ċ
  /* 0x10B */ 0x5F63,  // ċ
  /* 0x10C */ 0x1443,  // Č
  /* 0x10D */ 0x1463,  // č
  /* 0x10E */ 0x1444,  // Ď
  /* 0x10F */ 0xFF64,  // ď

  /* 0x110 */ 0xFF44,  // Đ
  /* 0x111 */ 0xFF64,  // đ
  /* 0x112 */ 0x1645,  // Ē
  /* 0x113 */ 0x1665,  // ē
  /* 0x114 */ 0x1545,  // Ĕ
  /* 0x115 */ 0x1565,  // ĕ
  /* 0x116 */ 0x5F45,  // Ė 
  /* 0x117 */ 0x5F65,  // ė
  /* 0x118 */ 0xFF45,  // Ę
  /* 0x119 */ 0xFF65,  // ę
  /* 0x11A */ 0x1445,  // Ě
  /* 0x11B */ 0x1465,  // ě
  /* 0x11C */ 0x5E47,  // Ĝ
  /* 0x11D */ 0x5E67,  // ĝ
  /* 0x11E */ 0x1547,  // Ğ
  /* 0x11F */ 0x1567,  // ğ

  /* 0x120 */ 0x5F47,  // Ġ
  /* 0x121 */ 0x5F67,  // ġ
  /* 0x122 */ 0x1847,  // Ģ
  /* 0x123 */ 0xFF67,  // ģ
  /* 0x124 */ 0x5E48,  // Ĥ
  /* 0x125 */ 0x5E68,  // ĥ
  /* 0x126 */ 0xFF48,  // Ħ
  /* 0x127 */ 0xFF68,  // ħ
  /* 0x128 */ 0x7E49,  // Ĩ
  /* 0x129 */ 0x7E10,  // ĩ
  /* 0x12A */ 0x1649,  // Ī
  /* 0x12B */ 0x1610,  // ī
  /* 0x12C */ 0x1549,  // Ĭ
  /* 0x12D */ 0x1510,  // ĭ
  /* 0x12E */ 0xFF49,  // Į
  /* 0x12F */ 0xFF69,  // į

  /* 0x130 */ 0x5F49,  // İ
  /* 0x131 */ 0xFF10,  // ı
  /* 0x132 */ 0xFFFF,  // Ĳ
  /* 0x133 */ 0xFFFF,  // ĳ
  /* 0x134 */ 0x5E4A,  // Ĵ
  /* 0x135 */ 0x5E11,  // ĵ
  /* 0x136 */ 0x184B,  // Ķ
  /* 0x137 */ 0x186B,  // ķ
  /* 0x138 */ 0xFF6B,  // ĸ
  /* 0x139 */ 0x134C,  // Ĺ
  /* 0x13A */ 0x136C,  // ĺ
  /* 0x13B */ 0x184C,  // Ļ
  /* 0x13C */ 0x186C,  // ļ
  /* 0x13D */ 0xFF4C,  // Ľ
  /* 0x13E */ 0xFF6C,  // ľ
  /* 0x13F */ 0xFF4C,  // Ŀ

  /* 0x140 */ 0xFF6C,  // ŀ
  /* 0x141 */ 0xFF4C,  // Ł
  /* 0x142 */ 0xFF6C,  // ł
  /* 0x143 */ 0x134E,  // Ń
  /* 0x144 */ 0x136E,  // ń
  /* 0x145 */ 0x184E,  // Ņ
  /* 0x146 */ 0x186E,  // ņ
  /* 0x147 */ 0x144E,  // Ň
  /* 0x148 */ 0x146E,  // ň
  /* 0x149 */ 0xFF6E,  // ŉ
  /* 0x14A */ 0xFF4E,  // Ŋ
  /* 0x14B */ 0xFF6E,  // ŋ
  /* 0x14C */ 0x164F,  // Ō
  /* 0x14D */ 0x166F,  // ō
  /* 0x14E */ 0x154F,  // Ŏ
  /* 0x14F */ 0x156F,  // ŏ

  /* 0x150 */ 0x7D4F,  // Ő
  /* 0x151 */ 0x7D6F,  // ő
  /* 0x152 */ 0xFF1E,  // Œ
  /* 0x153 */ 0xFF1B,  // œ
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
  /* 0x165 */ 0xFF74,  // ť
  /* 0x166 */ 0xFF54,  // Ŧ
  /* 0x167 */ 0xFF74,  // ŧ
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
  /* 0x172 */ 0xFF55,  // Ų
  /* 0x173 */ 0xFF75,  // ų
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
  /* 0x17F */ 0xFF20   // ſ
};
#endif

static constexpr uint16_t set2_translation_latin_1[] = {
  /* 0xA1 */ 0xFF20,  // ¡
  /* 0xA2 */ 0xFF98,  // ¢
  /* 0xA3 */ 0xFF8B,  // £
  /* 0xA4 */ 0xFF99,  // ¤
  /* 0xA5 */ 0xFF9A,  // ¥
  /* 0xA6 */ 0xFF9B,  // ¦
  /* 0xA7 */ 0xFF84,  // §
  /* 0xA8 */ 0xFF04,  // ¨
  /* 0xA9 */ 0xFF9C,  // ©
  /* 0xAA */ 0xFF9D,  // ª
  /* 0xAB */ 0xFF13,  // «
  /* 0xAC */ 0xFF9E,  // ¬
  /* 0xAD */ 0xFF2D,  // Soft hyphen
  /* 0xAE */ 0xFF9F,  // ®
  /* 0xAF */ 0xFF09,  // Macron
  /* 0xB0 */ 0xFF06,  // ° Degree
  /* 0xB1 */ 0xFFA0,  // ±
  /* 0xB2 */ 0xFFA1,  // ²
  /* 0xB3 */ 0xFFA2,  // ³
  /* 0xB4 */ 0xFF01,  // accute accent
  /* 0xB5 */ 0xFFA3,  // µ
  /* 0xB6 */ 0xFFA4,  // ¶ 
  /* 0xB7 */ 0xFFA5,  // middle dot
  /* 0xB8 */ 0xFF0B,  // cedilla
  /* 0xB9 */ 0xFFA6,  // ¹
  /* 0xBA */ 0xFFA7,  // º
  /* 0xBB */ 0xFF14,  // »
  /* 0xBC */ 0xFFA9,  // ¼
  /* 0xBD */ 0xFFAA,  // ½
  /* 0xBE */ 0xFFAB,  // ¾
  /* 0xBF */ 0xFF17,  // ¿
  /* 0xC0 */ 0x0041,  // À
  /* 0xC1 */ 0x0141,  // Á
  /* 0xC2 */ 0x0241,  // Â
  /* 0xC3 */ 0x0341,  // Ã
  /* 0xC4 */ 0x0441,  // Ä
  /* 0xC5 */ 0x0641,  // Å
  /* 0xC6 */ 0xFF8C,  // Æ
  /* 0xC7 */ 0x0B43,  // Ç
  /* 0xC8 */ 0x0045,  // È
  /* 0xC9 */ 0x0145,  // É
  /* 0xCA */ 0x0245,  // Ê
  /* 0xCB */ 0x0445,  // Ë
  /* 0xCC */ 0x0049,  // Ì
  /* 0xCD */ 0x0149,  // Í
  /* 0xCE */ 0x0249,  // Î
  /* 0xCF */ 0x0449,  // Ï
  /* 0xD0 */ 0xFF8D,  // Ð
  /* 0xD1 */ 0x034E,  // Ñ
  /* 0xD2 */ 0x004F,  // Ò
  /* 0xD3 */ 0x014F,  // Ó
  /* 0xD4 */ 0x024F,  // Ô
  /* 0xD5 */ 0x034F,  // Õ
  /* 0xD6 */ 0x044F,  // Ö
  /* 0xD7 */ 0xFFA8,  // ×
  /* 0xD8 */ 0xFF8F,  // Ø
  /* 0xD9 */ 0x0055,  // Ù
  /* 0xDA */ 0x0155,  // Ú
  /* 0xDB */ 0x0255,  // Û
  /* 0xDC */ 0x0455,  // Ü
  /* 0xDD */ 0x0159,  // Ý
  /* 0xDE */ 0xFF90,  // Þ
  /* 0xDF */ 0xFF97,  // ß
  /* 0xE0 */ 0x0061,  // à
  /* 0xE1 */ 0x0161,  // á
  /* 0xE2 */ 0x0261,  // â
  /* 0xE3 */ 0x0361,  // ã
  /* 0xE4 */ 0x0461,  // ä
  /* 0xE5 */ 0x0661,  // å
  /* 0xE6 */ 0xFF92,  // æ
  /* 0xE7 */ 0x0B63,  // ç
  /* 0xE8 */ 0x0065,  // è
  /* 0xE9 */ 0x0165,  // é
  /* 0xEA */ 0x0265,  // ê
  /* 0xEB */ 0x0465,  // ë
  /* 0xEC */ 0x0019,  // ì
  /* 0xED */ 0x0119,  // í
  /* 0xEE */ 0x0219,  // î
  /* 0xEF */ 0x0419,  // ï
  /* 0xF0 */ 0xFF93,  // ð
  /* 0xF1 */ 0x036E,  // ñ
  /* 0xF2 */ 0x006F,  // ò
  /* 0xF3 */ 0x016F,  // ó
  /* 0xF4 */ 0x026F,  // ô
  /* 0xF5 */ 0x036F,  // õ
  /* 0xF6 */ 0x046F,  // ö
  /* 0xF7 */ 0xFFAC,  // ÷
  /* 0xF8 */ 0xFF95,  // ø
  /* 0xF9 */ 0x0075,  // ù
  /* 0xFA */ 0x0175,  // ú
  /* 0xFB */ 0x0275,  // û
  /* 0xFC */ 0x0475,  // ü
  /* 0xFD */ 0x0179,  // ý
  /* 0xFE */ 0xFF96,  // þ
  /* 0xFF */ 0x0479   // ÿ
};

static constexpr uint16_t set2_translation_latin_A[] = {
  /* 0x100 */ 0x0941,  // Ā
  /* 0x101 */ 0x0961,  // ā
  /* 0x102 */ 0x0841,  // Ă
  /* 0x103 */ 0x0861,  // ă
  /* 0x104 */ 0x0C41,  // Ą
  /* 0x105 */ 0x0C61,  // ą
  /* 0x106 */ 0x0143,  // Ć
  /* 0x107 */ 0x0163,  // ć
  /* 0x108 */ 0x0243,  // Ĉ
  /* 0x109 */ 0x0263,  // ĉ
  /* 0x10A */ 0x0A43,  // Ċ
  /* 0x10B */ 0x0A63,  // ċ
  /* 0x10C */ 0x0743,  // Č
  /* 0x10D */ 0x0763,  // č
  /* 0x10E */ 0x0744,  // Ď
  /* 0x10F */ 0xFF85,  // ď

  /* 0x110 */ 0xFF8D,  // Đ
  /* 0x111 */ 0xFF83,  // đ
  /* 0x112 */ 0x0945,  // Ē
  /* 0x113 */ 0x0965,  // ē
  /* 0x114 */ 0x0845,  // Ĕ
  /* 0x115 */ 0x0865,  // ĕ
  /* 0x116 */ 0x0A45,  // Ė 
  /* 0x117 */ 0x0A65,  // ė
  /* 0x118 */ 0x0C45,  // Ę
  /* 0x119 */ 0x0C65,  // ę
  /* 0x11A */ 0x0745,  // Ě
  /* 0x11B */ 0x0765,  // ě
  /* 0x11C */ 0x0247,  // Ĝ
  /* 0x11D */ 0x0267,  // ĝ
  /* 0x11E */ 0x0847,  // Ğ
  /* 0x11F */ 0x0867,  // ğ

  /* 0x120 */ 0x0A47,  // Ġ
  /* 0x121 */ 0x0A67,  // ġ
  /* 0x122 */ 0x0B47,  // Ģ
  /* 0x123 */ 0xFF67,  // ģ   ??
  /* 0x124 */ 0x0248,  // Ĥ
  /* 0x125 */ 0x0268,  // ĥ
  /* 0x126 */ 0xFF48,  // Ħ
  /* 0x127 */ 0xFF68,  // ħ
  /* 0x128 */ 0x0349,  // Ĩ
  /* 0x129 */ 0x0319,  // ĩ
  /* 0x12A */ 0x0949,  // Ī
  /* 0x12B */ 0x0919,  // ī
  /* 0x12C */ 0x0849,  // Ĭ
  /* 0x12D */ 0x0819,  // ĭ
  /* 0x12E */ 0x0C49,  // Į
  /* 0x12F */ 0x0C69,  // į

  /* 0x130 */ 0x0A49,  // İ
  /* 0x131 */ 0xFF19,  // ı
  /* 0x132 */ 0xFF82,  // Ĳ
  /* 0x133 */ 0xFF8A,  // ĳ
  /* 0x134 */ 0x024A,  // Ĵ
  /* 0x135 */ 0x021A,  // ĵ
  /* 0x136 */ 0x0B4B,  // Ķ
  /* 0x137 */ 0x0B6B,  // ķ
  /* 0x138 */ 0xFF6B,  // ĸ   ??
  /* 0x139 */ 0x014C,  // Ĺ
  /* 0x13A */ 0x016C,  // ĺ
  /* 0x13B */ 0x0B4C,  // Ļ
  /* 0x13C */ 0x0B6C,  // ļ
  /* 0x13D */ 0xFF7F,  // Ľ
  /* 0x13E */ 0xFF86,  // ľ
  /* 0x13F */ 0xFF4C,  // Ŀ   ??

  /* 0x140 */ 0xFF6C,  // ŀ   ??
  /* 0x141 */ 0xFF80,  // Ł
  /* 0x142 */ 0xFF87,  // ł
  /* 0x143 */ 0x014E,  // Ń
  /* 0x144 */ 0x016E,  // ń
  /* 0x145 */ 0x0B4E,  // Ņ
  /* 0x146 */ 0x0B6E,  // ņ
  /* 0x147 */ 0x074E,  // Ň
  /* 0x148 */ 0x076E,  // ň
  /* 0x149 */ 0x276E,  // ŉ
  /* 0x14A */ 0xFF81,  // Ŋ
  /* 0x14B */ 0xFF88,  // ŋ
  /* 0x14C */ 0x094F,  // Ō
  /* 0x14D */ 0x096F,  // ō
  /* 0x14E */ 0x084F,  // Ŏ
  /* 0x14F */ 0x086F,  // ŏ

  /* 0x150 */ 0x054F,  // Ő
  /* 0x151 */ 0x056F,  // ő
  /* 0x152 */ 0xFF8E,  // Œ
  /* 0x153 */ 0xFF94,  // œ
  /* 0x154 */ 0x0152,  // Ŕ
  /* 0x155 */ 0x0172,  // ŕ
  /* 0x156 */ 0x0B52,  // Ŗ
  /* 0x157 */ 0x0B72,  // ŗ
  /* 0x158 */ 0x0752,  // Ř
  /* 0x159 */ 0x0772,  // ř
  /* 0x15A */ 0x0153,  // Ś
  /* 0x15B */ 0x0173,  // ś
  /* 0x15C */ 0x0253,  // Ŝ
  /* 0x15D */ 0x0273,  // ŝ
  /* 0x15E */ 0x0B53,  // Ş
  /* 0x15F */ 0x0B73,  // ş

  /* 0x160 */ 0x0753,  // Š
  /* 0x161 */ 0x0773,  // š
  /* 0x162 */ 0x0B54,  // Ţ
  /* 0x163 */ 0x0B74,  // ţ
  /* 0x164 */ 0x0754,  // Ť
  /* 0x165 */ 0xFF89,  // ť
  /* 0x166 */ 0xFF54,  // Ŧ  ??
  /* 0x167 */ 0xFF74,  // ŧ  ??
  /* 0x168 */ 0x0355,  // Ũ
  /* 0x169 */ 0x0375,  // ũ
  /* 0x16A */ 0x0955,  // Ū
  /* 0x16B */ 0x0975,  // ū
  /* 0x16C */ 0x0855,  // Ŭ
  /* 0x16D */ 0x0875,  // ŭ
  /* 0x16E */ 0x0655,  // Ů
  /* 0x16F */ 0x0675,  // ů

  /* 0x170 */ 0x0555,  // Ű
  /* 0x171 */ 0x0575,  // ű
  /* 0x172 */ 0x0C55,  // Ų
  /* 0x173 */ 0x0C75,  // ų
  /* 0x174 */ 0x0257,  // Ŵ
  /* 0x175 */ 0x0277,  // ŵ
  /* 0x176 */ 0x0259,  // Ŷ
  /* 0x177 */ 0x0279,  // ŷ
  /* 0x178 */ 0x0459,  // Ÿ
  /* 0x179 */ 0x015A,  // Ź
  /* 0x17A */ 0x017A,  // ź
  /* 0x17B */ 0x0A5A,  // Ż
  /* 0x17C */ 0x0A7A,  // ż
  /* 0x17D */ 0x075A,  // Ž
  /* 0x17E */ 0x077A,  // ž
  /* 0x17F */ 0xFFFF   // ſ   ???
};

/**
 * @brief Access to a IBMF font.
 * 
 * This is a class to allow acces to a IBMF font generated from METAFONT
 * 
 */
class IBMFFont
{
  public:
    typedef int16_t FIX16;

    #pragma pack(push, 1)
      struct LigKernStep {
        unsigned int next_char_code:8;
        union {
          unsigned int char_code:8;
          unsigned int kern_idx:8;  // Ligature: replacement char code, kern: displacement
        } u;
        unsigned int tag:1;         // 0 = Ligature, 1 = Kern
        unsigned int stop:1;
        unsigned int filler:6;
      };

      struct GlyphMetric {
        unsigned int          dyn_f:4;
        unsigned int first_is_black:1;
        unsigned int         filler:3;
      };

      struct GlyphInfo {
        uint8_t     char_code;
        uint8_t     bitmap_width;
        uint8_t     bitmap_height;
        int8_t      horizontal_offset;
        int8_t      vertical_offset;
        uint8_t     lig_kern_pgm_index; // = 255 if none
        uint16_t    packet_length;
        FIX16       advance;
        GlyphMetric glyph_metric;
      };

    //   struct Glyph {
    //     uint8_t     char_code;
    //     uint8_t     point_size;
    //     uint8_t     bitmap_width;
    //     uint8_t     bitmap_height;
    //     int8_t      horizontal_offset;
    //     int8_t      vertical_offset;
    //     int8_t      advance;
    //     uint16_t    bitmap_size;
    //     uint8_t     lig_kern_pgm_index; // = 255 if none
    //     uint8_t     pitch;
    //     uint8_t   * bitmap;
    //   };
    #pragma pack(pop)

  private:
    static constexpr char const * TAG = "IBMFFont";
    
    static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved
    static constexpr uint8_t IBMF_VERSION    =   2;

    bool initialized;
    bool memory_owner_is_the_instance;

    #pragma pack(push, 1)
      struct Preamble {
        char     marker[4];
        uint8_t  size_count;
        struct {
          uint8_t   version:5;
          uint8_t  char_set:3;
        } bits;
        uint32_t font_offsets[1];
      };

      struct Header {
        uint8_t    point_size;
        uint8_t    line_height;
        uint16_t   dpi;
        FIX16      x_height;
        FIX16      em_size;
        FIX16      slant_correction;
        uint8_t    descender_height;
        uint8_t    space_size;
        uint16_t   glyph_count;
        uint8_t    lig_kern_pgm_count;
        uint8_t    kern_count;
      };
    #pragma pack(pop)

    uint8_t     * memory;
    uint32_t      memory_length;

    uint8_t     * memory_ptr;
    uint8_t     * memory_end;

    uint32_t      repeat_count;

    Preamble    * preamble;
    uint8_t     * sizes;

    uint8_t     * current_font;
    uint8_t       current_point_size;

    Header      * header;
    GlyphInfo   * glyph_info_table[MAX_GLYPH_COUNT];
    LigKernStep * lig_kern_pgm;
    FIX16       * kerns;

    static constexpr uint8_t PK_REPEAT_COUNT =   14;
    static constexpr uint8_t PK_REPEAT_ONCE  =   15;

    GlyphInfo * glyph_info;
    Font::Glyph glyph;

    Font  & font;

    bool
    getnext8(uint8_t & val)
    {
      if (memory_ptr >= memory_end) return false;  
      val = *memory_ptr++;
      return true;
    }

    uint8_t nybble_flipper = 0xf0U;
    uint8_t nybble_byte;

    bool
    get_nybble(uint8_t & nyb)
    {
      if (nybble_flipper == 0xf0U) {
        if (!getnext8(nybble_byte)) return false;
        nyb = nybble_byte >> 4;
      }
      else {
        nyb = (nybble_byte & 0x0f);
      }
      nybble_flipper ^= 0xffU;
      return true;
    }

    // Pseudo-code:
    //
    // function pk_packed_num: integer;
    // var i,j,k: integer;
    // begin 
    //   i := get_nyb;
    //   if i = 0 then begin 
    //     repeat 
    //       j := getnyb; incr(i);
    //     until j != 0;
    //     while i > 0 do begin 
    //       j := j * 16 + get_nyb; 
    //       decr(i);
    //     end;
    //     pk_packed_num := j - 15 + (13 - dyn_f) * 16 + dyn_f;
    //   end
    //   else if i <= dyn_f then 
    //     pk_packed_num := i
    //   else if i < 14 then 
    //     pk_packed_num := (i - dyn_f - 1) * 16 + get_nyb + dyn_f + 1
    //   else begin 
    //     if repeat_count != 0 then abort('Extra repeat count!');
    //     if i = 14 then
    //        repeat_count := pk_packed_num
    //     else
    //        repeat_count := 1;
    //     send_out(true, repeat_count);
    //     pk_packed_num := pk_packed_num;
    //   end;
    // end;

    bool
    get_packed_number(uint32_t & val, const GlyphInfo & glyph)
    {
      uint8_t  nyb;
      uint32_t i, j;

      uint8_t dyn_f = glyph.glyph_metric.dyn_f;

      while (true) {
        if (!get_nybble(nyb)) return false; 
        i = nyb;
        if (i == 0) {
          do {
            if (!get_nybble(nyb)) return false;
            i++;
          } while (nyb == 0);
          j = nyb;
          while (i-- > 0) {
            if (!get_nybble(nyb)) return false;
            j = (j << 4) + nyb;
          }
          val = j - 15 + ((13 - dyn_f) << 4) + dyn_f;
          break;
        }
        else if (i <= dyn_f) {
          val = i;
          break;
        }
        else if (i < PK_REPEAT_COUNT) {
          if (!get_nybble(nyb)) return false;
          val = ((i - dyn_f - 1) << 4) + nyb + dyn_f + 1;
          break;
        }
        else { 
          // if (repeat_count != 0) {
          //   std::cerr << "Spurious repeat_count iteration!" << std::endl;
          //   return false;
          // }
          if (i == PK_REPEAT_COUNT) {
            if (!get_packed_number(repeat_count, glyph)) return false;
          }
          else { // i == PK_REPEAT_ONCE
            repeat_count = 1;
          }
        }
      }
      return true;
    }

    bool
    retrieve_bitmap(GlyphInfo * glyph_info, uint8_t * bitmap, Dim dim, Pos offsets)
    {
      // point on the glyphs' bitmap definition
      memory_ptr = ((uint8_t *)glyph_info) + sizeof(GlyphInfo);
      uint8_t * rowp;

      if (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) {
        uint32_t  row_size = (dim.width + 7) >> 3;
        rowp = bitmap + (offsets.y * row_size);

        if (glyph_info->glyph_metric.dyn_f == 14) {  // is a bitmap?
          uint32_t  count = 8;
          uint8_t   data;

          for (uint32_t row = 0; 
               row < glyph_info->bitmap_height; 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < glyph_info->bitmap_width + offsets.x; 
                 col++) {
              if (count >= 8) {
                if (!getnext8(data)) {
                  std::cerr << "Not enough bitmap data!" << std::endl;
                  return false;
                }
                // std::cout << std::hex << +data << ' ';
                count = 0;
              }
              if (data & (0x80U >> count)) rowp[col >> 3] |= (0x80U >> (col & 7));
              count++;
            }
          }
          // std::cout << std::endl;
        }
        else {
          uint32_t count = 0;

          repeat_count   = 0;
          nybble_flipper = 0xf0U;

          bool black = !(glyph_info->glyph_metric.first_is_black == 1);

          for (uint32_t row = 0; 
               row < glyph_info->bitmap_height; 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < glyph_info->bitmap_width + offsets.x; 
                 col++) {
              if (count == 0) {
                if (!get_packed_number(count, *glyph_info)) {
                  return false;
                }
                black = !black;
                // if (black) {
                //   std::cout << count << ' ';
                // }
                // else {
                //   std::cout << '(' << count << ')' << ' ';
                // }
              }
              if (black) rowp[col >> 3] |= (0x80U >> (col & 0x07));
              count--;
            }

            // if (repeat_count != 0) std::cout << "Repeat count: " << repeat_count << std::endl;
            while ((row < glyph_info->bitmap_height) && (repeat_count-- > 0)) {
              bcopy(rowp, 
                    rowp + row_size, 
                    row_size);
              row++;
              rowp += row_size;
            }

            repeat_count = 0;
          }
          // std::cout << std::endl;
        }
      }
      else {
        uint32_t  row_size = dim.width;
        rowp = bitmap + (offsets.y * row_size);

        repeat_count   = 0;
        nybble_flipper = 0xf0U;

        if (glyph_info->glyph_metric.dyn_f == 14) {  // is a bitmap?
          uint32_t  count = 8;
          uint8_t   data;

          for (uint32_t row = 0; 
               row < (glyph_info->bitmap_height); 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < (glyph_info->bitmap_width + offsets.x); 
                 col++) {
              if (count >= 8) {
                if (!getnext8(data)) {
                  std::cerr << "Not enough bitmap data!" << std::endl;
                  return false;
                }
                // std::cout << std::hex << +data << ' ';
                count = 0;
              }
              rowp[col] = (data & (0x80U >> count)) ? 0xFF : 0;
              count++;
            }
          }
          // std::cout << std::endl;
        }
        else {
          uint32_t count = 0;

          repeat_count   = 0;
          nybble_flipper = 0xf0U;

          bool black = !(glyph_info->glyph_metric.first_is_black == 1);

          for (uint32_t row = 0; 
               row < (glyph_info->bitmap_height); 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < (glyph_info->bitmap_width + offsets.x); 
                 col++) {
              if (count == 0) {
                if (!get_packed_number(count, *glyph_info)) {
                  return false;
                }
                black = !black;
                // if (black) {
                //   std::cout << count << ' ';
                // }
                // else {
                //   std::cout << '(' << count << ')' << ' ';
                // }
              }
              if (black) rowp[col] = 0xFF;
              count--;
            }

            // if (repeat_count != 0) std::cout << "Repeat count: " << repeat_count << std::endl;
            while ((row < dim.height) && (repeat_count-- > 0)) {
              bcopy(rowp, 
                    rowp + row_size, 
                    row_size);
              row++;
              rowp += row_size;
            }

            repeat_count = 0;
          }
          // std::cout << std::endl;
        }
      }
      return true;
    }

    bool
    load_preamble()
    {
      preamble = (Preamble *) memory;
      if (strncmp("IBMF", preamble->marker, 4) != 0) return false;
      if (preamble->bits.version != IBMF_VERSION) return false;
      sizes = (uint8_t *) &memory[6 + (preamble->size_count * 4)];
      current_font = nullptr;

      return true;
    }

    bool
    load_data()
    {
      //for (uint8_t i = 0; i < MAX_GLYPH_COUNT; i++) glyph_data_table[i] = nullptr;
      memset(glyph_info_table, 0, sizeof(glyph_info_table));

      memory_ptr = current_font;

      header = (Header *) current_font;

      memory_ptr += sizeof(Header);
      for (int i = 0; i < header->glyph_count; i++) {
        glyph_info = (GlyphInfo *) memory_ptr;
        glyph_info_table[glyph_info->char_code] = (GlyphInfo *) memory_ptr;
        memory_ptr += sizeof(GlyphInfo) + glyph_info->packet_length;
        if (memory_ptr > memory_end) return false;
      }

      lig_kern_pgm = (LigKernStep *) memory_ptr;
      memory_ptr += sizeof(LigKernStep) * header->lig_kern_pgm_count;
      if (memory_ptr > memory_end) return false;

      kerns = (FIX16 *) memory_ptr;

      memory_ptr += sizeof(FIX16) * header->kern_count;
      if (memory_ptr > memory_end) return false;

      return true;
    }

  public:

    IBMFFont(uint8_t * memory_font, uint32_t size, Font & font) 
        : memory(memory_font), 
          memory_length(size),
          font(font) { 
            
      memory_end = memory + memory_length;
      initialized = load_preamble();
      memory_owner_is_the_instance = false;
      if (!initialized) {
        std::cerr << "Font data not recognized!" << std::endl;
      }
    }

    IBMFFont(const std::string filename, Font & font) : font(font) {

      struct stat file_stat;

      initialized = false;

      if (stat(filename.c_str(), &file_stat) != -1) {
        FILE * file = fopen(filename.c_str(), "rb");
        memory = new uint8_t[memory_length = file_stat.st_size];
        memory_end = (memory == nullptr) ? nullptr : memory + memory_length;
        memory_owner_is_the_instance = true;

        if (memory != nullptr) {
          if (fread(memory, memory_length, 1, file) == 1) {
            initialized = load_preamble();
          }
          if (!initialized) {
            std::cerr << "Font data not recognized!" << std::endl;
          }
        }
        fclose(file);
      }
      else {
        std::cerr << "Unable to find font file %s!" << filename.c_str() << std::endl;
      }
    }

   ~IBMFFont() {
      if (memory_owner_is_the_instance && (memory != nullptr)) {
        delete [] memory;
        memory = nullptr;
      }
    }

    inline uint8_t                        get_font_size() { return  header->point_size;                }
    inline uint16_t                     get_line_height() { return  header->line_height;               }
    inline uint16_t                    get_chars_height() { return  header->em_size >> 6;              }
    inline int16_t                 get_descender_height() { return -(int16_t)header->descender_height; }
    inline LigKernStep        * get_lig_kern(uint8_t idx) { return &lig_kern_pgm[idx];                 }
    inline FIX16                      get_kern(uint8_t i) { return kerns[i];                           }
    inline GlyphInfo * get_glyph_info(uint8_t glyph_code) { return glyph_info_table[glyph_code];       }
    inline uint8_t                         get_char_set() { return preamble->bits.char_set;            }

    /**
     * @brief Translate unicode in an internal char code
     * 
     * The class allow for latin1+ characters to be plotted into a bitmap. As the
     * font doesn't contains all accented glyphs, a combination of glyphs must be
     * used to draw a single bitmap. This method translate some of the supported
     * unicode values to that combination. The least significant byte will contains 
     * the main glyph code and the next byte will contain the accent code. 
     * 
     * @param charcode The character code in unicode
     * @return The internal representation of a character
     */
    uint32_t translate(uint32_t charcode)
    {
      uint32_t glyph_code;

      if (preamble->bits.char_set == 0) {
        if ((charcode > 0x20) && (charcode < 0x7F)) {
          glyph_code = 0xFF00 | charcode;
        }
        else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
          glyph_code = set2_translation_latin_1[charcode - 0xA1];
        }
        else if ((charcode >= 0x100) && (charcode <= 0x1FF)) {
          glyph_code = set2_translation_latin_A[charcode - 0x100];
        }
        else {
          switch (charcode) {
            case 0x2013: glyph_code = 0xFF15; break; // endash
            case 0x2014: glyph_code = 0xFF16; break; // emdash
            case 0x2018: // quote left
            case 0x02BB: // reverse apostrophe
              glyph_code = 0xFF60; break;
            case 0x2019: // quote right
            case 0x02BC: // apostrophe
              glyph_code = 0xFF27; break;
            case 0x201C: glyph_code = 0xFF10; break; // quoted left "
            case 0x201D: glyph_code = 0xFF11; break; // quoted right
            case 0x02C6: glyph_code = 0xFF5E; break; // circumflex
            case 0x02DA: glyph_code = 0xFF06; break; // ring
            case 0x02DC: glyph_code = 0xFF7E; break; // tilde ~
            case 0x201A: glyph_code = 0xFF0D; break; // comma like ,
            case 0x2032: glyph_code = 0xFF27; break; // minute '
            case 0x2033: glyph_code = 0xFF22; break; // second "
            case 0x2044: glyph_code = 0xFF2F; break; // fraction /
            case 0x20AC: glyph_code = 0xFFAD; break; // euro
            default:
              glyph_code = 0xFFFE;
          }
        }
      }
      else {
        glyph_code = 0xFFFE;
      }

      return glyph_code;
    }

    bool
    get_glyph(uint32_t      glyph_code,
              Font::Glyph & app_glyph,
              GlyphInfo  ** glyph_data, 
              bool          load_bitmap)
    {
      //uint32_t glyph_code = translate(char_code);

      glyph.clear();

      uint8_t     accent      = (glyph_code & 0x0000FF00) >> 8;
      GlyphInfo * accent_info = (accent != 0xFF) ? glyph_info_table[accent] : nullptr;

      if (((glyph_code & 0xFF) == 0xFF) ||
          (((glyph_code & 0xFF) < header->glyph_count) && 
           (glyph_info_table[glyph_code & 0xFF] == nullptr))) {
        std::cerr << "No entry for char code 0x" 
                  << std::hex << glyph_code
                  << std::endl;         
        glyph_code = 0xFFFE;
      }

      if (glyph_code == 0xFFFE) {
        glyph.advance  =   header->space_size;
        glyph.line_height = header->line_height;

        app_glyph  = glyph;
       *glyph_data = nullptr;
       
        return true;
      }

      glyph_info = glyph_info_table[glyph_code & 0xFF];

      if (glyph_info == nullptr) return false;
      
      Dim dim     = Dim(glyph_info->bitmap_width, glyph_info->bitmap_height);
      Pos offsets = Pos(0, 0);

      uint8_t added_left = 0;

      if (accent_info != nullptr) {

        if (glyph_code == 0x276E) {  // Apostrophe n
          // offsets.x = 0; // already set
          added_left = accent_info->bitmap_width + 1 - (((header->x_height >> 6) * header->slant_correction) >> 6);
          dim.width  = added_left + glyph_info->bitmap_width;
        }
        else {
          // Horizontal adjustment
          if (glyph_code == 0x0C41) { // Ą 
            offsets.x = glyph_info->bitmap_width - accent_info->bitmap_width;
          }
          else if ((glyph_code == 0x0C61) || (glyph_code == 0x0C45)) { // ą or Ę
            offsets.x = glyph_info->bitmap_width - accent_info->bitmap_width - ((((int32_t)glyph_info->bitmap_height) * header->slant_correction) >> 6);
          }
          else {
            offsets.x = ((glyph_info->bitmap_width > accent_info->bitmap_width) ?
                            ((glyph_info->bitmap_width - accent_info->bitmap_width) >> 1) : 0)
                        + ((accent_info->vertical_offset < 5) ? 
                             - (((header->x_height >> 6) * header->slant_correction) >> 6) :
                            ((((int32_t)glyph_info->bitmap_height) * header->slant_correction) >> 6))
                        /*- (accent_info->horizontal_offset - glyph_info->horizontal_offset)*/;
          }
          if ((offsets.x == 0) && (glyph_info->bitmap_width < accent_info->bitmap_width))  {
            added_left = (accent_info->bitmap_width - glyph_info->bitmap_width) >> 1;
            dim.width = accent_info->bitmap_width;
          }
          if (dim.width < (offsets.x + accent_info->bitmap_width)) {
            dim.width = offsets.x + accent_info->bitmap_width;
          }
        }

        // Vertical adjustment
        if (accent_info->vertical_offset >= (header->x_height >> 6)) {
          // Accents that are on top of a main glyph
          dim.height += (accent_info->vertical_offset - (header->x_height >> 6));
        }
        else if (accent_info->vertical_offset < 5) {
          // Accents below the main glyph (cedilla)
          int16_t added_height = (glyph_info->bitmap_height - glyph_info->vertical_offset) - 
                                 ((-accent_info->vertical_offset) + accent_info->bitmap_height);
          if (added_height < 0) dim.height += -added_height;
          offsets.y = glyph_info->vertical_offset - accent_info->vertical_offset;
        }
        // else if ((glyph_code == 0x1648) || (glyph_code == 0x1568)) { // Ħ or ħ
        //   offsets.y = (glyph_info->bitmap_height * 1) >> 2;
        //   offsets.x = ((glyph_info->bitmap_height - offsets.y) * header->slant_correction) >> 6;
        // }
        // else if ((glyph_code == 0x1554) || (glyph_code == 0x1574)) { // Ŧ or ŧ 
        //   offsets.y = (glyph_info->bitmap_height * 1) >> 1;
        //   offsets.x = (offsets.y * header->slant_correction) >> 6;
        // }
      }

      uint16_t size = (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) ?
          dim.height * ((dim.width + 7) >> 3) : dim.height * dim.width;

      glyph.buffer = font.byte_pool_alloc(size);
      memset(glyph.buffer, 0, size);

      if (accent_info != nullptr) {
        if (load_bitmap) retrieve_bitmap(accent_info, glyph.buffer, dim, offsets);

        offsets.y = (accent_info->vertical_offset >=  (header->x_height >> 6)) ?
          (accent_info->vertical_offset - (header->x_height >> 6)) : 0;
        offsets.x = added_left;
      }

      if (load_bitmap) retrieve_bitmap(glyph_info, glyph.buffer, dim, offsets);

      glyph.dim      =   dim;
      glyph.xoff     =  -(glyph_info->horizontal_offset + offsets.x);
      glyph.yoff     =  -(glyph_info->vertical_offset   + offsets.y);
      glyph.advance  =   glyph_info->advance >> 6;
      glyph.pitch    =   (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) ?
                                     (dim.width + 7) >> 3 : dim.width;
      glyph.ligature_and_kern_pgm_index = glyph_info->lig_kern_pgm_index;
      glyph.line_height = header->line_height;

      app_glyph  = glyph;
     *glyph_data = glyph_info;

      return true;
    }

    bool
    set_font_size(uint8_t size)
    {
      if (!initialized) return false;
      uint8_t i = 0;
      while ((i < preamble->size_count) && (sizes[i] <= size)) i++;
      if (i > 0) i--;
      current_font = memory + preamble->font_offsets[i];
      current_point_size = sizes[i];
      return load_data();
    }

    bool
    show_glyph_info(const GlyphInfo & glyph, uint8_t char_code)
    {
      std::cout << "Glyph Char Code: " << char_code << std::endl  
                << "  Metrics: [" << std::dec
                <<      glyph.bitmap_width  << ", " 
                <<      glyph.bitmap_height << "] "
                <<      glyph.packet_length << std::endl
                << "  Position: ["
                <<      glyph.horizontal_offset << ", "
                <<      glyph.vertical_offset << ']' << std::endl;
      return true;
    }

    bool
    show_glyph(const Font::Glyph & glyph, uint32_t char_code)
    {
      std::cout << "Glyph Char Code: " << std::hex << char_code << std::dec << std::endl
                << "  Metrics: [" << std::dec
                <<      glyph.dim.width  << ", " 
                <<      glyph.dim.height << "] " << std::endl
                << "  Position: ["
                <<      glyph.xoff << ", "
                <<      glyph.yoff << ']' << std::endl
                 << "  Bitmap available: " 
                <<      ((glyph.buffer == nullptr) ? "No" : "Yes") << std::endl;

      if (glyph.buffer == nullptr) return true;

      uint32_t        row, col;
      uint32_t        row_size = glyph.pitch;
      const uint8_t * row_ptr;

      std::cout << '+';
      for (col = 0; col < glyph.dim.width; col++) std::cout << '-';
      std::cout << '+' << std::endl;

      for (row = 0, row_ptr = glyph.buffer; 
           row < glyph.dim.height; 
           row++, row_ptr += row_size) {
        std::cout << '|';
        for (col = 0; col < glyph.dim.width; col++) {
          std::cout << ((row_ptr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
        }
        std::cout << '|';
        std::cout << std::endl;
      }

      std::cout << '+';
      for (col = 0; col < glyph.dim.width; col++) std::cout << '-';
      std::cout << '+' << std::endl << std::endl;

      return true;
    }

    inline bool is_initialized() { return initialized; }
};