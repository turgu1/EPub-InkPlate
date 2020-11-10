#define _TTF_ 1
#include "ttf2.hpp"

#include "screen.hpp"

FT_Library TTF::library{ nullptr };

TTF::TTF(const std::string & filename)
{
  face = nullptr;

  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
    }
  }

  set_font_face_from_file(filename);

  memory_font  = nullptr;
  current_size = -1;
}

TTF::TTF(unsigned char * buffer, int32_t size)
{
  face = nullptr;
  
  if (library == nullptr) {
    int error = FT_Init_FreeType(& library);
    if (error) {
      LOG_E("An error occurred during FreeType library initialization.");
    }
  }

  set_font_face_from_memory(buffer, size);
  current_size = -1;
}

TTF::~TTF()
{
  if (face != nullptr) clear_face();
}

void
TTF::clear_face()
{
  clear_cache();
  if (face != nullptr) FT_Done_Face(face);
  face = nullptr;
  free(memory_font);
  
  current_size = -1;
}

void
TTF::clear_cache()
{
  for (auto const & entry : cache) {
    for (auto const & glyph : entry.second) {
      free(glyph.second->buffer);
      free(glyph.second);      
    }
  }
  cache.clear();
  cache.reserve(50);
}

TTF::BitmapGlyph *
TTF::get_glyph(int32_t charcode)
{
  int error;
  GlyphsCache::iterator cit;
  Glyphs::iterator git;

  if (face == nullptr) return nullptr;

  cit = cache.find(current_size);
  bool found = (cit != cache.end()) &&
               ((git = cit->second.find(charcode)) != cit->second.end());

  if (found) {
    return &git->second;
  }
  else {
    int glyph_index = FT_Get_Char_Index(face, charcode);
    if (glyph_index == 0) {
      LOG_E("Charcode not found in face: %d", charcode);
      return nullptr;
    }
    else {
      error = FT_Load_Glyph(
            face,             /* handle to face object */
            glyph_index,      /* glyph index           */
            FT_LOAD_DEFAULT); /* load flags            */
      if (error) {
        LOG_E("Unable to load glyph for charcode: %d", charcode);
        return nullptr;
      }
      else {
        if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
          error = FT_Render_Glyph(face->glyph,            /* glyph slot  */
                                  FT_RENDER_MODE_NORMAL); /* render mode */
          if (error) {
            LOG_E("Unable to render glyph for charcode: %d", charcode);
            return nullptr;
          }
        }
      }
    }

    BitmapGlyph * glyph; (BitmapGlyph *) allocate(sizeof(BitmapGlyph));
    FT_Glyph ft_glyph;
    if (error = FT_Get_Glyph(face->glyph, (FT_Glyph *) &ft_glyph)) {
      LOG_E("Unable to copy glyph... Out of memory?");
      return nullptr;
    }
    cache[current_size][charcode] = glyph;

    glyph->width     = ft_glyph.width;
    glyph->rows      = ft_glyph.rows;
    glyph->xoff      = ft_glyph.xoff;
    glyph->yoff      = ft_glyph.yoff;
    glyph->advance   = ;
    glyph->left_side = ;

    return glyph;
  }
}

bool 
TTF::set_font_size(int16_t size)
{
  if (face == nullptr) return false;
  if (current_size == size) return true;

  int error = FT_Set_Char_Size(
          face,                 /* handle to face object           */
          0,                    /* char_width in 1/64th of points  */
          size * 64,            /* char_height in 1/64th of points */
          Screen::RESOLUTION,   /* horizontal device resolution    */
          Screen::RESOLUTION);  /* vertical device resolution      */

  if (error) {
    LOG_E("Unable to set font size.");
    return false;
  }

  current_size = size;
  return true;
}

bool 
TTF::set_font_face_from_file(const std::string font_filename)
{
  if (face != nullptr) clear_face();

  int error = FT_New_Face(library, font_filename.c_str(), 0, &face);
  if (error == FT_Err_Unknown_File_Format) {
    LOG_E("The font file format is unsupported: %s", font_filename);
    return false;
  }
  else if (error) {
    LOG_E("The font file could not be opened or read, or is broken: %s", font_filename);
    return false;
  }

  return true;
}

bool 
TTF::set_font_face_from_memory(unsigned char * buffer, int32_t size)
{
  if (face != nullptr) clear_face();

  int error = FT_New_Memory_Face(library, (const FT_Byte *) buffer, size, 0, &face);
  if (error) {
    LOG_E("The memory font format is unsupported or is broken.");
    return false;
  }

  memory_font = buffer;
  return true;
}