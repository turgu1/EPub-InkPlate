#define __EPUB__ 1
#include "epub.hpp"

#include "fonts.hpp"
#include "unzip.hpp"
#include "books_dir.hpp"
#include "logging.hpp"

#include "stb_image.h"

#include <cstring>
#include <string>

using namespace rapidxml;

static const char * TAG = "EPub";

void rapidxml::parse_error_handler(const char * what, void * where) { 
   LOG_E("RapidXML Parse error: %s.", what); 
   std::abort();
}

EPub::EPub()
{
  opf_data          = nullptr;
  current_item_data = nullptr;
  current_itemref   = nullptr;
  file_is_open      = false;
  opf_base_path.clear();
}

EPub::~EPub()
{
  close_file();
}

std::string 
extract_path(const char * fname)
{
  std::string p = "";
  int i = strlen(fname) - 1;
  while ((i > 0) && (fname[i] != '/')) i--;
  if (i > 0) p.assign(fname, ++i);
  return p;
}

bool 
EPub::get_opf()
{
  int err = 0;
  #define ERR(e) { err = e; break; }

  char * data;
  int32_t size;

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

  // A file named 'META-INF/container.xml' must be present and point to the OPF file
  LOG_D("Check container.xml.");
  if (!(data = unzip.get_file("META-INF/container.xml", size))) return false;
  
  xml_document<>    doc;
  xml_node<>      * node;
  xml_attribute<> * attr;

  doc.parse<0>(data);

  bool completed = false;
  while (!completed) {
    if (!(node = doc.first_node("container"))) ERR(1);
    if (!((attr = node->first_attribute("version")) && (strcmp(attr->value(), "1.0") == 0))) ERR(2);
    if (!(node = node->first_node("rootfiles"))) ERR(3);
    for (node = node->first_node("rootfile"); 
         node; 
         node = node->next_sibling("rootfile")) {
      //LOG_D(">> %s %d", node->name(), node->first_attribute("media-type")->value());

      if ((attr = node->first_attribute("media-type")) && 
          (strcmp(attr->value(), "application/oebps-package+xml") == 0)) break;
    }

    // Get the OPF and parse its XML content
    if (!node) ERR(4);
    if (!(attr = node->first_attribute("full-path"))) ERR(5);
    if (!(opf_data = unzip.get_file(attr->value(), size))) ERR(6);

    opf_base_path = extract_path(attr->value());

    opf.parse<0>(opf_data);

    // Verifie that the OPF is of one of the version understood by this application
    if (!((node = opf.first_node("package")) && 
          (attr = node->first_attribute("xmlns")) &&
          (strcmp(attr->value(), "http://www.idpf.org/2007/opf") == 0) &&
          (attr = node->first_attribute("version")) &&
          ((strcmp(attr->value(), "2.0") == 0) || (strcmp(attr->value(), "3.0") == 0)))) {
      LOG_E("This book is not compatible with this software.");
      break;
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_opf error: %d", err);
  }

  free(data);
  doc.clear();
  
  LOG_D("get_opf() completed.");

  return completed;
}

char *
EPub::retrieve_file(const std::string & fname, int32_t & size)
{
  std::string filename = opf_base_path;
  
  filename.append(fname);
  char * str = unzip.get_file(filename.c_str(), size);

  return str;
}

void 
EPub::retrieve_page_locs(int16_t idx)
{
  books_dir.get_page_locs(page_locs, idx);

  // int16_t page_nbr = 0;
  // for (auto & page : page_locs) {
  //   std::cout << 
  //     "Page " << page_nbr << 
  //     " ref:" << page.itemref_index << 
  //     " off:" << page.offset <<
  //     " siz:" << page.size << std::endl;
  //   page_nbr++;
  // }
}

void
EPub::retrieve_fonts_from_css(CSS & css)
{
#if USE_EPUB_FONTS
  const CSS::PropertySuite * suite = css.get_property_suite("@font-face");

  if (suite == nullptr) return;

  for (auto & props : *suite) {
    const CSS::Values * values;
    if ((values = css.get_values_from_props(*props, CSS::FONT_FAMILY))) {
      Fonts::FaceStyle style = Fonts::NORMAL;
      std::string font_family = values->front()->str;
      Fonts::FaceStyle font_weight = Fonts::NORMAL;
      Fonts::FaceStyle font_style = Fonts::NORMAL;
      if ((values = css.get_values_from_props(*props, CSS::FONT_STYLE))) {
        font_style = (Fonts::FaceStyle) values->front()->choice;
      }
      if ((values = css.get_values_from_props(*props, CSS::FONT_WEIGHT))) {
        font_weight = (Fonts::FaceStyle) values->front()->choice;
      }
      style = fonts.adjust_font_style(style, font_style, font_weight);
      // LOG_D("Style: %d text-style: %d text-weight: %d", style, font_style, font_weight);

      if (fonts.get_index(font_family.c_str(), style) == -1) { // If not already loaded
        if ((values = css.get_values_from_props(*props, CSS::SRC)) &&
            (!values->empty()) &&
            (values->front()->value_type == CSS::URL)) {
          std::string filename = css.get_folder_path();
          filename.append(values->front()->str);
          int32_t size;
          LOG_D("Font file name: %s", filename.c_str());
          unsigned char * buffer = (unsigned char *) retrieve_file(filename, size);
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

bool 
EPub::get_item(rapidxml::xml_node<> * itemref)
{
  int err = 0;
  #define ERR(e) { err = e; break; }

  if (!file_is_open) return false;

  if ((current_item_data != nullptr) && (current_itemref == itemref)) return true;
  
  clear_item_data();

  xml_node<>      * node;
  xml_attribute<> * attr;

  char * id = itemref->first_attribute("idref")->value();

  bool completed = false;

  while (!completed) {
    if (!((node = opf.first_node("package")) && 
          (node = node->first_node("manifest")) && 
          (node = node->first_node("item")))) ERR(1);
    for (; node != nullptr; node = node->next_sibling("item")) {
      if ((attr = node->first_attribute("id")) && (strcmp(attr->value(), id) == 0)) break;
    }
    if (!node) ERR(2);

    if (!(attr = node->first_attribute("media-type"))) ERR(3);
    char * media_type = attr->value();
    
    if (strcmp(media_type, "application/xhtml+xml") == 0) item_media_type = XML;
    else if (strcmp(media_type, "image/jpeg") == 0) item_media_type = JPEG;
    else if (strcmp(media_type, "image/png" ) == 0) item_media_type =  PNG;
    else if (strcmp(media_type, "image/bmp" ) == 0) item_media_type =  BMP;
    else if (strcmp(media_type, "image/gif" ) == 0) item_media_type =  GIF;
    else ERR(4);

    if (!(attr = node->first_attribute("href"))) ERR(4);

    int32_t size;
    current_item_file_path = extract_path(attr->value());

    if (!(current_item_data = retrieve_file(attr->value(), size))) ERR(5);

    if (item_media_type == XML) {

      // LOG_D("Reading file %s", attr->value().c_str());

      current_item.parse<parse_normalize_whitespace>(current_item_data);
      
      // current_item.parse<0>(current_item_data);
      
      //if (css.size() > 0) css.clear(); 

      // Retrieve css files, puting them in the css_cache vector (as a cache).
      // The properties are then merged into the current_css map for the item
      // being processed.

      if ((node = current_item.first_node("html"))) {
        if ((node = node->first_node("head"))) {
          if ((node = node->first_node("link"))) {
            do {
              if ((attr = node->first_attribute("type")) &&
                  (strcmp(attr->value(), "text/css") == 0) &&
                  (attr = node->first_attribute("href"))) {

                std::string css_id = attr->value(); // uses href as id

                // search the list of css files to see if it already been
                // parsed
                int16_t idx = 0;
                CSSList::iterator css_cache_it = css_cache.begin();
                while (css_cache_it != css_cache.end()) {
                  if ((*css_cache_it)->get_id()->compare(css_id) == 0) break;
                  css_cache_it++;
                  idx++;
                }
                if (css_cache_it == css_cache.end()) {

                  // The css file was not found. Load it in the cache.
                  int32_t size;
                  std::string fname = current_item_file_path;
                  fname.append(css_id.c_str());
                  char * data = retrieve_file(fname, size);
                  
                  if (data != nullptr) {
                    #if COMPUTE_SIZE
                      memory_used += size;
                    #endif
                    // LOG_D("CSS Filename: %s", fname.c_str());
                    CSS * css_tmp = new CSS(extract_path(fname.c_str()), data, size, css_id);
                    free(data);
                    // css_tmp->show();
                    retrieve_fonts_from_css(*css_tmp);
                    css_cache.push_back(css_tmp);
                    current_item_css_list.push_back(css_tmp);
                  }
                }
                else {
                  current_item_css_list.push_back(*css_cache_it);
                }
              }
            } while ((node = node->next_sibling("link")));
          }
        }
      }

      // Now look at <style> tags presents in the <html><head>, creating a temporary
      // css object for each of them.

      if ((node = current_item.first_node("html")) &&
          (node = node->first_node("head")) &&
          (node = node->first_node("style"))) {
        do {
          CSS * css_tmp = new CSS(current_item_file_path, node->value(), node->value_size() , "current-item");
          retrieve_fonts_from_css(*css_tmp);
          // css_tmp->show();
          temp_css_cache.push_back(css_tmp);
        } while ((node = node->next_sibling("style")));
      }

      // Populate the current_item css structure with property suites present in
      // the identified css files in the <meta> portion of the html file.

      if (current_item_css != nullptr) delete current_item_css;
      current_item_css = new CSS("", nullptr, 0, "MergedForItem");
      for (auto * css : current_item_css_list) {
        current_item_css->retrieve_data_from_css(*css);
      }
      for (auto * css : temp_css_cache) {
        current_item_css->retrieve_data_from_css(*css);
      }
      // current_item_css->show();
    }

    completed = true;
  }

  if (!completed) {
    LOG_E("EPub get_current_item error: %d", err);
    clear_item_data();
  }
  else {
    //LOG_D("EPub get_item retrieved item id: %s", id);
    current_itemref = itemref;
  }
  return completed;
}

bool 
EPub::open_file(const std::string & epub_filename)
{
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
  
  LOG_D("Getting the OPF file");
  if (!get_opf()) {
    LOG_E("EPub open_file: Unable to get opf of %s", epub_filename.c_str());
    unzip.close_zip_file();
    return false;
  }

  current_itemref_index = 0;
  file_is_open = true;

  LOG_D("EPub file is now open.");

  return true;
}

void
EPub::clear_item_data()
{
  current_item.clear();
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
    opf.clear();
    free(opf_data);
  }

  opf_base_path.clear();
  unzip.close_zip_file();

  for (auto * css : css_cache) {
    delete css;
  }
  css_cache.clear();
  fonts.clear();

  file_is_open = false;

  return true;
}

const char * 
EPub::get_meta(const std::string & name)
{
  if (!file_is_open) return nullptr;

  xml_node<> * node;
  //xml_attribute<> * attr;

  if (!((node = opf.first_node("package")) && 
        (node = node->first_node("metadata")) && 
        (node = node->first_node(name.c_str())))) return nullptr;

  return node->value();
}

const char *
EPub::get_cover_filename()
{
  if (!file_is_open) return nullptr;

  xml_node<>      * node;
  xml_attribute<> * attr;

  const char * itemref = nullptr;
  const char * filename = nullptr;

  // First, try to find it from metadata

  if ((node = opf.first_node("package")) &&
        (node = node->first_node("metadata")) &&
        (node = node->first_node("meta"))) {

    for (; node != nullptr; node = node->next_sibling("meta")) {
      if ((attr = node->first_attribute("name")) && 
          (strcmp(attr->value(), "cover") == 0) && 
          (attr = node->first_attribute("content")) &&
          (itemref = attr->value())) {

        xml_node<> *n;
        xml_attribute<> *a;

        if ((n = opf.first_node("package")) &&
            (n = n->first_node("manifest")) &&
            (n = n->first_node("item"))) {

          for (; n != nullptr; n = n->next_sibling("item")) {
            if ((a = n->first_attribute("id")) && 
                (strcmp(a->value(), itemref) == 0) &&
                (a = n->first_attribute("href"))) {
              filename = a->value();
            }
          }
        }

        break;
      }
    }
  }

  if (filename == nullptr) {
    // Look inside manifest

    if ((node = opf.first_node("package")) &&
          (node = node->first_node("manifest")) &&
          (node = node->first_node("item"))) {

      for (; node != nullptr; node = node->next_sibling("item")) {
        if ((attr = node->first_attribute("id")) && 
            (strcmp(attr->value(), "cover-image") == 0) && 
            (attr = node->first_attribute("href"))) {

          filename = attr->value();
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
  xml_node<> * node;

  if (!((node = opf.first_node("package")) &&
        (node = node->first_node("spine")) &&
        (node = node->first_node("itemref"))))
    return false;

  if ((current_itemref != node) && (!get_item(node))) return false;
  current_itemref_index = 0;
  return true;
}

bool 
EPub::get_next_item()
{
  if (!file_is_open) return false;
  if (current_itemref == nullptr) return false;
  xml_node<> * node = current_itemref->next_sibling("itemref");

  if (node) {
    current_itemref_index++;
    return get_item(node);
  }

  current_itemref_index = 0;
  return false;
}

bool 
EPub::get_item_at_index(int16_t itemref_index)
{
  if (!file_is_open) return false;

  if (current_itemref_index == itemref_index) return true;
  
  xml_node<>* node;

  if (!((node = opf.first_node("package")) &&
        (node = node->first_node("spine")) &&
        (node = node->first_node("itemref"))))
    return false;

  int16_t index = 0;

  while (node && (index < itemref_index)) {
    node = node->next_sibling("itemref");
    index++;
  }

  if (node == nullptr) return false;

  if ((current_item_data == nullptr) || (current_itemref != node)) {
    current_itemref_index = index;
    return get_item(node);
  }
  return true;
}

bool 
EPub::get_previous_item()
{
  if (!file_is_open) return false;
  if (current_itemref == nullptr) return false;
  xml_node<> * node = current_itemref->previous_sibling("itemref");

  if (node) {
    current_itemref_index--;
    return get_item(node);
  }

  current_itemref_index = 0;
  return false;
}

bool
EPub::get_image(std::string & filename, Page::Image & image, int16_t & channel_count)
{
  int32_t size;
  unsigned char * data = (unsigned char *) epub.retrieve_file(filename, size);

  if (data == NULL) {
    LOG_E("Unable to retrieve cover file: %s", filename.c_str());
  }
  else {
    int w, h, c;
    image.bitmap = stbi_load_from_memory(data, size, &w, &h, &c, 1);
 
    image.width   = w;
    image.height  = h;
    channel_count = c;

    free(data);

    if (image.bitmap != nullptr) return true;
  }

  return false;
}