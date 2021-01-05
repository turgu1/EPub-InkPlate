// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __EPUB__ 1
#include "models/epub.hpp"

#include "models/fonts.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/page_locs.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/book_viewer.hpp"
#include "helpers/unzip.hpp"
#include "logging.hpp"

#include "stb_image.h"
#include "image_info.hpp"

#if EPUB_INKPLATE_BUILD
  #include "esp_heap_caps.h"
#endif

#include <cstring>
#include <string>

using namespace pugi;

EPub::EPub()
{
  opf_data          = nullptr;
  current_item_data = nullptr;
  file_is_open      = false;
  opf_base_path.clear();
  current_itemref   = xml_node(NULL);
  current_filename.clear();
}

EPub::~EPub()
{
  close_file();
}

void
extract_path(const char * fname, std::string & path)
{
  path.clear();
  int i = strlen(fname) - 1;
  while ((i > 0) && (fname[i] != '/')) i--;
  if (i > 0) path.assign(fname, ++i);
}

bool
EPub::check_mimetype()
{
  char   * data;
  uint32_t size;

  // A file named 'mimetype' must be present and must contain the
  // string 'application/epub+zip'

  LOG_D("Check mimetype.");
  if (!(data = unzip.get_file("mimetype", size))) return false;
  if (strncmp(data, "application/epub+zip", 20)) {
    LOG_E("This is not an EPUB ebook format.");
    free(data);
    return false;
  }

  free(data);
  return true;
}

#define ERR(e) { err = e; break; }

bool
EPub::get_opf_filename(std::string & filename)
{
  int          err = 0;
  char       * data;
  uint32_t     size;

  // A file named 'META-INF/container.xml' must be present and point to the OPF file
  LOG_D("Check container.xml.");
  if (!(data = unzip.get_file("META-INF/container.xml", size))) return false;
  
  xml_document    doc;
  xml_node        node;
  xml_attribute   attr;

  xml_parse_result res = doc.load_buffer_inplace(data, size);
  if (res.status != status_ok) {
    LOG_E("xml load error: %d", res.status);
    free(data);
    return false;
  }

  bool completed = false;
  while (!completed) {
    if (!(node = doc.child("container"))) ERR(1);
    if (!((attr = node.attribute("version")) && (strcmp(attr.value(), "1.0") == 0))) ERR(2);
    if (!(node = node.child("rootfiles"))) ERR(3);
    for (node = node.child("rootfile"); 
         node; 
         node = node.next_sibling("rootfile")) {
      //LOG_D(">> %s %d", node->name(), node.attribute("media-type")->value());

      if ((attr = node.attribute("media-type")) && 
          (strcmp(attr.value(), "application/oebps-package+xml") == 0)) break;
    }
    if (!node) ERR(4);
    if (!(attr = node.attribute("full-path"))) ERR(5);
    
    filename.assign(attr.value());

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_opf error: %d", err);
  }

  doc.reset(); 
  free(data);

  return completed;
}

bool 
EPub::get_opf(std::string & filename)
{
  int err = 0;
  uint32_t size;

  xml_node      node;
  xml_attribute attr;

  bool completed = false;
  while (!completed) {
    extract_path(filename.c_str(), opf_base_path);
    LOG_D("opf_base_path: %s", opf_base_path.c_str());

    if (!(opf_data = unzip.get_file(filename.c_str(), size))) ERR(6);

    xml_parse_result res = opf.load_buffer_inplace(opf_data, size);
    if (res.status != status_ok) {
      LOG_E("xml load error: %d", res.status);
      opf.reset();
      free(opf_data);
      opf_data = nullptr;
      return false;
    }

    // Verifie that the OPF is of one of the version understood by this application
    if (!((node = opf.child("package")) && 
          (attr = node.attribute("xmlns")) &&
          (strcmp(attr.value(), "http://www.idpf.org/2007/opf") == 0) &&
          (attr = node.attribute("version")) &&
          ((strcmp(attr.value(), "2.0") == 0) || (strcmp(attr.value(), "3.0") == 0)))) {
      LOG_E("This book is not compatible with this software.");
      break;
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_opf error: %d", err);
    opf.reset();
    free(opf_data);
  }
 
  LOG_D("get_opf() completed.");

  return completed;
}

unsigned char 
bin(char ch)
{
  if ((ch >= '0') && (ch <= '9')) return ch - '0';
  if ((ch >= 'A') && (ch <= 'F')) return ch - 'A' + 10;
  if ((ch >= 'a') && (ch <= 'f')) return ch - 'a' + 10;
  return 0;
}

char *
EPub::retrieve_file(const char * fname, uint32_t & size)
{
  char name[256];
  uint8_t idx = 0;
  const char * s = fname;
  while ((idx < 255) && (*s != 0)) {
    if (*s == '%') {
      name[idx++] = (bin(s[1]) << 4) + bin(s[2]);
      s += 3;
    }
    else {
      name[idx++] = *s++;
    }
  }
  name[idx] = 0;

  std::string filename = opf_base_path;
  filename.append(name);

  // LOG_D("Retrieving file %s", filename.c_str());

  char * str = unzip.get_file(filename.c_str(), size);

  return str;
}

void
EPub::retrieve_fonts_from_css(CSS & css)
{
#if USE_EPUB_FONTS
  int8_t use_fonts_in_books;
  config.get(Config::Ident::USE_FONTS_IN_BOOKS, &use_fonts_in_books);

  if (use_fonts_in_books == 0) return;
  
  const CSS::PropertySuite * suite = css.get_property_suite("@font-face");

  if (suite == nullptr) return;

  bool first = true;

  for (auto & props : *suite) {
    const CSS::Values * values;
    if ((values = css.get_values_from_props(*props, CSS::PropertyId::FONT_FAMILY))) {

      Fonts::FaceStyle style       = Fonts::FaceStyle::NORMAL;
      Fonts::FaceStyle font_weight = Fonts::FaceStyle::NORMAL;
      Fonts::FaceStyle font_style  = Fonts::FaceStyle::NORMAL;
      std::string      font_family = values->front()->str;

      if ((values = css.get_values_from_props(*props, CSS::PropertyId::FONT_STYLE))) {
        font_style = (Fonts::FaceStyle) values->front()->choice.face_style;
      }
      if ((values = css.get_values_from_props(*props, CSS::PropertyId::FONT_WEIGHT))) {
        font_weight = (Fonts::FaceStyle) values->front()->choice.face_style;
      }
      style = fonts.adjust_font_style(style, font_style, font_weight);
      // LOG_D("Style: %d text-style: %d text-weight: %d", style, font_style, font_weight);

      if (fonts.get_index(font_family.c_str(), style) == -1) { // If not already loaded
        if ((values = css.get_values_from_props(*props, CSS::PropertyId::SRC)) &&
            (!values->empty()) &&
            (values->front()->value_type == CSS::ValueType::URL)) {

          if (first) {
            first = false;
            LOG_D("Displaying font loading msg.");
            msg_viewer.show(
              MsgViewer::INFO, 
              false, false, 
              "Retrieving Font(s)", 
              "The application is retrieving font(s) from the EPub file. Please wait."
            );
          }

          std::string filename = css.get_folder_path();
          filename.append(values->front()->str);
          uint32_t size;
          LOG_D("Font file name: %s", filename.c_str());
          unsigned char * buffer = (unsigned char *) retrieve_file(filename.c_str(), size);
          if (buffer == nullptr) {
            LOG_E("Unable to retrieve font file: %s", values->front()->str.c_str());
          }
          else {
            fonts.add(font_family, style, buffer, size);
          }
        }
      }
    }
  }
#endif
}

void
EPub::retrieve_css(pugi::xml_document & item_doc, 
                   CSSList & item_css_list, 
                   CSS ** item_css)
{
  xml_node      node;
  xml_attribute attr;
  
  if ((node = item_doc.child("html").child("head").child("link"))) {
    do {
      if ((attr = node.attribute("type")) &&
          (strcmp(attr.value(), "text/css") == 0) &&
          (attr = node.attribute("href"))) {

        std::string css_id = attr.value(); // uses href as id

        // search the list of css files to see if it already been parsed
        int16_t idx = 0;
        CSSList::iterator css_cache_it = css_cache.begin();

        while (css_cache_it != css_cache.end()) {
          if ((*css_cache_it)->get_id()->compare(css_id) == 0) break;
          css_cache_it++;
          idx++;
        }
        if (css_cache_it == css_cache.end()) {

          // The css file was not found. Load it in the cache.
          uint32_t size;
          std::string fname = current_item_file_path;
          fname.append(css_id.c_str());
          char * data = retrieve_file(fname.c_str(), size);

          if (data != nullptr) {
#if COMPUTE_SIZE
            memory_used += size;
#endif
            // LOG_D("CSS Filename: %s", fname.c_str());
            std::string path;
            extract_path(fname.c_str(), path);
            CSS * css_tmp = new CSS(path, data, size, css_id);
            if (css_tmp == nullptr) msg_viewer.out_of_memory("css temp allocation");
            free(data);

            // css_tmp->show();
            
            retrieve_fonts_from_css(*css_tmp);
                css_cache.push_back(css_tmp);
            item_css_list.push_back(css_tmp);
          }
        } 
        else {
          item_css_list.push_back(*css_cache_it);
        }
      }
    } while ((node = node.next_sibling("link")));
  }

  // Now look at <style> tags presents in the <html><head>, creating a temporary
  // css object for each of them.

  if ((node = item_doc.child("html").child("head").child("style"))) {
    do {
      CSS * css_tmp = new CSS(current_item_file_path, node.value(), strlen(node.value()) , "current-item");
      if (css_tmp == nullptr) msg_viewer.out_of_memory("css temp allocation");
      retrieve_fonts_from_css(*css_tmp);
      // css_tmp->show();
      temp_css_cache.push_back(css_tmp);
    } while ((node = node.next_sibling("style")));
  }

  // Populate the current_item css structure with property suites present in
  // the identified css files in the <meta> portion of the html file.

  if (*item_css != nullptr) delete *item_css;
  if ((*item_css = new CSS("", nullptr, 0, "MergedForItem")) == nullptr) {
    msg_viewer.out_of_memory("css allocation");
  }
  for (auto * css : item_css_list ) (*item_css)->retrieve_data_from_css(*css);
  for (auto * css : temp_css_cache) (*item_css)->retrieve_data_from_css(*css);
  // (*item_css)->show();
}


char * 
EPub::get_item(pugi::xml_node itemref, pugi::xml_document & item_doc)
{
  int err = 0;
  #define ERR(e) { err = e; break; }

  if (!file_is_open) return nullptr;

  // if ((current_item_data != nullptr) && (current_itemref == itemref))
  // return true;

  // clear_item_data();

  char *        item_data = nullptr;
  xml_node      node;
  xml_attribute attr;

  const char * id = itemref.attribute("idref").value();

  bool completed = false;

  while (!completed) {
    if (!(node = opf.child("package").child("manifest").child("item"))) ERR(1);
    for (; node; node = node.next_sibling("item")) {
      if ((attr = node.attribute("id")) && (strcmp(attr.value(), id) == 0)) break;
    }
    if (!node) ERR(2);

    if (!(attr = node.attribute("media-type"))) ERR(3);
    const char* media_type = attr.value();

    MediaType item_media_type;

    if      (strcmp(media_type, "application/xhtml+xml") == 0) item_media_type = MediaType::XML;
    else if (strcmp(media_type, "image/jpeg"           ) == 0) item_media_type = MediaType::JPEG;
    else if (strcmp(media_type, "image/png"            ) == 0) item_media_type = MediaType::PNG;
    else if (strcmp(media_type, "image/bmp"            ) == 0) item_media_type = MediaType::BMP;
    else if (strcmp(media_type, "image/gif"            ) == 0) item_media_type = MediaType::GIF;
    else ERR(4);

    if (!(attr = node.attribute("href"))) ERR(4);

    // LOG_D("Retrieving file %s", attr.value());

    uint32_t size;
    extract_path(attr.value(), current_item_file_path);

    // LOG_D("current_item_file_path: %s.", current_item_file_path.c_str());

    if ((item_data = retrieve_file(attr.value(), size)) == nullptr) ERR(5);

    if (item_media_type == MediaType::XML) {

      // LOG_D("Reading file %s", attr.value().c_str());

      xml_parse_result res = item_doc.load_buffer_inplace(item_data, size);
      if (res.status != status_ok) {
        LOG_E("item_doc xml load error: %d", res.status);
        item_doc.reset();
        if (item_data != nullptr) free(item_data);
        return nullptr;
      }

      // current_item.parse<0>(current_item_data);

      // if (css.size() > 0) css.clear();

      // Retrieve css files, puting them in the css_cache vector (as a cache).
      // The properties are then merged into the current_css map for the item
      // being processed.
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_current_item error: %d", err);
    item_doc.reset();
    if (item_data != nullptr) free(item_data);
    item_data = nullptr;
    //clear_item_data();
  }
  else {
    //LOG_D("EPub get_item retrieved item id: %s", id);
    current_itemref = itemref;
  }
  return item_data;
}

bool 
EPub::open_file(const std::string & epub_filename)
{
  if (file_is_open && (current_filename == epub_filename)) return true;
  if (file_is_open) close_file();

  page_locs.clear();
  
  #if COMPUTE_SIZE
    memory_used = 0;
  #endif

  LOG_D("Opening EPub file through unzip...");
  if (!unzip.open_zip_file(epub_filename.c_str())) {
    LOG_E("EPub open_file: Unable to open zip file: %s", epub_filename.c_str());
    return false;
  }

  if (!check_mimetype()) return false;

  LOG_D("Getting the OPF file");
  std::string filename;
  if (!get_opf_filename(filename)) return false;

  if (!get_opf(filename)) {
    LOG_E("EPub open_file: Unable to get opf of %s", epub_filename.c_str());
    unzip.close_zip_file();
    return false;
  }

  current_itemref_index = -1;
  current_filename      = epub_filename;
  file_is_open          = true;

  LOG_D("EPub file is now open.");

  return true;
}

void
EPub::clear_item_data()
{
  current_item.reset();
  free(current_item_data);
  current_item_data = nullptr;

  // for (auto * css : current_item_css_list) {
  //   delete css;
  // }
  current_item_css_list.clear();

  for (auto * css : temp_css_cache) {
    delete css;
  }
  temp_css_cache.clear();
}

bool 
EPub::close_file()
{
  if (!file_is_open) return true;

  clear_item_data();

  if (opf_data) {
    opf.reset();
    free(opf_data);
    opf_data = nullptr;
  }

  opf_base_path.clear();
  unzip.close_zip_file();

  for (auto * css : css_cache) delete css;

  css_cache.clear();
  fonts.clear();

  file_is_open = false;
  current_filename.clear();

  return true;
}

const char * 
EPub::get_meta(const std::string & name)
{
  if (!file_is_open) return nullptr;

  return opf.child("package").child("metadata").child_value(name.c_str());
}

const char *
EPub::get_cover_filename()
{
  if (!file_is_open) return nullptr;

  xml_node      node;
  xml_attribute attr;

  const char * itemref = nullptr;
  const char * filename = nullptr;

  // First, try to find it from metadata

  if ((node = opf.child("package").child("metadata").child("meta"))) {

    for (; node; node = node.next_sibling("meta")) {
      if ((attr = node.attribute("name")) && 
          (strcmp(attr.value(), "cover") == 0) && 
          (attr = node.attribute("content")) &&
          (itemref = attr.value())) {

        xml_node      n;
        xml_attribute a;

        if ((n = opf.child("package").child("manifest").child("item"))) {

          for (; n != nullptr; n = n.next_sibling("item")) {
            if ((a = n.attribute("id")) && 
                (strcmp(a.value(), itemref) == 0) &&
                (a = n.attribute("href"))) {
              filename = a.value();
            }
          }
        }

        break;
      }
    }
  }

  if (filename == nullptr) {
    // Look inside manifest

    if ((node = opf.child("package").child("manifest").child("item"))) {

      for (; node != nullptr; node = node.next_sibling("item")) {
        if ((attr = node.attribute("id")) && 
            (strcmp(attr.value(), "cover-image") == 0) && 
            (attr = node.attribute("href"))) {

          filename = attr.value();
        }
      }
    }
  }

  return filename;
}

bool 
EPub::get_first_item()
{
  current_itemref_index = 0;

  if (!file_is_open) return false;
  xml_node node;

  if (!((node = opf.child("package").child("spine").child("itemref"))))
    return false;

  // LOG_D("Getting item %s...", node.attribute("idref").value());

  clear_item_data();
  if ((current_itemref != node) && ((current_item_data = get_item(node, current_item)) == nullptr)) return false;
  retrieve_css(current_item, current_item_css_list, &current_item_css);
  current_itemref_index = 0;
  return true;
}

bool 
EPub::get_next_item()
{
  if (!file_is_open) return false;
  if (current_itemref.empty()) return false;
  xml_node node = current_itemref.next_sibling("itemref");

  clear_item_data();
  if (node) {
    // LOG_D("Getting item %s...", node.attribute("idref").value());
  
    current_itemref_index++;
    current_item_data = get_item(node, current_item);
  }

  if (current_item_data != nullptr) {
    retrieve_css(current_item, current_item_css_list, &current_item_css);
    return true;
  }
  else {
    current_itemref_index = 0;
    return false;
  }
}

int16_t 
EPub::get_item_count()
{
  if (!file_is_open) return 0;

  xml_node node;

  if (!((node = opf.child("package").child("spine").child("itemref"))))
    return 0;
  
  int16_t count = 0;
  do {
    count++;
  } while ((node = node.next_sibling("itemref")));

  return count;
}

bool 
EPub::get_item_at_index(int16_t itemref_index)
{
  if (!file_is_open) return false;

  if (current_itemref_index == itemref_index) return true;
  
  xml_node node;

  if (!((node = opf.child("package").child("spine").child("itemref"))))
    return false;

  int16_t index = 0;

  while (node && (index < itemref_index)) {
    node = node.next_sibling("itemref");
    index++;
  }

  if (node == nullptr) return false;

  if ((current_item_data == nullptr) || (current_itemref != node)) {
    current_itemref_index = index;
    clear_item_data();
    current_item_data = get_item(node, current_item);
    if (current_item_data != nullptr) {
      retrieve_css(current_item, current_item_css_list, &current_item_css);
    }
  }
  return current_item_data != nullptr;
}

bool 
EPub::get_item_at_index(int16_t        itemref_index, 
                        char **        item_data, 
                        xml_document & item_doc,
                        CSSList &      item_css_list,
                        CSS **         item_css)
{
  if (!file_is_open) return false;

  xml_node node;

  if (!((node = opf.child("package").child("spine").child("itemref"))))
    return false;

  int16_t index = 0;

  while (node && (index < itemref_index)) {
    node = node.next_sibling("itemref");
    index++;
  }

  if (node == nullptr) return false;

  *item_data = get_item(node, item_doc);
  if (*item_data != nullptr) {
    retrieve_css(current_item, item_css_list, item_css);
  }
  return *item_data != nullptr;
}

bool
EPub::get_image(std::string & filename, Page::Image & image, int16_t & channel_count)
{
  uint32_t size;
  uint8_t * data = (unsigned char *) epub.retrieve_file(filename.c_str(), size);

  if (data == NULL) {
    LOG_E("Unable to retrieve image file: %s", filename.c_str());
  }
  else {
    ImageInfo * info = get_image_info(data, size);
    #if EPUB_INKPLATE_BUILD
      uint32_t max_size = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) - 100e3;
    #else
      uint32_t max_size = 25e5;
    #endif

    if ((info == nullptr) || (info->size > max_size)) {
      free(data);
      LOG_E("Image is not valid or too large to load. Space available: %d", max_size);
      return false; // image format not supported or not valid
    }

    int w, h, c;
    image.bitmap = stbi_load_from_memory(data, size, &w, &h, &c, 1);
 
    image.dim = Dim(w, h);
    channel_count = c;

    free(data);

    if (image.bitmap != nullptr) return true;
  }

  return false;
}
