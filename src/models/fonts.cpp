// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __FONTS__ 1
#include "models/fonts.hpp"

#include "models/config.hpp"
#include "models/font_factory.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "controllers/book_param_controller.hpp"
#include "controllers/option_controller.hpp"
#include "helpers/unzip.hpp"
#include "alloc.hpp"
#include "pugixml.hpp"

#include <algorithm>
#include <sys/stat.h>

using namespace pugi;

static constexpr const char * font_fnames[8] = {
  "CrimsonPro",
  "Caladea",
  "Asap",
  "AsapCondensed",
  "DejaVuSerif",
  "DejaVuSerifCondensed",
  "DejaVuSans",
  "DejaVuSansCondensed"
};

static constexpr const char * font_labels[8] = {
  "CRIMSON S",
  "CALADEA S",
  "ASAP",
  "ASAP COND",
  "DEJAVUE S",
  "DEJAVUE COND S",
  "DEJAVU",
  "DEJAVU COND"
};

Fonts::Fonts()
{
  #if USE_EPUB_FONTS
    font_cache.reserve(20);
  #else
    font_cache.reserve(4);
  #endif
}

char *
Fonts::get_file(const char * filename, uint32_t size)
{
  FILE * f = fopen(filename, "r");
  char * buff = nullptr;

  if (f) {
    buff = (char *) allocate(size);
    if (buff && (fread(buff, size, 1, f) != 1)) {
      free(buff);
      buff = nullptr;
    } 
    fclose(f);
  }

  return buff;
}

static uint32_t font_size;
inline bool check_res(xml_parse_result res) { return res.status == status_ok; }

static bool check_file(const std::string & filename) 
{
  constexpr const char * TAG = "Check File";
  struct stat file_stat;
  if (!filename.empty()) {
    std::string full_name = std::string(FONTS_FOLDER "/").append(filename); 
    if (stat(full_name.c_str(), &file_stat) != -1) {
      font_size += file_stat.st_size;
      if (font_size > (1024 * 300)) {
        LOG_E("Font size too big for %s", filename.c_str());
      }
      else return true;
    }
    else {
      LOG_E("Font file can't be found: %s", full_name.c_str());
    }
  }
  else {
    LOG_E("Null string...");
  }
  return false;
}

std::string & Fonts::filter_filename(std::string & fname)
{
  std::size_t pos = fname.find("%DPI%");
  if (pos != std::string::npos) {
    char dpi[5];
    int_to_str(Screen::RESOLUTION, dpi, 5);
    fname.replace(pos, 5, dpi);
  }
  return fname;
}

bool Fonts::setup()
{
  FontEntry   font_entry;
  struct stat file_stat;

  LOG_D("Fonts initialization");

  clear_everything();

  constexpr static const char * xml_fonts_descr = MAIN_FOLDER "/fonts_list.xml";

  xml_document     fd;
  char *           fd_data = nullptr;

  if ((stat(xml_fonts_descr, &file_stat) != -1) &&
      ((fd_data = get_file(xml_fonts_descr, file_stat.st_size)) != nullptr) &&
      (check_res(fd.load_buffer_inplace(fd_data, file_stat.st_size)))) {

    LOG_I("Reading fonts definition from fonts_list.xml...");

    // System fonts
    auto sys_group = fd.child("fonts")
                       .find_child_by_attribute("group", "name", "SYSTEM");

    if (sys_group) {
      auto fnt = sys_group.find_child_by_attribute("font", "name", "ICON").child("normal");
      if (!(fnt && add("Icon", 
                       FaceStyle::ITALIC, 
                       filter_filename(std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ICON font.");
        return false;
      }

      fnt = sys_group.find_child_by_attribute("font", "name", "TEXT").child("normal");
      if (!(fnt && add("System", 
                      FaceStyle::NORMAL, 
                      filter_filename(std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM NORMAL font.");
        return false;
      }

      fnt = sys_group.find_child_by_attribute("font", "name", "TEXT").child("italic");
      if (!(fnt && add("System", 
                      FaceStyle::ITALIC, 
                      filter_filename(std::string(FONTS_FOLDER "/").append(fnt.attribute("filename").value()))))) {
        LOG_E("Unable to find SYSTEM ITALIC font.");
        return false;
      }
    }
    else {
      LOG_E("SYSTEM group not found in fonts_list.xml.");
      return false;
    }

    font_count = 0;
    auto user_group = fd.child("fonts").find_child_by_attribute("group", "name", "USER");
    for (auto fnt : user_group.children("font")) {
      if (font_count >= 8) break;
      std::string str = fnt.attribute("name").value();
      font_size = 0;
      if (!str.empty()) {
        LOG_D("%s...", str.c_str());
        font_names[font_count] = char_pool.set(str);
        str = fnt.child("normal").attribute("filename").value();
        if (check_file(str = filter_filename(str))) {
          regular_fname[font_count] = char_pool.set(str);
          str = fnt.child("bold").attribute("filename").value();
          if (check_file(str = filter_filename(str))) {
            bold_fname[font_count] = char_pool.set(str);
            str = fnt.child("italic").attribute("filename").value();
            if (check_file(str = filter_filename(str))) {
              italic_fname[font_count] = char_pool.set(str);
              str = fnt.child("bold-italic").attribute("filename").value();
              if (check_file(str = filter_filename(str))) {
                bold_italic_fname[font_count] = char_pool.set(str);
                LOG_I("Font %s OK", font_names[font_count]);
                font_count++;
              }
            }
          }
        }
      }
    }

    LOG_D("Got %d fonts from xml file.", font_count);

    if (font_count == 0) {
      LOG_E("No USER font detected!");
      return false;
    }

    FormChoiceField::adjust_font_choices(font_names, font_count);

    book_param_controller.set_font_count(font_count);
        option_controller.set_font_count(font_count);

    int8_t font_index;
    config.get(Config::Ident::DEFAULT_FONT, &font_index);
    if ((font_index < 0) || (font_index >= font_count)) font_index = 0;

    std::string normal      = std::string(FONTS_FOLDER "/").append(    regular_fname[font_index]);
    std::string bold        = std::string(FONTS_FOLDER "/").append(       bold_fname[font_index]);
    std::string italic      = std::string(FONTS_FOLDER "/").append(     italic_fname[font_index]);
    std::string bold_italic = std::string(FONTS_FOLDER "/").append(bold_italic_fname[font_index]);

    if (!add(font_names[font_index], FaceStyle::NORMAL,      normal     )) return false;
    if (!add(font_names[font_index], FaceStyle::BOLD,        bold       )) return false;
    if (!add(font_names[font_index], FaceStyle::ITALIC,      italic     )) return false;
    if (!add(font_names[font_index], FaceStyle::BOLD_ITALIC, bold_italic)) return false;   
    
    fd.reset();
    free(fd_data);
    return true;
  }

  LOG_E("Unable to read fonts definition file fonts_list.xml.");
  return false;
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
  std::scoped_lock guard(mutex);
  
  // LOG_D("Fonts Clear!");
  // Keep the first 7 fonts as they are reused. Caches will be cleared.
  #if USE_EPUB_FONTS
    int i = 0;
    for (auto & entry : font_cache) {
      if ((all && (i >= 3)) || (i >= 7)) delete entry.font;
      else entry.font->clear_cache();
      i++;
    }
    font_cache.resize(all ? 3 : 7);
    font_cache.reserve(20);
  #endif
}

void
Fonts::clear_everything()
{
  std::scoped_lock guard(mutex);
  
  // LOG_D("Fonts Clear!");
  // Keep the first 7 fonts as they are reused. Caches will be cleared.
  for (auto & entry : font_cache) {
    delete entry.font;
  }
  font_cache.resize(0);
  font_cache.reserve(20);
}

void
Fonts::adjust_default_font(uint8_t font_index)
{
  if (font_cache.at(3).name.compare(font_names[font_index]) != 0) {
    std::string normal      = std::string(FONTS_FOLDER "/").append(    regular_fname[font_index]);
    std::string bold        = std::string(FONTS_FOLDER "/").append(       bold_fname[font_index]);
    std::string italic      = std::string(FONTS_FOLDER "/").append(     italic_fname[font_index]);
    std::string bold_italic = std::string(FONTS_FOLDER "/").append(bold_italic_fname[font_index]);

    if (!replace(3, font_names[font_index], FaceStyle::NORMAL,      normal     )) return;
    if (!replace(4, font_names[font_index], FaceStyle::BOLD,        bold       )) return;
    if (!replace(5, font_names[font_index], FaceStyle::ITALIC,      italic     )) return;
    if (!replace(6, font_names[font_index], FaceStyle::BOLD_ITALIC, bold_italic)) return;

    LOG_D("Default font is now %s", font_names[font_index]);
  }
}

void
Fonts::clear_glyph_caches()
{
  for (auto & entry : font_cache) {
    entry.font->clear_cache();
  }
}

int16_t
Fonts::get_index(const std::string & name, FaceStyle style)
{
  int16_t idx = 0;

  { std::scoped_lock guard(mutex);

    for (auto & entry : font_cache) {
      if ((entry.name.compare(name) == 0) && 
          (entry.style == style)) return idx;
      idx++;
    }
  }
  return -1;
}

bool 
Fonts::replace(int16_t             index,
               const std::string & name, 
               FaceStyle           style,
               const std::string & filename)
{
  std::scoped_lock guard(mutex);
  
  FontEntry f;
  if ((f.font = FontFactory::create(filename))) {
    if (f.font->is_ready()) {
      f.name  = name;
      f.style = style;
      f.font->set_fonts_cache_index(index);
      delete font_cache.at(index).font;
      font_cache.at(index) = f;

      LOG_D("Font %s (%s) replacement at index %d and style %d.",
        f.name.c_str(), 
        filename.c_str(),
        f.font->get_fonts_cache_index(),
        (int)f.style);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.out_of_memory("font allocation");
  }

  return false;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle           style,
           const std::string & filename)
{
  std::scoped_lock guard(mutex);
  
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;
  if ((f.font = FontFactory::create(filename))) {
    if (f.font->is_ready()) {
      f.name  = name;
      f.style = style;
      f.font->set_fonts_cache_index(font_cache.size());
      font_cache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.",
        f.name.c_str(), 
        f.font->get_fonts_cache_index(),
        (int)f.style);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.out_of_memory("font allocation");
  }

  return false;
}

bool 
Fonts::add(const std::string & name, 
           FaceStyle           style,
           unsigned char *     buffer,
           int32_t             size,
           const std::string & filename)
{
  std::scoped_lock guard(mutex);
  
  // If the font is already loaded, return promptly
  for (auto & font : font_cache) {
    if ((name.compare(font.name) == 0) && 
        (font.style == style)) return true;
  }

  FontEntry f;

  if ((f.font = FontFactory::create(filename, buffer, size))) {
    if (f.font->is_ready()) {
      f.name  = name;
      f.style = style;
      f.font->set_fonts_cache_index(font_cache.size());
      font_cache.push_back(f);

      LOG_D("Font %s added to cache at index %d and style %d.",
        f.name.c_str(), 
        f.font->get_fonts_cache_index(),
        (int)f.style);
      return true;
    }
    else {
      delete f.font;
    }
  }
  else {
    LOG_E("Unable to allocate memory.");
    // msg_viewer.out_of_memory("font allocation");
  }

  return false;
}

Fonts::FaceStyle
Fonts::adjust_font_style(FaceStyle style, FaceStyle font_style, FaceStyle font_weight) const
{
  if (font_style == FaceStyle::ITALIC) { 
    // NORMAL -> ITALIC
    // BOLD -> BOLD_ITALIC
    // ITALIC (no change)
    // BOLD_ITALIC (no change)
    if      (style == FaceStyle::NORMAL) style = FaceStyle::ITALIC;
    else if (style == FaceStyle::BOLD  ) style = FaceStyle::BOLD_ITALIC;
  }
  else if (font_style == FaceStyle::NORMAL) { 
    // NORMAL
    // BOLD
    // ITALIC -> NORMAL
    // BOLD_ITALIC -> BOLD
    if      (style == FaceStyle::BOLD_ITALIC) style = FaceStyle::BOLD;
    else if (style == FaceStyle::ITALIC     ) style = FaceStyle::NORMAL;
  }
  if (font_weight == FaceStyle::BOLD) { 
    // NORMAL -> BOLD
    // BOLD
    // ITALIC -> BOLD_ITALIC
    // BOLD_ITALIC
    if      (style == FaceStyle::ITALIC) style = FaceStyle::BOLD_ITALIC;
    else if (style == FaceStyle::NORMAL) style = FaceStyle::BOLD;
  }
  else if (font_weight == FaceStyle::NORMAL) { 
    // NORMAL
    // BOLD -> NORMAL
    // ITALIC
    // BOLD_ITALIC -> ITALIC
    if      (style == FaceStyle::BOLD       ) style = FaceStyle::NORMAL;
    else if (style == FaceStyle::BOLD_ITALIC) style = FaceStyle::ITALIC;
  }

  return style;
}