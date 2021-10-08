#pragma once

#include <iostream>
#include <fstream>
#include <cstring>

#include "global.hpp"
#include "sys/stat.h"
#include "models/font.hpp"
#include "viewers/msg_viewer.hpp"

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
        unsigned int next_char_code:7;
        unsigned int stop:1;
        union {
          unsigned int char_code:7;
          unsigned int kern_idx:7;  // Ligature: replacement char code, kern: displacement
        } u;
        unsigned int tag:1;         // 0 = Ligature, 1 = Kern
      };

      struct GlyphMetric {
        unsigned int dyn_f:4;
        unsigned int first_is_black:1;
        unsigned int preamble_kind:3;
      };

      struct GlyphData {
        uint8_t     char_code;
        uint8_t     bitmap_width;
        uint8_t     bitmap_height;
        int8_t      horizontal_offset;
        int8_t      vertical_offset;
        uint8_t     packet_length;
        FIX16       advance;
        GlyphMetric glyph_metric;
        uint8_t     lig_kern_pgm_index; // = 255 if none
      };
    #pragma pack(pop)

  private:
    static constexpr char const * TAG = "IBMFFont";
    
    static constexpr uint8_t MAX_GLYPH_COUNT = 128;
    static constexpr uint8_t IBMF_VERSION    =   1;

    Font & font;

    bool initialized;
    bool memory_owner_is_the_instance;

    #pragma pack(push, 1)
      struct Preamble {
        char marker[4];
        uint16_t size_count;
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

    uint8_t  * memory;
    uint32_t   memory_length;

    uint8_t  * memory_ptr;
    uint8_t  * memory_end;

    uint32_t    repeat_count;

    Preamble    * preamble;
    uint8_t     * sizes;

    uint8_t     * current_font;

    Header      * header;
    GlyphData   * glyph_data_table[MAX_GLYPH_COUNT];
    LigKernStep * lig_kern_pgm;
    FIX16       * kerns;

    static constexpr uint8_t PK_REPEAT_COUNT =   14;
    static constexpr uint8_t PK_REPEAT_ONCE  =   15;

    GlyphData * glyph_info;
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
    get_packed_number(uint32_t & val)
    {
      uint8_t  nyb;
      uint32_t i, j, k;

      uint8_t dyn_f = glyph_info->glyph_metric.dyn_f;

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
            if (!get_packed_number(repeat_count)) return false;
          }
          else { // i == PK_REPEAT_ONCE
            repeat_count = 1;
          }
        }
      }
      return true;
    }

    bool
    retrieve_bitmap()
    {
      uint32_t  row_size = (glyph_info->bitmap_width + 7) >> 3;
      uint16_t  size     = row_size * glyph_info->bitmap_height;

      uint8_t * bitmap   = font.byte_pool_alloc(size);
      if (bitmap == nullptr) {
        LOG_E("Unable to allocate memory for glyph's bitmap.");
        msg_viewer.out_of_memory("glyph allocation");
      }

      memset(bitmap, 0, size);
      glyph.buffer = bitmap;

      uint8_t * rowp = bitmap;

      repeat_count   = 0;
      nybble_flipper = 0xf0U;

      if (glyph_info->glyph_metric.dyn_f == 14) {  // is a bitmap?
        uint32_t  count = 8;
        uint8_t   data;

        for (uint32_t row = 0; row < glyph_info->bitmap_height; row++, rowp += row_size) {
          for (uint32_t col = 0; col < glyph_info->bitmap_width; col++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough data!" << std::endl;
                glyph.buffer = nullptr;
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
        uint32_t  count = 0;

        bool black = !(glyph_info->glyph_metric.first_is_black == 1);

        for (uint32_t row = 0; row < glyph_info->bitmap_height; row++, rowp += row_size) {
          for (uint32_t col = 0; col < glyph_info->bitmap_width; col++) {
            if (count == 0) {
              if (!get_packed_number(count)) {
                glyph.buffer = nullptr;
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
            bcopy(rowp, rowp + row_size, row_size);
            row++;
            rowp += row_size;
          }

          repeat_count = 0;
        }
        // std::cout << std::endl;
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
      //for (uint8_t i = 0; i < MAX_GLYPH_COUNT; i++) glyph_data_table[i] = nullptr;
      memset(glyph_data_table, 0, sizeof(glyph_data_table));

      uint8_t byte;
      bool    result    = true;
      bool    completed = false;

      memory_ptr = current_font;

      header = (Header *) current_font;
      if (header->version != IBMF_VERSION) return false;

      memory_ptr += sizeof(Header);
      for (int i = 0; i < header->glyph_count; i++) {
        glyph_info = (GlyphData *) memory_ptr;
        glyph_data_table[glyph_info->char_code] = (GlyphData *) memory_ptr;
        memory_ptr += sizeof(GlyphData) + glyph_info->packet_length;
        if (memory_ptr > memory_end) return false;
      }

      lig_kern_pgm = (LigKernStep *) memory_ptr;
      memory_ptr += sizeof(LigKernStep) * header->lig_kern_pgm_count;
      if (memory_ptr > memory_end) return false;

      kerns = (FIX16 *) memory_ptr;

      memory_ptr += sizeof(FIX16) * header->kern_count;
      if (memory_ptr != memory_end) return false;

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

    inline uint8_t         get_font_size() { return  header->point_size;          }
    inline uint16_t      get_line_height() { return  header->line_height;         }
    inline int16_t  get_descender_height() { return -(int16_t)header->descender_height;    }
    inline LigKernStep * get_lig_kern(uint8_t idx) { return &lig_kern_pgm[idx];   }
    inline FIX16       get_kern(uint8_t i) { return kerns[i];                     }
    inline GlyphData * get_glyph_data(uint8_t glyph_code) { return glyph_data_table[glyph_code]; }

    bool
    get_glyph(uint8_t & glyph_code, 
              Font::Glyph & app_glyph, 
              GlyphData ** glyph_data, 
              bool load_bitmap)
    {
      if ((glyph_code > MAX_GLYPH_COUNT) ||
          (glyph_data_table[glyph_code] == nullptr)) {
        std::cerr << "No entry for glyph code " 
                 << +glyph_code << " 0x" << std::hex << +glyph_code
                 << std::endl;         
        return false;
      }

      uint8_t temp;

      memset(&glyph, 0, sizeof(Font::Glyph));

      glyph_info = glyph_data_table[glyph_code];

      memory_ptr = ((uint8_t *) glyph_info) + sizeof(GlyphData);

      if (load_bitmap && retrieve_bitmap()) {}

      glyph.dim     =  Dim(glyph_info->bitmap_width, glyph_info->bitmap_height);
      glyph.xoff    = -glyph_info->horizontal_offset;
      glyph.yoff    = -glyph_info->vertical_offset;
      glyph.advance =  glyph_info->advance >> 6;
      glyph.pitch   = (glyph_info->bitmap_width + 7) >> 3;
      glyph.ligature_and_kern_pgm_index = glyph_info->lig_kern_pgm_index;
      glyph.line_height = header->line_height;

      //bcopy(&glyph_info, &glyph, sizeof(Glyph));
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
      return load_data();
    }

    bool
    show_glyph_data(const GlyphData & glyph, uint8_t char_code)
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
    show_glyph(const Font::Glyph & glyph, uint8_t char_code)
    {
      std::cout << "Glyph Char Code: " << char_code << std::endl
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