// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FONTS__ 1
#include "models/fonts.hpp"
#include "models/config.hpp"
#include "alloc.hpp"

#include <algorithm>

static const char * font_names[8] = {
  "CrimsonPro",
  "Caladea",
  "Asap",
  "AsapCondensed",
  "DejaVuSerif",
  "DejaVuSerifCondensed",
  "DejaVuSans",
  "DejaVuSansCondensed"
};

Fonts::Fonts()
{
  #if USE_EPUB_FONTS
    font_cache.reserve(20);
  #else
    font_cache.reserve(4);
  #endif
}

bool Fonts::setup()
{
  FontEntry font_entry;

  LOG_D("Fonts initialization");

  clear(true);

  int8_t font_index;
  config.get(Config::DEFAULT_FONT, &font_index);
  if ((font_index < 0) || (font_index > 7)) font_index = 0;

  std::string def         = "Default";
  std::string draw        = "Drawings";

  std::string drawings    = FONTS_FOLDER "/drawings.otf";
  
  std::string normal      = std::string(FONTS_FOLDER "/").append(font_names[font_index]).append("-Regular.otf"   );
  std::string bold        = std::string(FONTS_FOLDER "/").append(font_names[font_index]).append("-Bold.otf"      );
  std::string italic      = std::string(FONTS_FOLDER "/").append(font_names[font_index]).append("-Italic.otf"    );
  std::string bold_italic = std::string(FONTS_FOLDER "/").append(font_names[font_index]).append("-BoldItalic.otf");

  if (!add(draw, NORMAL,      drawings   )) return false;
  if (!add(def,  NORMAL,      normal     )) return false;
  if (!add(def,  BOLD,        bold       )) return false;
  if (!add(def,  ITALIC,      italic     )) return false;
  if (!add(def,  BOLD_ITALIC, bold_italic)) return false;   

  return true;
}

Fonts::~Fonts()
{
  for (auto & entry : font_cache) {
    delete entry.font;
  }
  font_cache.clear();
}

void
Fonts::clear(bool all)
{
  // LOG_D("Fonts Clear!");
  // Keep the first 5 fonts as they are reused. Caches will be cleared.
  #if USE_EPUB_FONTS
    int i = 0;
    for (auto & entry : font_cache) {
      if (all || (i >= 5)) delete entry.font;
      else entry.font->clear_cache();
      i++;
    }
    font_cache.resize(all ? 0 : 4);
    font_cache.reserve(20);
  #endif
}

int16_t
Fonts::get_index(const std::string & name, FaceStyle style)
{
  int16_t idx = 0;

  for (auto & entry : font_cache) {
    if ((entry.name.compare(name) == 0) && 
        (entry.style == style)) return idx;
    idx++;
  }

  return -1;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle style,
           const std::string & filename)
{
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;
  if ((f.font = new TTF(filename))) {
    if (f.font->ready()) {
      f.name        = name;
      f.font->index = font_cache.size();
      f.style       = style;
      font_cache.push_back(f);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
  }

  return false;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle           style,
           unsigned char *     buffer,
           int32_t             size)
{
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;

  if ((f.font = new TTF(buffer, size))) {
    if (f.font->ready()) {
      f.name        = name;
      f.font->index = font_cache.size();
      f.style       = style;
      font_cache.push_back(f);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
  }

  return false;
}

Fonts::FaceStyle
Fonts::adjust_font_style(FaceStyle style, FaceStyle font_style, FaceStyle font_weight)
{
  if (font_style == ITALIC) { 
    // NORMAL -> ITALIC
    // BOLD -> BOLD_ITALIC
    // ITALIC (no change)
    // BOLD_ITALIC (no change)
    if      (style == NORMAL) style = ITALIC;
    else if (style == BOLD  ) style = BOLD_ITALIC;
  }
  else if (font_style == NORMAL) { 
    // NORMAL
    // BOLD
    // ITALIC -> NORMAL
    // BOLD_ITALIC -> BOLD
    if      (style == BOLD_ITALIC) style = BOLD;
    else if (style == ITALIC     ) style = NORMAL;
  }
  if (font_weight == BOLD) { 
    // NORMAL -> BOLD
    // BOLD
    // ITALIC -> BOLD_ITALIC
    // BOLD_ITALIC
    if      (style == ITALIC) style = BOLD_ITALIC;
    else if (style == NORMAL) style = BOLD;
  }
  else if (font_weight == NORMAL) { 
    // NORMAL
    // BOLD -> NORMAL
    // ITALIC
    // BOLD_ITALIC -> ITALIC
    if      (style == BOLD       ) style = NORMAL;
    else if (style == BOLD_ITALIC) style = ITALIC;
  }

  return style;
}