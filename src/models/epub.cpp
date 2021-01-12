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
  opf_data               = nullptr;
  current_item_info.data = nullptr;
  file_is_open           = false;
  current_itemref        = xml_node(NULL);
  opf_base_path.clear();
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

  if (book_format_params.use_fonts_in_book == 0) return;
  
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
EPub::retrieve_css(ItemInfo & item)
{

  // Retrieve css files, puting them in the css_cache vector (as a cache).
  // The properties are then merged into the current_css map for the item
  // being processed.

  xml_node      node;
  xml_attribute attr;
  
  if ((node = item.xml_doc.child("html").child("head").child("link"))) {
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
          std::string fname = item.file_path;
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
            item.css_list.push_back(css_tmp);
          }
        } 
        else {
          item.css_list.push_back(*css_cache_it);
        }
      }
    } while ((node = node.next_sibling("link")));
  }

  // Now look at <style> tags presents in the <html><head>, creating a temporary
  // css object for each of them.

  if ((node = item.xml_doc.child("html").child("head").child("style"))) {
    do {
      CSS * css_tmp = new CSS(item.file_path, node.value(), strlen(node.value()) , "current-item");
      if (css_tmp == nullptr) msg_viewer.out_of_memory("css temp allocation");
      retrieve_fonts_from_css(*css_tmp);
      // css_tmp->show();
      item.css_cache.push_back(css_tmp);
    } while ((node = node.next_sibling("style")));
  }

  // Populate the current_item css structure with property suites present in
  // the identified css files in the <meta> portion of the html file.

  if (item.css != nullptr) delete item.css;
  if ((item.css = new CSS("", nullptr, 0, "MergedForItem")) == nullptr) {
    msg_viewer.out_of_memory("css allocation");
  }
  for (auto * css : item.css_list ) (item.css)->retrieve_data_from_css(*css);
  for (auto * css : item.css_cache) (item.css)->retrieve_data_from_css(*css);
  // (*item_css)->show();
}


bool 
EPub::get_item(pugi::xml_node itemref, 
               ItemInfo &     item)
{
  int err = 0;
  #define ERR(e) { err = e; break; }

  if (!file_is_open) return false;

  // if ((item.data != nullptr) && (current_itemref == itemref))
  // return true;

  clear_item_data(item);

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

    if      (strcmp(media_type, "application/xhtml+xml") == 0) item.media_type = MediaType::XML;
    else if (strcmp(media_type, "image/jpeg"           ) == 0) item.media_type = MediaType::JPEG;
    else if (strcmp(media_type, "image/png"            ) == 0) item.media_type = MediaType::PNG;
    else if (strcmp(media_type, "image/bmp"            ) == 0) item.media_type = MediaType::BMP;
    else if (strcmp(media_type, "image/gif"            ) == 0) item.media_type = MediaType::GIF;
    else ERR(4);

    if (!(attr = node.attribute("href"))) ERR(5);

    LOG_D("Retrieving file %s", attr.value());

    uint32_t size;
    extract_path(attr.value(), item.file_path);

    // LOG_D("item.file_path: %s.", item.file_path.c_str());

    if ((item.data = retrieve_file(attr.value(), size)) == nullptr) ERR(6);

    if (item.media_type == MediaType::XML) {

      LOG_D("Reading file %s", attr.value());

      xml_parse_result res = item.xml_doc.load_buffer_inplace(item.data, size);
      if (res.status != status_ok) {
        LOG_E("item_doc xml load error: %d", res.status);
        item.xml_doc.reset();
        if (item.data != nullptr) {
          free(item.data);
          item.data = nullptr;
        }
        return false;
      }

      // current_item.parse<0>(current_item_data);

      // if (css.size() > 0) css.clear();

     retrieve_css(item);
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_current_item error: %d", err);
    clear_item_data(item);
  }
  return completed;
}

void 
EPub::update_book_format_params()
{
  if (book_params == nullptr) {
    book_format_params = {
      .orientation       =  0,  // Get de compiler happy (no warning). Will be set below...
      .show_images       = -1,
      .font_size         = -1,
      .use_fonts_in_book = -1,
      .font              = -1
    };
  }
  else {
    book_params->get(BookParams::Ident::SHOW_IMAGES,        &book_format_params.show_images      );
    book_params->get(BookParams::Ident::FONT_SIZE,          &book_format_params.font_size        );
    book_params->get(BookParams::Ident::USE_FONTS_IN_BOOK,  &book_format_params.use_fonts_in_book);
    book_params->get(BookParams::Ident::FONT,               &book_format_params.font             );
  }

  config.get(Config::Ident::ORIENTATION, &book_format_params.orientation);

  if (book_format_params.show_images       == -1) config.get(Config::Ident::SHOW_IMAGES,        &book_format_params.show_images      );
  if (book_format_params.font_size         == -1) config.get(Config::Ident::FONT_SIZE,          &book_format_params.font_size        );
  if (book_format_params.use_fonts_in_book == -1) config.get(Config::Ident::USE_FONTS_IN_BOOKS, &book_format_params.use_fonts_in_book);
  if (book_format_params.font              == -1) config.get(Config::Ident::DEFAULT_FONT,       &book_format_params.font             );
}

void
EPub::open_params(const std::string & epub_filename)
{
  std::string params_filename = epub_filename.substr(0, epub_filename.find_last_of('.')) + ".pars";
  book_params = new BookParams(params_filename, false);
  if (book_params != nullptr) {
    book_params->read();
  }
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

  open_params(epub_filename);
  update_book_format_params();

  clear_item_data(current_item_info);

  current_filename                = epub_filename;
  file_is_open                    = true;

  LOG_D("EPub file is now open.");

  return true;
}

void
EPub::clear_item_data(ItemInfo & item)
{
  item.xml_doc.reset();
  if (item.data != nullptr) {
    free(item.data);
    item.data = nullptr;
  }

  // for (auto * css : current_item_css_list) {
  //   delete css;
  // }
  item.css_list.clear();

  for (auto * css : item.css_cache) delete css;
  item.css_cache.clear();

  item.itemref_index = -1;
}

bool 
EPub::close_file()
{
  if (!file_is_open) return true;

  clear_item_data(current_item_info);

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

  if (book_params != nullptr) {
    book_params->save();
    delete book_params;
    book_params = nullptr;
  }

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

  if (current_item_info.itemref_index == itemref_index) 
    return true;
  
  xml_node node;

  if (!((node = opf.child("package").child("spine").child("itemref")))) 
    return false;

  int16_t index = 0;

  while (node && (index < itemref_index)) {
    node = node.next_sibling("itemref");
    index++;
  }

  if (node == nullptr) return false;

  bool res = false;

  if ((current_item_info.data == nullptr) || (current_itemref != node)) {
    if ((res = get_item(node, current_item_info))) current_itemref = node;
    current_item_info.itemref_index = itemref_index;
  }
  return res;
}

// This is in support of the page locations retrieval mechanism. The ItemInfo
// is being used to retrieve asynchroniously the book page numbers without
// interfering with the main book viewer thread.
bool 
EPub::get_item_at_index(int16_t    itemref_index, 
                        ItemInfo & item)
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

  bool res = get_item(node, item);
  item.itemref_index = itemref_index;

  return res;
}

bool
EPub::get_image(std::string & filename, Page::Image & image, int16_t & channel_count)
{
  uint32_t size;
  uint8_t * data = (unsigned char *) epub.retrieve_file(filename.c_str(), size);

  if (data == nullptr) {
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

    if (image.bitmap != nullptr) {
      LOG_D("Image first pixel: %02x.", image.bitmap[0]);
      return true;
    }
  }

  return false;
}