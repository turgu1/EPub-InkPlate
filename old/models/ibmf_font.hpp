#pragma once

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

#include "font.hpp"
#include "global.hpp"
#include "screen.hpp"

#include "sys/stat.h"

#if 0
static constexpr uint16_t translationLatin1[] = {
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

static constexpr uint16_t translationLatinA[] = {
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

static constexpr uint16_t set2TranslationLatin1[] = {
    /* 0xA1 */ 0xFF20, // ¡
    /* 0xA2 */ 0xFF98, // ¢
    /* 0xA3 */ 0xFF8B, // £
    /* 0xA4 */ 0xFF99, // ¤
    /* 0xA5 */ 0xFF9A, // ¥
    /* 0xA6 */ 0xFF9B, // ¦
    /* 0xA7 */ 0xFF84, // §
    /* 0xA8 */ 0xFF04, // ¨
    /* 0xA9 */ 0xFF9C, // ©
    /* 0xAA */ 0xFF9D, // ª
    /* 0xAB */ 0xFF13, // «
    /* 0xAC */ 0xFF9E, // ¬
    /* 0xAD */ 0xFF2D, // Soft hyphen
    /* 0xAE */ 0xFF9F, // ®
    /* 0xAF */ 0xFF09, // Macron
    /* 0xB0 */ 0xFF06, // ° Degree
    /* 0xB1 */ 0xFFA0, // ±
    /* 0xB2 */ 0xFFA1, // ²
    /* 0xB3 */ 0xFFA2, // ³
    /* 0xB4 */ 0xFF01, // accute accent
    /* 0xB5 */ 0xFFA3, // µ
    /* 0xB6 */ 0xFFA4, // ¶
    /* 0xB7 */ 0xFFA5, // middle dot
    /* 0xB8 */ 0xFF0B, // cedilla
    /* 0xB9 */ 0xFFA6, // ¹
    /* 0xBA */ 0xFFA7, // º
    /* 0xBB */ 0xFF14, // »
    /* 0xBC */ 0xFFA9, // ¼
    /* 0xBD */ 0xFFAA, // ½
    /* 0xBE */ 0xFFAB, // ¾
    /* 0xBF */ 0xFF17, // ¿
    /* 0xC0 */ 0x0041, // À
    /* 0xC1 */ 0x0141, // Á
    /* 0xC2 */ 0x0241, // Â
    /* 0xC3 */ 0x0341, // Ã
    /* 0xC4 */ 0x0441, // Ä
    /* 0xC5 */ 0x0641, // Å
    /* 0xC6 */ 0xFF8C, // Æ
    /* 0xC7 */ 0x0B43, // Ç
    /* 0xC8 */ 0x0045, // È
    /* 0xC9 */ 0x0145, // É
    /* 0xCA */ 0x0245, // Ê
    /* 0xCB */ 0x0445, // Ë
    /* 0xCC */ 0x0049, // Ì
    /* 0xCD */ 0x0149, // Í
    /* 0xCE */ 0x0249, // Î
    /* 0xCF */ 0x0449, // Ï
    /* 0xD0 */ 0xFF8D, // Ð
    /* 0xD1 */ 0x034E, // Ñ
    /* 0xD2 */ 0x004F, // Ò
    /* 0xD3 */ 0x014F, // Ó
    /* 0xD4 */ 0x024F, // Ô
    /* 0xD5 */ 0x034F, // Õ
    /* 0xD6 */ 0x044F, // Ö
    /* 0xD7 */ 0xFFA8, // ×
    /* 0xD8 */ 0xFF8F, // Ø
    /* 0xD9 */ 0x0055, // Ù
    /* 0xDA */ 0x0155, // Ú
    /* 0xDB */ 0x0255, // Û
    /* 0xDC */ 0x0455, // Ü
    /* 0xDD */ 0x0159, // Ý
    /* 0xDE */ 0xFF90, // Þ
    /* 0xDF */ 0xFF97, // ß
    /* 0xE0 */ 0x0061, // à
    /* 0xE1 */ 0x0161, // á
    /* 0xE2 */ 0x0261, // â
    /* 0xE3 */ 0x0361, // ã
    /* 0xE4 */ 0x0461, // ä
    /* 0xE5 */ 0x0661, // å
    /* 0xE6 */ 0xFF92, // æ
    /* 0xE7 */ 0x0B63, // ç
    /* 0xE8 */ 0x0065, // è
    /* 0xE9 */ 0x0165, // é
    /* 0xEA */ 0x0265, // ê
    /* 0xEB */ 0x0465, // ë
    /* 0xEC */ 0x0019, // ì
    /* 0xED */ 0x0119, // í
    /* 0xEE */ 0x0219, // î
    /* 0xEF */ 0x0419, // ï
    /* 0xF0 */ 0xFF93, // ð
    /* 0xF1 */ 0x036E, // ñ
    /* 0xF2 */ 0x006F, // ò
    /* 0xF3 */ 0x016F, // ó
    /* 0xF4 */ 0x026F, // ô
    /* 0xF5 */ 0x036F, // õ
    /* 0xF6 */ 0x046F, // ö
    /* 0xF7 */ 0xFFAC, // ÷
    /* 0xF8 */ 0xFF95, // ø
    /* 0xF9 */ 0x0075, // ù
    /* 0xFA */ 0x0175, // ú
    /* 0xFB */ 0x0275, // û
    /* 0xFC */ 0x0475, // ü
    /* 0xFD */ 0x0179, // ý
    /* 0xFE */ 0xFF96, // þ
    /* 0xFF */ 0x0479  // ÿ
};

static constexpr uint16_t set2TranslationLatinA[] = {
    /* 0x100 */ 0x0941, // Ā
    /* 0x101 */ 0x0961, // ā
    /* 0x102 */ 0x0841, // Ă
    /* 0x103 */ 0x0861, // ă
    /* 0x104 */ 0x0C41, // Ą
    /* 0x105 */ 0x0C61, // ą
    /* 0x106 */ 0x0143, // Ć
    /* 0x107 */ 0x0163, // ć
    /* 0x108 */ 0x0243, // Ĉ
    /* 0x109 */ 0x0263, // ĉ
    /* 0x10A */ 0x0A43, // Ċ
    /* 0x10B */ 0x0A63, // ċ
    /* 0x10C */ 0x0743, // Č
    /* 0x10D */ 0x0763, // č
    /* 0x10E */ 0x0744, // Ď
    /* 0x10F */ 0xFF85, // ď

    /* 0x110 */ 0xFF8D, // Đ
    /* 0x111 */ 0xFF83, // đ
    /* 0x112 */ 0x0945, // Ē
    /* 0x113 */ 0x0965, // ē
    /* 0x114 */ 0x0845, // Ĕ
    /* 0x115 */ 0x0865, // ĕ
    /* 0x116 */ 0x0A45, // Ė
    /* 0x117 */ 0x0A65, // ė
    /* 0x118 */ 0x0C45, // Ę
    /* 0x119 */ 0x0C65, // ę
    /* 0x11A */ 0x0745, // Ě
    /* 0x11B */ 0x0765, // ě
    /* 0x11C */ 0x0247, // Ĝ
    /* 0x11D */ 0x0267, // ĝ
    /* 0x11E */ 0x0847, // Ğ
    /* 0x11F */ 0x0867, // ğ

    /* 0x120 */ 0x0A47, // Ġ
    /* 0x121 */ 0x0A67, // ġ
    /* 0x122 */ 0x0B47, // Ģ
    /* 0x123 */ 0xFF67, // ģ   ??
    /* 0x124 */ 0x0248, // Ĥ
    /* 0x125 */ 0x0268, // ĥ
    /* 0x126 */ 0xFF48, // Ħ
    /* 0x127 */ 0xFF68, // ħ
    /* 0x128 */ 0x0349, // Ĩ
    /* 0x129 */ 0x0319, // ĩ
    /* 0x12A */ 0x0949, // Ī
    /* 0x12B */ 0x0919, // ī
    /* 0x12C */ 0x0849, // Ĭ
    /* 0x12D */ 0x0819, // ĭ
    /* 0x12E */ 0x0C49, // Į
    /* 0x12F */ 0x0C69, // į

    /* 0x130 */ 0x0A49, // İ
    /* 0x131 */ 0xFF19, // ı
    /* 0x132 */ 0xFF82, // Ĳ
    /* 0x133 */ 0xFF8A, // ĳ
    /* 0x134 */ 0x024A, // Ĵ
    /* 0x135 */ 0x021A, // ĵ
    /* 0x136 */ 0x0B4B, // Ķ
    /* 0x137 */ 0x0B6B, // ķ
    /* 0x138 */ 0xFF6B, // ĸ   ??
    /* 0x139 */ 0x014C, // Ĺ
    /* 0x13A */ 0x016C, // ĺ
    /* 0x13B */ 0x0B4C, // Ļ
    /* 0x13C */ 0x0B6C, // ļ
    /* 0x13D */ 0xFF7F, // Ľ
    /* 0x13E */ 0xFF86, // ľ
    /* 0x13F */ 0xFF4C, // Ŀ   ??

    /* 0x140 */ 0xFF6C, // ŀ   ??
    /* 0x141 */ 0xFF80, // Ł
    /* 0x142 */ 0xFF87, // ł
    /* 0x143 */ 0x014E, // Ń
    /* 0x144 */ 0x016E, // ń
    /* 0x145 */ 0x0B4E, // Ņ
    /* 0x146 */ 0x0B6E, // ņ
    /* 0x147 */ 0x074E, // Ň
    /* 0x148 */ 0x076E, // ň
    /* 0x149 */ 0x276E, // ŉ
    /* 0x14A */ 0xFF81, // Ŋ
    /* 0x14B */ 0xFF88, // ŋ
    /* 0x14C */ 0x094F, // Ō
    /* 0x14D */ 0x096F, // ō
    /* 0x14E */ 0x084F, // Ŏ
    /* 0x14F */ 0x086F, // ŏ

    /* 0x150 */ 0x054F, // Ő
    /* 0x151 */ 0x056F, // ő
    /* 0x152 */ 0xFF8E, // Œ
    /* 0x153 */ 0xFF94, // œ
    /* 0x154 */ 0x0152, // Ŕ
    /* 0x155 */ 0x0172, // ŕ
    /* 0x156 */ 0x0B52, // Ŗ
    /* 0x157 */ 0x0B72, // ŗ
    /* 0x158 */ 0x0752, // Ř
    /* 0x159 */ 0x0772, // ř
    /* 0x15A */ 0x0153, // Ś
    /* 0x15B */ 0x0173, // ś
    /* 0x15C */ 0x0253, // Ŝ
    /* 0x15D */ 0x0273, // ŝ
    /* 0x15E */ 0x0B53, // Ş
    /* 0x15F */ 0x0B73, // ş

    /* 0x160 */ 0x0753, // Š
    /* 0x161 */ 0x0773, // š
    /* 0x162 */ 0x0B54, // Ţ
    /* 0x163 */ 0x0B74, // ţ
    /* 0x164 */ 0x0754, // Ť
    /* 0x165 */ 0xFF89, // ť
    /* 0x166 */ 0xFF54, // Ŧ  ??
    /* 0x167 */ 0xFF74, // ŧ  ??
    /* 0x168 */ 0x0355, // Ũ
    /* 0x169 */ 0x0375, // ũ
    /* 0x16A */ 0x0955, // Ū
    /* 0x16B */ 0x0975, // ū
    /* 0x16C */ 0x0855, // Ŭ
    /* 0x16D */ 0x0875, // ŭ
    /* 0x16E */ 0x0655, // Ů
    /* 0x16F */ 0x0675, // ů

    /* 0x170 */ 0x0555, // Ű
    /* 0x171 */ 0x0575, // ű
    /* 0x172 */ 0x0C55, // Ų
    /* 0x173 */ 0x0C75, // ų
    /* 0x174 */ 0x0257, // Ŵ
    /* 0x175 */ 0x0277, // ŵ
    /* 0x176 */ 0x0259, // Ŷ
    /* 0x177 */ 0x0279, // ŷ
    /* 0x178 */ 0x0459, // Ÿ
    /* 0x179 */ 0x015A, // Ź
    /* 0x17A */ 0x017A, // ź
    /* 0x17B */ 0x0A5A, // Ż
    /* 0x17C */ 0x0A7A, // ż
    /* 0x17D */ 0x075A, // Ž
    /* 0x17E */ 0x077A, // ž
    /* 0x17F */ 0xFFFF  // ſ   ???
};

/**
 * @brief Access to a IBMF font.
 *
 * This is a class to allow acces to a IBMF font generated from METAFONT
 *
 */
class IBMFFont {
public:
  using FIX16 = int16_t;

  #pragma pack(push, 1)
  struct LigKernStep {
    unsigned int nextCharCode : 8;
    union {
      unsigned int charCode : 8;
      unsigned int kernIdx : 8; // Ligature: replacement char code, kern: displacement
    } u;
    unsigned int tag : 1; // 0 = Ligature, 1 = Kern
    unsigned int stop : 1;
    unsigned int filler : 6;
  };

  struct GlyphMetric {
    unsigned int dynF : 4;
    unsigned int firstIsBlack : 1;
    unsigned int filler : 3;
  };

  struct GlyphInfo {
    uint8_t charCode;
    uint8_t bitmapWidth;
    uint8_t bitmapHeight;
    int8_t horizontalOffset;
    int8_t verticalOffset;
    uint8_t ligKernPgmIndex; // = 255 if none
    uint16_t packetLength;
    FIX16 advance;
    GlyphMetric glyphMetric;
  };

  //   struct Glyph {
  //     uint8_t     charCode;
  //     uint8_t     pointSize;
  //     uint8_t     bitmapWidth;
  //     uint8_t     bitmapHeight;
  //     int8_t      horizontalOffset;
  //     int8_t      verticalOffset;
  //     int8_t      advance;
  //     uint16_t    bitmapSize;
  //     uint8_t     ligKernPgmIndex; // = 255 if none
  //     uint8_t     pitch;
  //     uint8_t   * bitmap;
  //   };
  #pragma pack(pop)

private:
  static constexpr char const *TAG = "IBMFFont";

  static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved
  static constexpr uint8_t IBMF_VERSION    = 2;

  bool initialized;

  #pragma pack(push, 1)
  struct Preamble {
    char marker[4];
    uint8_t sizeCount;
    struct {
      uint8_t version : 5;
      uint8_t charSet : 3;
    } bits;
    uint32_t fontOffsets[1];
  };

  struct Header {
    uint8_t pointSize;
    uint8_t lineHeight;
    uint16_t dpi;
    FIX16 xHeight;
    FIX16 emSize;
    FIX16 slantCorrection;
    uint8_t descenderHeight;
    uint8_t spaceSize;
    uint16_t glyphCount;
    uint8_t ligKernPgmCount;
    uint8_t kernCount;
  };
  #pragma pack(pop)

  std::unique_ptr<uint8_t[]> ownedMemory{};
  uint8_t *memory{nullptr};
  uint32_t memoryLength{0};

  uint8_t *memoryPtr;
  uint8_t *memoryEnd;

  uint32_t repeatCount;

  Preamble *preamble;
  uint8_t *sizes;

  uint8_t *currentFont;
  uint8_t currentPointSize;

  Header *header;
  GlyphInfo *glyphInfoTable[MAX_GLYPH_COUNT];
  LigKernStep *ligKernPgm;
  FIX16 *kerns;

  static constexpr uint8_t PK_REPEAT_COUNT = 14;
  static constexpr uint8_t PK_REPEAT_ONCE  = 15;

  GlyphInfo *glyphInfo;
  Glyph glyph;

  Font &font;

  auto getnext8(uint8_t &val) -> bool {
    if (memoryPtr >= memoryEnd) return false;
    val = *memoryPtr++;
    return true;
  }

  uint8_t nybbleFlipper = 0xf0U;
  uint8_t nybbleByte;

  auto getNybble(uint8_t &nyb) -> bool {
    if (nybbleFlipper == 0xf0U) {
      if (!getnext8(nybbleByte)) return false;
      nyb = nybbleByte >> 4;
    } else {
      nyb = (nybbleByte & 0x0f);
    }
    nybbleFlipper ^= 0xffU;
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
  //     pk_packed_num := j - 15 + (13 - dynF) * 16 + dynF;
  //   end
  //   else if i <= dynF then
  //     pk_packed_num := i
  //   else if i < 14 then
  //     pk_packed_num := (i - dynF - 1) * 16 + get_nyb + dynF + 1
  //   else begin
  //     if repeatCount != 0 then abort('Extra repeat count!');
  //     if i = 14 then
  //        repeatCount := pk_packed_num
  //     else
  //        repeatCount := 1;
  //     send_out(true, repeatCount);
  //     pk_packed_num := pk_packed_num;
  //   end;
  // end;

  auto getPackedNumber(uint32_t &val, const GlyphInfo &glyph) -> bool {
    uint8_t nyb;
    uint32_t i, j;

    uint8_t dynF = glyph.glyphMetric.dynF;

    while (true) {
      if (!getNybble(nyb)) return false;
      i = nyb;
      if (i == 0) {
        do {
          if (!getNybble(nyb)) return false;
          ++i;
        } while (nyb == 0);
        j = nyb;
        while (i-- > 0) {
          if (!getNybble(nyb)) return false;
          j = (j << 4) + nyb;
        }
        val = j - 15 + ((13 - dynF) << 4) + dynF;
        break;
      } else if (i <= dynF) {
        val = i;
        break;
      } else if (i < PK_REPEAT_COUNT) {
        if (!getNybble(nyb)) return false;
        val = ((i - dynF - 1) << 4) + nyb + dynF + 1;
        break;
      } else {
        // if (repeatCount != 0) {
        //   std::cerr << "Spurious repeatCount iteration!" << std::endl;
        //   return false;
        // }
        if (i == PK_REPEAT_COUNT) {
          if (!getPackedNumber(repeatCount, glyph)) return false;
        } else { // i == PK_REPEAT_ONCE
          repeatCount = 1;
        }
      }
    }
    return true;
  }

  auto retrieveBitmap(GlyphInfo *glyphInfo, uint8_t *bitmap, Dim dim, Pos offsets) -> bool {
    // point on the glyphs' bitmap definition
    memoryPtr = ((uint8_t *)glyphInfo) + sizeof(GlyphInfo);
    uint8_t *rowp;

    if (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT) {
      uint32_t rowSize = (dim.width + 7) >> 3;
      rowp             = bitmap + (offsets.y * rowSize);

      if (glyphInfo->glyphMetric.dynF == 14) { // is a bitmap?
        uint32_t count = 8;
        uint8_t data;

        for (uint32_t row = 0; row < glyphInfo->bitmapHeight; ++row, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < glyphInfo->bitmapWidth + offsets.x; ++col) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough bitmap data!" << std::endl;
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            if (data & (0x80U >> count)) rowp[col >> 3] |= (0x80U >> (col & 7));
            ++count;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount   = 0;
        nybbleFlipper = 0xf0U;

        bool black = !(glyphInfo->glyphMetric.firstIsBlack == 1);

        for (uint32_t row = 0; row < glyphInfo->bitmapHeight; ++row, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < glyphInfo->bitmapWidth + offsets.x; ++col) {
            if (count == 0) {
              if (!getPackedNumber(count, *glyphInfo)) {
                return false;
              }
              black = !black;
            }
            if (black) rowp[col >> 3] |= (0x80U >> (col & 0x07));
            --count;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount << std::endl;
          while ((row < glyphInfo->bitmapHeight) && (repeatCount-- > 0)) {
            bcopy(rowp, rowp + rowSize, rowSize);
            ++row;
            rowp += rowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    } else {
      uint32_t rowSize = dim.width;
      rowp             = bitmap + (offsets.y * rowSize);

      repeatCount   = 0;
      nybbleFlipper = 0xf0U;

      if (glyphInfo->glyphMetric.dynF == 14) { // is a bitmap?
        uint32_t count = 8;
        uint8_t data;

        for (uint32_t row = 0; row < (glyphInfo->bitmapHeight); ++row, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < (glyphInfo->bitmapWidth + offsets.x); ++col) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough bitmap data!" << std::endl;
                return false;
              }
              // std::cout << std::hex << +data << ' ';
              count = 0;
            }
            rowp[col] = (data & (0x80U >> count)) ? 0xFF : 0;
            ++count;
          }
        }
        // std::cout << std::endl;
      } else {
        uint32_t count = 0;

        repeatCount   = 0;
        nybbleFlipper = 0xf0U;

        bool black = !(glyphInfo->glyphMetric.firstIsBlack == 1);

        for (uint32_t row = 0; row < (glyphInfo->bitmapHeight); ++row, rowp += rowSize) {
          for (uint32_t col = offsets.x; col < (glyphInfo->bitmapWidth + offsets.x); ++col) {
            if (count == 0) {
              if (!getPackedNumber(count, *glyphInfo)) {
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
            --count;
          }

          // if (repeatCount != 0) std::cout << "Repeat count: " << repeatCount << std::endl;
          while ((row < dim.height) && (repeatCount-- > 0)) {
            bcopy(rowp, rowp + rowSize, rowSize);
            ++row;
            rowp += rowSize;
          }

          repeatCount = 0;
        }
        // std::cout << std::endl;
      }
    }
    return true;
  }

  auto loadPreamble() -> bool {
    preamble = (Preamble *)memory;
    if (strncmp("IBMF", preamble->marker, 4) != 0) return false;
    if (preamble->bits.version != IBMF_VERSION) return false;
    sizes       = (uint8_t *)&memory[6 + (preamble->sizeCount * 4)];
    currentFont = nullptr;

    return true;
  }

  auto loadData() -> bool {
    memset(glyphInfoTable, 0, sizeof(glyphInfoTable));

    memoryPtr = currentFont;

    header = (Header *)currentFont;

    memoryPtr += sizeof(Header);
    for (int i = 0; i < header->glyphCount; ++i) {
      glyphInfo                           = (GlyphInfo *)memoryPtr;
      glyphInfoTable[glyphInfo->charCode] = (GlyphInfo *)memoryPtr;
      memoryPtr += sizeof(GlyphInfo) + glyphInfo->packetLength;
      if (memoryPtr > memoryEnd) return false;
    }

    ligKernPgm = (LigKernStep *)memoryPtr;
    memoryPtr += sizeof(LigKernStep) * header->ligKernPgmCount;
    if (memoryPtr > memoryEnd) return false;

    kerns = (FIX16 *)memoryPtr;

    memoryPtr += sizeof(FIX16) * header->kernCount;
    if (memoryPtr > memoryEnd) return false;

    return true;
  }

public:
  IBMFFont(uint8_t *memoryFont, uint32_t size, Font &font)
      : memory(memoryFont), memoryLength(size), font(font) {

    memoryEnd   = memory + memoryLength;
    initialized = loadPreamble();
    if (!initialized) {
      std::cerr << "Font data not recognized!" << std::endl;
    }
  }

  IBMFFont(const std::string filename, Font &font) : font(font) {

    struct stat fileStat;

    initialized = false;

    if (stat(filename.c_str(), &fileStat) != -1) {
      memoryLength = fileStat.st_size;
      ownedMemory  = std::make_unique<uint8_t[]>(memoryLength);
      memory       = ownedMemory.get();
      memoryEnd    = (memory == nullptr) ? nullptr : memory + memoryLength;

      if (memory != nullptr) {
        std::ifstream file(filename, std::ios::binary);
        if (file.is_open()) {
          file.read(reinterpret_cast<char *>(memory), static_cast<std::streamsize>(memoryLength));
        }
        if (file.good() || (file.eof() && (static_cast<uint32_t>(file.gcount()) == memoryLength))) {
          initialized = loadPreamble();
        }
        if (!initialized) {
          std::cerr << "Font data not recognized!" << std::endl;
        }
      }
    } else {
      std::cerr << "Unable to find font file " << filename << "!" << std::endl;
    }
  }

  ~IBMFFont() = default;

  [[nodiscard]] inline auto getFontSize() -> uint8_t { return header->pointSize; }
  [[nodiscard]] inline auto getLineHeight() -> uint16_t { return header->lineHeight; }
  [[nodiscard]] inline auto getCharsHeight() -> uint16_t { return header->emSize >> 6; }
  [[nodiscard]] inline auto getDescenderHeight() -> int16_t {
    return -static_cast<int16_t>(header->descenderHeight);
  }
  [[nodiscard]] inline auto getLigKern(uint8_t idx) -> LigKernStep * { return &ligKernPgm[idx]; }
  [[nodiscard]] inline auto getKern(uint8_t i) -> FIX16 { return kerns[i]; }
  [[nodiscard]] inline auto getGlyphInfo(uint8_t glyphCode) -> GlyphInfo * {
    return glyphInfoTable[glyphCode];
  }
  [[nodiscard]] inline auto getCharSet() -> uint8_t { return preamble->bits.charSet; }

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
  auto translate(uint32_t charcode) -> uint32_t {
    uint32_t glyphCode;

    if (preamble->bits.charSet == 0) {
      if ((charcode > 0x20) && (charcode < 0x7F)) {
        glyphCode = 0xFF00 | charcode;
      } else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
        glyphCode = set2TranslationLatin1[charcode - 0xA1];
      } else if ((charcode >= 0x100) && (charcode <= 0x1FF)) {
        glyphCode = set2TranslationLatinA[charcode - 0x100];
      } else {
        switch (charcode) {
        case 0x2013:
          glyphCode = 0xFF15;
          break; // endash
        case 0x2014:
          glyphCode = 0xFF16;
          break;     // emdash
        case 0x2018: // quote left
        case 0x02BB: // reverse apostrophe
          glyphCode = 0xFF60;
          break;
        case 0x2019: // quote right
        case 0x02BC: // apostrophe
          glyphCode = 0xFF27;
          break;
        case 0x201C:
          glyphCode = 0xFF10;
          break; // quoted left "
        case 0x201D:
          glyphCode = 0xFF11;
          break; // quoted right
        case 0x02C6:
          glyphCode = 0xFF5E;
          break; // circumflex
        case 0x02DA:
          glyphCode = 0xFF06;
          break; // ring
        case 0x02DC:
          glyphCode = 0xFF7E;
          break; // tilde ~
        case 0x201A:
          glyphCode = 0xFF0D;
          break; // comma like ,
        case 0x2032:
          glyphCode = 0xFF27;
          break; // minute '
        case 0x2033:
          glyphCode = 0xFF22;
          break; // second "
        case 0x2044:
          glyphCode = 0xFF2F;
          break; // fraction /
        case 0x20AC:
          glyphCode = 0xFFAD;
          break; // euro
        default:
          glyphCode = 0xFFFE;
        }
      }
    } else {
      glyphCode = 0xFFFE;
    }

    return glyphCode;
  }

  auto getGlyph(uint32_t glyphCode, Glyph &appGlyph, GlyphInfo **glyphData, bool loadBitmap)
      -> bool {
    // uint32_t glyphCode = translate(charCode);

    glyph.clear();

    uint8_t accent        = (glyphCode & 0x0000FF00) >> 8;
    GlyphInfo *accentInfo = (accent != 0xFF) ? glyphInfoTable[accent] : nullptr;

    if (((glyphCode & 0xFF) == 0xFF) || (((glyphCode & 0xFF) < header->glyphCount) &&
                                         (glyphInfoTable[glyphCode & 0xFF] == nullptr))) {
      std::cerr << "No entry for char code 0x" << std::hex << glyphCode << std::endl;
      glyphCode = 0xFFFE;
    }

    if (glyphCode == 0xFFFE) {
      glyph.advance    = header->spaceSize;
      glyph.lineHeight = header->lineHeight;

      appGlyph   = glyph;
      *glyphData = nullptr;

      return true;
    }

    glyphInfo = glyphInfoTable[glyphCode & 0xFF];

    if (glyphInfo == nullptr) return false;

    Dim dim     = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
    Pos offsets = Pos(0, 0);

    uint8_t addedLeft = 0;

    if (accentInfo != nullptr) {

      if (glyphCode == 0x276E) { // Apostrophe n
        // offsets.x = 0; // already set
        addedLeft =
            accentInfo->bitmapWidth + 1 - (((header->xHeight >> 6) * header->slantCorrection) >> 6);
        dim.width = addedLeft + glyphInfo->bitmapWidth;
      } else {
        // Horizontal adjustment
        if (glyphCode == 0x0C41) { // Ą
          offsets.x = glyphInfo->bitmapWidth - accentInfo->bitmapWidth;
        } else if ((glyphCode == 0x0C61) || (glyphCode == 0x0C45)) { // ą or Ę
          offsets.x = glyphInfo->bitmapWidth - accentInfo->bitmapWidth -
                      ((((int32_t)glyphInfo->bitmapHeight) * header->slantCorrection) >> 6);
        } else {
          offsets.x = ((glyphInfo->bitmapWidth > accentInfo->bitmapWidth)
                           ? ((glyphInfo->bitmapWidth - accentInfo->bitmapWidth) >> 1)
                           : 0) +
                      ((accentInfo->verticalOffset < 5)
                           ? -(((header->xHeight >> 6) * header->slantCorrection) >> 6)
                           : ((((int32_t)glyphInfo->bitmapHeight) * header->slantCorrection) >> 6))
              /*- (accentInfo->horizontalOffset - glyphInfo->horizontalOffset)*/;
        }
        if ((offsets.x == 0) && (glyphInfo->bitmapWidth < accentInfo->bitmapWidth)) {
          addedLeft = (accentInfo->bitmapWidth - glyphInfo->bitmapWidth) >> 1;
          dim.width = accentInfo->bitmapWidth;
        }
        if (dim.width < (offsets.x + accentInfo->bitmapWidth)) {
          dim.width = offsets.x + accentInfo->bitmapWidth;
        }
      }

      // Vertical adjustment
      if (accentInfo->verticalOffset >= (header->xHeight >> 6)) {
        // Accents that are on top of a main glyph
        dim.height += (accentInfo->verticalOffset - (header->xHeight >> 6));
      } else if (accentInfo->verticalOffset < 5) {
        // Accents below the main glyph (cedilla)
        int16_t addedHeight = (glyphInfo->bitmapHeight - glyphInfo->verticalOffset) -
                              ((-accentInfo->verticalOffset) + accentInfo->bitmapHeight);
        if (addedHeight < 0) dim.height += -addedHeight;
        offsets.y = glyphInfo->verticalOffset - accentInfo->verticalOffset;
      }
      // else if ((glyphCode == 0x1648) || (glyphCode == 0x1568)) { // Ħ or ħ
      //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 2;
      //   offsets.x = ((glyphInfo->bitmapHeight - offsets.y) * header->slantCorrection) >> 6;
      // }
      // else if ((glyphCode == 0x1554) || (glyphCode == 0x1574)) { // Ŧ or ŧ
      //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 1;
      //   offsets.x = (offsets.y * header->slantCorrection) >> 6;
      // }
    }

    uint16_t size = (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT)
                        ? dim.height * ((dim.width + 7) >> 3)
                        : dim.height * dim.width;

    if (loadBitmap) {
      glyph.buffer = font.bytePoolAlloc(size);
      memset(glyph.buffer, 0, size);
    } else {
      glyph.buffer = nullptr;
    }

    if (accentInfo != nullptr) {
      if (loadBitmap) retrieveBitmap(accentInfo, glyph.buffer, dim, offsets);

      offsets.y = (accentInfo->verticalOffset >= (header->xHeight >> 6))
                      ? (accentInfo->verticalOffset - (header->xHeight >> 6))
                      : 0;
      offsets.x = addedLeft;
    }

    if (loadBitmap) retrieveBitmap(glyphInfo, glyph.buffer, dim, offsets);

    glyph.dim     = dim;
    glyph.xoff    = -(glyphInfo->horizontalOffset + offsets.x);
    glyph.yoff    = -(glyphInfo->verticalOffset + offsets.y);
    glyph.advance = glyphInfo->advance >> 6;
    glyph.pitch   = (screen.getPixelResolution() == Screen::PixelResolution::ONE_BIT)
                        ? (dim.width + 7) >> 3
                        : dim.width;
    glyph.ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex;
    glyph.lineHeight              = header->lineHeight;

    appGlyph   = glyph;
    *glyphData = glyphInfo;

    return true;
  }

  auto setFontSize(uint8_t size) -> bool {
    if (!initialized) return false;
    uint8_t i = 0;
    while ((i < preamble->sizeCount) && (sizes[i] <= size)) ++i;
    if (i > 0) --i;
    currentFont      = memory + preamble->fontOffsets[i];
    currentPointSize = sizes[i];
    return loadData();
  }

  auto showGlyphInfo(const GlyphInfo &glyph, uint8_t charCode) -> bool {
    std::cout << "Glyph Char Code: " << charCode << std::endl
              << "  Metrics: [" << std::dec << glyph.bitmapWidth << ", " << glyph.bitmapHeight
              << "] " << glyph.packetLength << std::endl
              << "  Position: [" << glyph.horizontalOffset << ", " << glyph.verticalOffset << ']'
              << std::endl;
    return true;
  }

  auto showGlyph(const Glyph &glyph, uint32_t charCode) -> bool {
    std::cout << "Glyph Char Code: " << std::hex << charCode << std::dec << std::endl
              << "  Metrics: [" << std::dec << glyph.dim.width << ", " << glyph.dim.height << "] "
              << std::endl
              << "  Position: [" << glyph.xoff << ", " << glyph.yoff << ']' << std::endl
              << "  Bitmap available: " << ((glyph.buffer == nullptr) ? "No" : "Yes") << std::endl;

    if (glyph.buffer == nullptr) return true;

    uint32_t row, col;
    uint32_t rowSize = glyph.pitch;
    const uint8_t *rowPtr;

    std::cout << '+';
    for (col = 0; col < glyph.dim.width; col++) std::cout << '-';
    std::cout << '+' << std::endl;

    for (row = 0, rowPtr = glyph.buffer; row < glyph.dim.height; ++row, rowPtr += rowSize) {
      std::cout << '|';
      for (col = 0; col < glyph.dim.width; ++col) {
        std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
      }
      std::cout << '|';
      std::cout << std::endl;
    }

    std::cout << '+';
    for (col = 0; col < glyph.dim.width; ++col) std::cout << '-';
    std::cout << '+' << std::endl << std::endl;

    return true;
  }

  [[nodiscard]] inline auto isInitialized() -> bool { return initialized; }
};