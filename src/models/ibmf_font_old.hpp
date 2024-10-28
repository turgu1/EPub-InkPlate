#pragma once

#include <iostream>
#include <fstream>
#include <cstring>

#include "global.hpp"
#include "sys/stat.h"
#include "models/font.hpp"
#include "viewers/msg_viewer.hpp"

#include "screen.hpp"

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
          unsigned int  kern_idx:8;  // Ligature: replacement char code, kern: displacement
        } u;
        unsigned int stop:1;
        unsigned int tag:1;          // 0 = Ligature, 1 = Kern
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
    #pragma pack(pop)

  private:
    static constexpr char const * TAG = "IBMFFont";
    
    static constexpr uint8_t MAX_GLYPH_COUNT = 256;
    static constexpr uint8_t IBMF_VERSION    =   1;

    Font & font;

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
        uint16_t font_offsets[1];
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
        uint8_t    glyph_count;
        uint8_t    lig_kern_pgm_count;
        uint8_t    kern_count;
        uint8_t    version;
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

    Header      * header;
    GlyphInfo   * glyph_info_table[MAX_GLYPH_COUNT];
    LigKernStep * lig_kern_pgm;
    FIX16       * kerns;

    static constexpr uint8_t PK_REPEAT_COUNT =   14;
    static constexpr uint8_t PK_REPEAT_ONCE  =   15;

    GlyphInfo * glyph_info;
    Font::Glyph glyph;

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
      uint32_t i, j, k;

      uint8_t dyn_f = glyph.glyph_metric.dyn_f;

      while (true) {
        if (!get_nybble(nyb)) return false; i = nyb;
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
    retrieve_bitmap(GlyphInfo * glyph, uint8_t * bitmap, Dim8 dim, Pos8 offsets)
    {
      // point on the glyphs' bitmap definition
      memory_ptr = ((uint8_t *)glyph) + sizeof(GlyphInfo);
      uint8_t * rowp;

      if (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) {
        uint32_t  row_size = (dim.width + 7) >> 3;
        rowp = bitmap + (offsets.y * row_size);

        if (glyph->glyph_metric.dyn_f == 14) {  // is a bitmap?
          uint32_t  count = 8;
          uint8_t   data;

          for (uint32_t row = 0; 
               row < glyph->bitmap_height; 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < glyph->bitmap_width + offsets.x; 
                 col++) {
              if (count >= 8) {
                if (!getnext8(data)) {
                  LOG_E("Not enough bitmap data!");
                  return false;
                }
                // std::cout << std::hex << +data << ' ';
                count = 0;
              }
              rowp[col >> 3] |= (data & (0x80U >> count)) ? (0x80U >> (col & 7)) : 0;
              count++;
            }
          }
          // std::cout << std::endl;
        }
        else {
          uint32_t count = 0;

          repeat_count   = 0;
          nybble_flipper = 0xf0U;

          bool black = !(glyph->glyph_metric.first_is_black == 1);

          for (uint32_t row = 0; 
               row < glyph->bitmap_height; 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < glyph->bitmap_width + offsets.x; 
                 col++) {
              if (count == 0) {
                if (!get_packed_number(count, *glyph)) {
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
            while ((row < glyph->bitmap_height) && (repeat_count-- > 0)) {
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

        if (glyph->glyph_metric.dyn_f == 14) {  // is a bitmap?
          uint32_t  count = 8;
          uint8_t   data;

          for (uint32_t row = 0; 
               row < (glyph->bitmap_height); 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < (glyph->bitmap_width + offsets.x); 
                 col++) {
              if (count >= 8) {
                if (!getnext8(data)) {
                  LOG_E("Not enough bitmap data!");
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

          bool black = !(glyph->glyph_metric.first_is_black == 1);

          for (uint32_t row = 0; 
               row < (glyph->bitmap_height); 
               row++, rowp += row_size) {
            for (uint32_t col = offsets.x; 
                 col < (glyph->bitmap_width + offsets.x); 
                 col++) {
              if (count == 0) {
                if (!get_packed_number(count, *glyph)) {
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
      sizes = (uint8_t *) &memory[6 + (preamble->size_count * 2)];
      current_font = nullptr;

      return true;
    }

    bool
    load_data()
    {
      //for (uint8_t i = 0; i < MAX_GLYPH_COUNT; i++) glyph_info_table[i] = nullptr;
      memset(glyph_info_table, 0, sizeof(glyph_info_table));

      uint8_t byte;
      bool    result    = true;
      bool    completed = false;

      memory_ptr = current_font;

      header = (Header *) current_font;
      if (header->version != IBMF_VERSION) return false;

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
        }
        fclose(file);
      }
      else {
        std::cerr << "Unable to stat file %s!" << filename.c_str() << std::endl;
      }
    }

   ~IBMFFont() {
      if (memory_owner_is_the_instance && (memory != nullptr)) {
        delete [] memory;
        memory = nullptr;
      }
    }

    inline bool                   is_initialized() { return initialized;                         }
    inline uint8_t                 get_font_size() { return  header->point_size;                 }
    inline uint16_t              get_line_height() { return  header->line_height;                }
    inline int16_t          get_descender_height() { return - static_cast<int16_t>(header->descender_height);  }
    inline uint8_t                  get_char_set() { return preamble->bits.char_set;             }
    inline LigKernStep * get_lig_kern(uint8_t idx) { return &lig_kern_pgm[idx];                  }
    inline FIX16               get_kern(uint8_t i) { return kerns[i];                            }
    inline GlyphInfo * get_glyph_info(uint8_t glyph_code) { return glyph_info_table[glyph_code]; }

    bool
    get_glyph(uint32_t    & glyph_code, 
              Font::Glyph & app_glyph, 
              GlyphInfo  ** glyph_data, 
              bool          load_bitmap)
    {
      uint8_t     accent      = (glyph_code & 0x0000FF00) >> 8;
      GlyphInfo * accent_info = accent ? glyph_info_table[accent] : nullptr;

      if (((glyph_code & 0xFF) > MAX_GLYPH_COUNT) ||
          (glyph_info_table[glyph_code & 0xFF] == nullptr)) {
        std::cerr << "No entry for glyph code " 
                  << +glyph_code << " 0x" << std::hex << +glyph_code
                  << std::endl;         
        return false;
      }

      memset(&glyph, 0, sizeof(Font::Glyph));

      glyph_info = glyph_info_table[glyph_code & 0xFF];

      Dim8 dim           = Dim8(glyph_info->bitmap_width, glyph_info->bitmap_height);
      Pos8 offsets       = Pos8(0, 0);
      uint8_t added_left = 0;

      if (accent_info != nullptr) {
        offsets.x = ((glyph_info->bitmap_width > accent_info->bitmap_width) ?
                        ((glyph_info->bitmap_width - accent_info->bitmap_width) >> 1) : 0)
                    + ((((int32_t)glyph_info->bitmap_height - (header->x_height >> 6)) * header->slant_correction) >> 6);

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
        if (glyph_info->bitmap_width < accent_info->bitmap_width)  {
          added_left = (accent_info->bitmap_width - glyph_info->bitmap_width) >> 1;
          dim.width = accent_info->bitmap_width;
        }
        if (dim.width < (offsets.x + accent_info->bitmap_width)) {
          dim.width = offsets.x + accent_info->bitmap_width;
        }
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

      glyph.dim     =  Dim(dim.width, dim.height);
      glyph.xoff    = -(glyph_info->horizontal_offset + offsets.x);
      glyph.yoff    = -(glyph_info->vertical_offset   + offsets.y);
      glyph.advance =   glyph_info->advance >> 6;
      glyph.pitch   =  (screen.get_pixel_resolution() == Screen::PixelResolution::ONE_BIT) ?
                            (dim.width + 7) >> 3 : dim.width;
      glyph.ligature_and_kern_pgm_index = glyph_info->lig_kern_pgm_index;
      glyph.line_height = header->line_height;

      //bcopy(&glyph_info, &glyph, sizeof(Glyph));
      app_glyph  = glyph;
     *glyph_data = glyph_info;

      if (accent_info != nullptr) show_glyph(glyph, glyph_code);

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
};