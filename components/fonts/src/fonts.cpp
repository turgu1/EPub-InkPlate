// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FONTS__ 1
#include "fonts.hpp"

#include "config.hpp"
#include "font_factory.hpp"

#include "fonts.hpp"
#include <algorithm>
#include <sys/stat.h>

#include <freetype/ftlogging.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftsystem.h>

using namespace pugi;

void myFtLog(const char *ft_component, const char *fmt, va_list args) {

  const char *TAG = "FreeType";
  LOG_I("FreeType: %s: ", ft_component);
  vprintf(fmt, args);
}

static auto myFtAlloc(FT_Memory memory, long size) -> void * {
  #if EPUB_INKPLATE_BUILD
    return heap_caps_malloc(static_cast<size_t>(size), MALLOC_CAP_SPIRAM);
  #else
    return malloc(static_cast<size_t>(size));
  #endif
}

static auto myFtRealloc(FT_Memory memory, long currSize, long newSize, void *block) -> void * {
  #if EPUB_INKPLATE_BUILD
    return heap_caps_realloc(block, static_cast<size_t>(newSize), MALLOC_CAP_SPIRAM);
  #else
    return realloc(block, static_cast<size_t>(newSize));
  #endif
}

static void myFtFree(FT_Memory memory, void *block) {
  #if EPUB_INKPLATE_BUILD
    heap_caps_free(block);
  #else
    free(block);
  #endif
}

static FT_MemoryRec_ ftMemory = {
    nullptr,
    myFtAlloc,
    myFtFree,
    myFtRealloc,
};

Fonts::Fonts(bool thisIsAppFonts) : thisIsAppFonts(thisIsAppFonts) {
  #if USE_EPUB_FONTS
    fontCache.reserve(20);
  #else
    fontCache.reserve(4);
  #endif

  if (library == nullptr) {
    FT_Error error = FT_New_Library(&ftMemory, &library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
      std::abort();
    }
    FT_Add_Default_Modules(library);
    FT_Set_Log_Handler(myFtLog);
  }
}

auto Fonts::setup() -> bool {
  LOG_D("Fonts initialization");

  clearEverything();

  charPool = CharPool::Make();

  config.get(Config::Ident::FONTS_DB, &fontsDB);

  if (fontsDB) {
    if (thisIsAppFonts) {
      // The use of user fonts are only related to books, so for
      // the rest of the app, only the first 3 fonts (the icon
      // and system fonts) are loaded.
      LOG_D("Loading Application fonts...");

      for (int i = 0; i < 3; ++i) {
        const auto descr = fontsDB->getFontFaceDescriptor(i);
        if (descr) {
          LOG_D("Loading font %s...", ((*descr)->name.c_str()));
          if (!add(*descr)) {
            LOG_E("Unable to load font %s", ((*descr)->name.c_str()));
          }
        }
      }
    } else {
      LOG_D("Loading fonts for books...");
      for (int i = 0; i < 7; ++i) {
        const auto descr = fontsDB->getFontFaceDescriptor(i);
        if (descr) {
          LOG_D("Loading font %s...", ((*descr)->name.c_str()));
          if (!add(*descr)) {
            LOG_E("Unable to load font %s", ((*descr)->name.c_str()));
          }
        }
      }
    }
  } else {
    LOG_E("No font descriptor found in config!");
    return false;
  }

  return true;
}

Fonts::~Fonts() {
  fontCache.clear();
  if (library) {
    FT_Done_Library(library);
    library = nullptr;
  }
}

auto Fonts::clear(bool all) -> void {
  #if USE_EPUB_FONTS
    if (!thisIsAppFonts) {
      for (int i = 0; i < (all ? 3 : 7); ++i) {
        fontCache.at(i)->font->clearCache();
      }
      fontCache.resize(all ? 3 : 7);
      bookFontFaceDescriptors.clear();
      fontCache.reserve(20);
    }
  #endif
}

auto Fonts::clearEverything() -> void {
  {
    // std::scoped_lock guard(mutex);

    charPool.reset();

    fontCache.clear();
    fontCache.resize(0);
    fontCache.reserve(20);
    bookFontFaceDescriptors.clear();
  }
}

auto Fonts::clearGlyphCaches() -> void {
  // std::scoped_lock guard(mutex);

  for (auto &entry : fontCache) {
    entry->font->clearCache();
  }
}

auto Fonts::setGlyphLoadMode(Font::GlyphLoadMode mode) -> void {
  defaultGlyphLoadMode_ = mode;
  for (auto &entry : fontCache) {
    entry->font->setGlyphLoadMode(mode);
  }
}

auto Fonts::getIndex(const HimemString &name, FaceStyle style) -> int16_t {
  int16_t idx = 0;

  {
    // std::scoped_lock guard(mutex);

    for (auto &entry : fontCache) {
      if ((entry->name.compare(name) == 0) && (entry->style == style)) return idx;
      ++idx;
    }
  }
  return -1;
}

auto Fonts::replace(int16_t index, const FontFaceDescriptorPtr &descr) -> bool {

  auto f = FontEntry::Make();
  if ((f->font = FontFactory::create(descr, library))) {
    if (f->font->isReady()) {
      f->name  = descr->name;
      f->style = descr->style;
      f->font->setFontsCacheIndex(index);
      f->font->setGlyphLoadMode(defaultGlyphLoadMode_);
      fontCache.at(index) = f;

      LOG_D("Font %s (%s) replacement at index %d and style %d.", f->name.c_str(),
            descr->filename.c_str(), f->font->getFontsCacheIndex(), (int)f->style);
      return true;
    }
  } else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.outOfMemory("font allocation");
  }

  return false;
}

void Fonts::adjustDefaultFont(uint8_t fontIndex) {

  if (thisIsAppFonts) {
    if (!fontsDB->isDefaultFont(fontIndex)) {
      fontsDB->adjustDefaultFont(fontIndex);
    }
  } else {
    if (fontCache.at(3)->name.compare(fontsDB->getStandardFontName(fontIndex)) != 0) {
      fontsDB->adjustDefaultFont(fontIndex);

      replace(3, *fontsDB->getFontFaceDescriptor(3));
      replace(4, *fontsDB->getFontFaceDescriptor(4));
      replace(5, *fontsDB->getFontFaceDescriptor(5));
      replace(6, *fontsDB->getFontFaceDescriptor(6));

      LOG_D("Default font is now %s", fontsDB->getStandardFontName(fontIndex).c_str());
    }
  }
}

auto Fonts::add(const FontFaceDescriptorPtr &descr) -> bool {
  // std::scoped_lock guard(mutex);

  // If the font is already loaded, return promptly
  for (auto &font : fontCache) {
    if ((descr->name.compare(font->name) == 0) && (descr->style == font->style)) return true;
  }

  auto f = FontEntry::Make();

  if ((f->font = FontFactory::create(descr, library))) {
    if (f->font->isReady()) {
      f->name  = descr->name;
      f->style = descr->style;
      f->font->setFontsCacheIndex(fontCache.size());
      f->font->setGlyphLoadMode(defaultGlyphLoadMode_);
      fontCache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.", f->name.c_str(),
            f->font->getFontsCacheIndex(), (int)f->style);
      return true;
    } else {
      f->font.reset();
    }
  } else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.outOfMemory("font allocation");
  }

  return false;
}

auto Fonts::add(const HimemString &fontFamily, FaceStyle style, FileContentPtr buffer, size_t size,
                const HimemString &filename) -> bool {

  FontFaceDescriptorPtr descr = FontFaceDescriptor::Make();

  descr->name         = fontFamily;
  descr->style        = style;
  descr->filename     = filename;
  descr->fontData     = std::move(buffer);
  descr->fontDataSize = size;

  bookFontFaceDescriptors.push_back(std::move(descr));

  return add(bookFontFaceDescriptors.back());
}

auto Fonts::adjustFontStyle(FaceStyle style, FaceStyle fontStyle, FaceStyle fontWeight) const
    -> FaceStyle {
  if (fontStyle == FaceStyle::ITALIC) {
    // NORMAL -> ITALIC
    // BOLD -> BOLD_ITALIC
    // ITALIC (no change)
    // BOLD_ITALIC (no change)
    if (style == FaceStyle::NORMAL)
      style = FaceStyle::ITALIC;
    else if (style == FaceStyle::BOLD)
      style = FaceStyle::BOLD_ITALIC;
  } else if (fontStyle == FaceStyle::NORMAL) {
    // NORMAL
    // BOLD
    // ITALIC -> NORMAL
    // BOLD_ITALIC -> BOLD
    if (style == FaceStyle::BOLD_ITALIC)
      style = FaceStyle::BOLD;
    else if (style == FaceStyle::ITALIC)
      style = FaceStyle::NORMAL;
  }
  if (fontWeight == FaceStyle::BOLD) {
    // NORMAL -> BOLD
    // BOLD
    // ITALIC -> BOLD_ITALIC
    // BOLD_ITALIC
    if (style == FaceStyle::ITALIC)
      style = FaceStyle::BOLD_ITALIC;
    else if (style == FaceStyle::NORMAL)
      style = FaceStyle::BOLD;
  } else if (fontWeight == FaceStyle::NORMAL) {
    // NORMAL
    // BOLD -> NORMAL
    // ITALIC
    // BOLD_ITALIC -> ITALIC
    if (style == FaceStyle::BOLD)
      style = FaceStyle::NORMAL;
    else if (style == FaceStyle::BOLD_ITALIC)
      style = FaceStyle::ITALIC;
  }

  return style;
}