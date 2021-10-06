#pragma once

#include <iostream>
#include <fstream>
#include <cstring>

#include "sys/stat.h"

/**
 * @brief Access to a PK font.
 * 
 * This is a class to allow acces to a PK font generated through the METAFONT
 * and the gftopk application.
 * 
 * This class is to validate the comprehension of the author about the PK file
 * format.
 */
class PKFont
{
  public:
    struct Glyph {
      uint32_t  tfm_width;
      uint32_t  packet_length;
      uint32_t  char_code;
      uint32_t  horizontal_escapement;
      uint32_t  vertical_escapement;
      uint32_t  bitmap_width;
      uint32_t  bitmap_height;
      int32_t   horizontal_offset;
      int32_t   vertical_offset;
      uint32_t  repeat_count;
      uint32_t  raster_size;
      uint8_t   dyn_f;
      bool      first_nibble_is_black;
      bool      is_a_bitmap;
      uint8_t * bitmap;
      Glyph() { bitmap = nullptr; }
    };
    

  private:
    static constexpr uint8_t MAX_GLYPH_COUNT = 128;

    bool initialized;
    bool memory_owner_is_the_instance;

    uint8_t  * memory;
    uint32_t   memory_length;

    uint8_t * memory_ptr;
    uint8_t * memory_end;

    uint8_t * glyph_table[MAX_GLYPH_COUNT];
    uint8_t   glyph_count;

    static constexpr uint8_t PK_XXX1         = 0xf0;
    static constexpr uint8_t PK_XXX2         = 0xf1;
    static constexpr uint8_t PK_XXX3         = 0xf2;
    static constexpr uint8_t PK_XXX4         = 0xf3;
    static constexpr uint8_t PK_YYY          = 0xf4;
    static constexpr uint8_t PK_POST         = 0xf5;
    static constexpr uint8_t PK_NO_OP        = 0xf6;
    static constexpr uint8_t PK_PRE          = 0xf7;

    static constexpr uint8_t PK_MARKER       =   89;

    static constexpr uint8_t PK_REPEAT_COUNT =   14;
    static constexpr uint8_t PK_REPEAT_ONCE  =   15;

    int32_t checksum;
    int32_t design_size;
    int32_t hppp;
    int32_t vppp;

    uint8_t    font_size;
    uint16_t   line_height;
    uint8_t    descender_height;

    Glyph glyph_info;

    bool
    getnextch(char & ch)
    {
      if (memory_ptr >= memory_end) return false;  
      ch = *memory_ptr++;
      return true;
    }

    bool
    getnext8(uint8_t & val)
    {
      if (memory_ptr >= memory_end) return false;  
      val = *memory_ptr++;
      return true;
    }

    bool
    getnext16(uint16_t & val)
    {
      if ((memory_ptr + 1) >= memory_end) return false;
      val  = *memory_ptr++ << 8;
      val |= *memory_ptr++;
      return true;
    }

    bool
    getnext24(uint32_t & val)
    {
      if ((memory_ptr + 2) >= memory_end) return false;
      val  = *memory_ptr++ << 16;
      val |= *memory_ptr++ << 8;
      val |= *memory_ptr++;
      return true;
    }

    bool
    getnext32(int32_t & val)
    {
      if ((memory_ptr + 3) >= memory_end) return false;
      val  = *memory_ptr++ << 24;
      val |= *memory_ptr++ << 16;
      val |= *memory_ptr++ << 8;
      val |= *memory_ptr++;
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
          val = j - 15 + ((13 - glyph_info.dyn_f) << 4) + glyph_info.dyn_f;
          break;
        }
        else if (i <= glyph_info.dyn_f) {
          val = i;
          break;
        }
        else if (i < PK_REPEAT_COUNT) {
          if (!get_nybble(nyb)) return false;
          val = ((i - glyph_info.dyn_f - 1) << 4) + nyb + glyph_info.dyn_f + 1;
          break;
        }
        else { 
          // if (glyph_info.repeat_count != 0) {
          //   std::cerr << "Spurious repeat_count iteration!" << std::endl;
          //   return false;
          // }
          if (i == PK_REPEAT_COUNT) {
            if (!get_packed_number(glyph_info.repeat_count)) return false;
          }
          else { // i == PK_REPEAT_ONCE
            glyph_info.repeat_count = 1;
          }
        }
      }
      return true;
    }

    bool
    glyph_short_preamble()
    {
      uint32_t val24;
      uint8_t val8;

      if (!getnext8(val8)) return false;
      glyph_info.packet_length = val8;

      if (!getnext8(val8)) return false;
      glyph_info.char_code = val8;

      if (!getnext24(val24)) return false;
      glyph_info.tfm_width = val24;

      if (!getnext8(val8)) return false;
      glyph_info.horizontal_escapement = val8;
      glyph_info.vertical_escapement   = 0;

      if (!getnext8(val8)) return false;
      glyph_info.bitmap_width = val8;

      if (!getnext8(val8)) return false;
      glyph_info.bitmap_height = val8;

      if (!getnext8(val8)) return false;
      glyph_info.horizontal_offset = (int8_t) val8;

      if (!getnext8(val8)) return false;
      glyph_info.vertical_offset = (int8_t) val8;

      glyph_info.raster_size = glyph_info.packet_length - 8;

      return true;
    }

    bool
    glyph_medium_preamble()
    {
      uint32_t val24;
      uint16_t val16;
      uint8_t val8;

      if (!getnext16(val16)) return false;
      glyph_info.packet_length = val16;

      if (!getnext8(val8)) return false;
      glyph_info.char_code = val8;

      if (!getnext24(val24)) return false;
      glyph_info.tfm_width = val24;

      if (!getnext16(val16)) return false;
      glyph_info.horizontal_escapement = val16;
      glyph_info.vertical_escapement   = 0;

      if (!getnext16(val16)) return false;
      glyph_info.bitmap_width = val16;

      if (!getnext16(val16)) return false;
      glyph_info.bitmap_height = val16;

      if (!getnext16(val16)) return false;
      glyph_info.horizontal_offset = (int16_t) val16;

      if (!getnext16(val16)) return false;
      glyph_info.vertical_offset = (int16_t) val16;

      glyph_info.raster_size = glyph_info.packet_length - 13;

      return true;
    }

    bool
    glyph_long_preamble()
    {
      int32_t val32;

      if (!getnext32(val32)) return false;
      glyph_info.packet_length = val32;

      if (!getnext32(val32)) return false;
      glyph_info.char_code = val32;

      if (!getnext32(val32)) return false;
      glyph_info.tfm_width = val32;

      if (!getnext32(val32)) return false;
      glyph_info.horizontal_escapement = val32;

      if (!getnext32(val32)) return false;
      glyph_info.vertical_escapement = val32;

      if (!getnext32(val32)) return false;
      glyph_info.bitmap_width = val32;

      if (!getnext32(val32)) return false;
      glyph_info.bitmap_height = val32;

      if (!getnext32(val32)) return false;
      glyph_info.horizontal_offset = val32;

      if (!getnext32(val32)) return false;
      glyph_info.vertical_offset = val32;

      glyph_info.raster_size = glyph_info.packet_length - 28;
      
      return true;
    }

    bool glyph_preamble(uint8_t byte)
    {
      glyph_info.dyn_f = byte >> 4;
      glyph_info.is_a_bitmap = glyph_info.dyn_f == 14;
      glyph_info.first_nibble_is_black = byte & 0x08;

      switch (byte & 0x07) {
        case 0:
        case 1:
        case 2:
        case 3:
          if (!glyph_short_preamble()) return false;
          glyph_info.packet_length += (byte & 3) << 8;
          break;
        case 4:
        case 5:
        case 6:
          if (!glyph_medium_preamble()) return false;
          glyph_info.packet_length += (byte & 3) << 16;
          break;
        case 7:
          if (!glyph_long_preamble()) return false;
      }

      glyph_info.repeat_count = 0;
      nybble_flipper          = 0xf0U;

      return true;
    }

    bool 
    preamble()
    {
      uint8_t i, len;

      if (getnext8(i)) {
        if (i != PK_MARKER) return false;

        if (getnext8(len)) memory_ptr += len;
        //   std::cout << "    Comment: ";
        //   for (int j = 0; j < len; j++) {
        //     char ch;
        //     if (!getnextch(ch)) return false;
        //     std::cout << ch;
        //   }
        //   std::cout << std::endl;
        // }

        if (!getnext32(design_size)) return false;
        if (!getnext32(checksum   )) return false; 
        if (!getnext32(hppp       )) return false; 
        if (!getnext32(vppp       )) return false;

        return true;
      }

      return false;
    }

    bool
    xxx(uint8_t type)
    {
      int32_t  len;
      uint32_t len24;
      uint16_t len16;
      uint8_t  len8;

      switch (type) {
        case 1: if (! getnext8(len8 )) return false; len = len8;  break;
        case 2: if (!getnext16(len16)) return false; len = len16; break;
        case 3: if (!getnext24(len24)) return false; len = len24; break;
        case 4: if (!getnext32(len  )) return false;              break;
        default:  return false;
      }
      // std::cout << "Block of " << (len + type) << " bytes." << std::endl;
      memory_ptr += len;
      return true; 
    }

    bool
    yyy()
    {
      memory_ptr += 4;
      return true;
    }

    bool
    retrieve_bitmap()
    {
      uint32_t  row_size = (glyph_info.bitmap_width + 7) >> 3;
      uint32_t  size     = row_size * glyph_info.bitmap_height;

      uint8_t * bitmap   = new uint8_t[size];

      if (bitmap == nullptr) {
        std::cerr << "Unable to allocation bitmap of size " << size << std::endl;
        return false;
      }
      memset(bitmap, 0, size);
      glyph_info.bitmap = bitmap;

      uint8_t * rowp = bitmap;
      
      if (glyph_info.is_a_bitmap) {
        uint32_t  count = 8;
        uint8_t   data;

        for (uint32_t row = 0; row < glyph_info.bitmap_height; row++, rowp += row_size) {
          for (uint32_t col = 0; col < glyph_info.bitmap_width; col++) {
            if (count >= 8) {
              if (!getnext8(data)) {
                std::cerr << "Not enough data!" << std::endl;
                delete [] bitmap;
                glyph_info.bitmap = nullptr;
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

        bool black = !glyph_info.first_nibble_is_black;

        for (uint32_t row = 0; row < glyph_info.bitmap_height; row++, rowp += row_size) {
          for (uint32_t col = 0; col < glyph_info.bitmap_width; col++) {
            if (count == 0) {
              if (!get_packed_number(count)) {
                delete [] bitmap;
                glyph_info.bitmap = nullptr;
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

          // if (glyph_info.repeat_count != 0) std::cout << "Repeat count: " << glyph_info.repeat_count << std::endl;
          while ((row < glyph_info.bitmap_height) && (glyph_info.repeat_count-- > 0)) {
            bcopy(rowp, rowp + row_size, row_size);
            row++;
            rowp += row_size;
          }

          glyph_info.repeat_count = 0;
        }
        // std::cout << std::endl;
      }

      return true;
    }

    int 
    load_glyph_table()
    {
      for (uint8_t i = 0; i < MAX_GLYPH_COUNT; i++) glyph_table[i] = nullptr;
      glyph_count = 0;

      uint8_t byte;
      bool result    = true;
      bool completed = false;

      memory_ptr = memory;

      while (result && !completed && (memory_ptr < memory_end)) {
        byte = *memory_ptr++;
        switch (byte) {
          case PK_XXX1: result = xxx(1); break;
          case PK_XXX2: result = xxx(2); break;
          case PK_XXX3: result = xxx(3); break;
          case PK_XXX4: result = xxx(4); break;
          case PK_YYY:  result = yyy( ); break;
          case PK_NO_OP:                 break;
          case PK_POST:
            completed = true;
            break;
          case PK_PRE:
            result = preamble();
            break;
          case 0xf8:
          case 0xf9:
          case 0xfa:
          case 0xfb:
          case 0xfc:
          case 0xfd:
          case 0xfe:
          case 0xff:
            std::cerr << "Unkown command byte: " << std::hex << +byte << std::endl;
            result = false;
            completed = true;
            break;
          default: {
            uint8_t * glyph_ptr = memory_ptr - 1;
            if (!(result = glyph_preamble(byte))) {
              std::cerr << "Glyph format issue..." << std::endl;
            }
            else {
              if (glyph_info.char_code < MAX_GLYPH_COUNT) { 
                glyph_count++;
                memory_ptr += glyph_info.raster_size;
                glyph_table[glyph_info.char_code] = glyph_ptr;
              }
              else {
                std::cerr << "Char code out of limits: " 
                          << std::hex << "0x" << glyph_info.char_code << std::endl; 
              }
            }
          }
            // completed = true;
        }
      }

      return result;
    }

  public:

    PKFont(uint8_t * memory_font, uint32_t size) 
        : memory(memory_font), 
          memory_length(size) { 
      font_size = 12; 
      line_height = (166 * font_size) / 72; 
      descender_height = line_height / 5; 

      memory_end = memory + memory_length;
      initialized = load_glyph_table();
      memory_owner_is_the_instance = false;
    }

    PKFont(const std::string filename) {
      struct stat file_stat;
      initialized = false;
      font_size = 12; 
      line_height = (166 * font_size) / 72;
      descender_height = line_height / 5; 
      if (stat(filename.c_str(), &file_stat) != -1) {
        FILE * file = fopen(filename.c_str(), "rb");
        memory = new uint8_t[memory_length = file_stat.st_size];
        memory_end = (memory == nullptr) ? nullptr : memory + memory_length;
        memory_owner_is_the_instance = true;
        if (memory != nullptr) {
          if (fread(memory, memory_length, 1, file) == 1) {
            initialized = load_glyph_table();
          }
        }
        fclose(file);
      }
      else {
        std::cerr << "Unable to stat file %s!" << filename.c_str() << std::endl;
      }
    }

   ~PKFont() {
      if (memory_owner_is_the_instance && (memory != nullptr)) {
        delete [] memory;
        memory = nullptr;
      }
    }

    inline uint8_t         get_font_size() { return font_size;        }
    inline uint16_t      get_line_height() { return line_height;      }
    inline uint16_t get_descender_height() { return descender_height; }

    void release_bitmap() { 
      if (glyph_info.bitmap != nullptr) { 
        delete [] glyph_info.bitmap;
        glyph_info.bitmap = nullptr;
      }
    }

    bool
    get_glyph(uint8_t & glyph_code, Glyph ** glyph, bool load_bitmap)
    {
      if ((glyph_code > MAX_GLYPH_COUNT) ||
          (glyph_table[glyph_code] == nullptr)) {
        std::cerr << "No entry for glyph code " 
                 << +glyph_code << " 0x" << std::hex << +glyph_code
                 << std::endl;         
        return false;
      }

      uint8_t temp;

      memset(&glyph_info, 0, sizeof(glyph_info));

      memory_ptr = glyph_table[glyph_code];

      uint8_t byte = *memory_ptr++;

      if (!glyph_preamble(byte)) {
        std::cerr << "Unable to retrieve glyph preamble." << std::endl;
        return false;
      }

      if (load_bitmap && retrieve_bitmap()) {}

      //bcopy(&glyph_info, &glyph, sizeof(Glyph));
      *glyph = &glyph_info;
      return true;
    }

    bool
    show_glyph(const Glyph & glyph)
    {
      std::cout << "Glyph Char Code: " << glyph.char_code << std::endl  
                << "  Metrics: [" << std::dec
                <<      glyph.bitmap_width  << ", " 
                <<      glyph.bitmap_height << "] "
                <<      glyph.raster_size   << std::endl
                << "  Position: ["
                <<      glyph.horizontal_offset << ", "
                <<      glyph.vertical_offset << ']' << std::endl
                << "  Bitmap available: " 
                <<      ((glyph.bitmap == nullptr) ? "No" : "Yes") << std::endl;

      if (glyph.bitmap == nullptr) return false;

      uint32_t        row, col;
      uint32_t        row_size = (glyph.bitmap_width + 7) >> 3;
      const uint8_t * row_ptr;

      std::cout << '+';
      for (col = 0; col < glyph.bitmap_width; col++) std::cout << '-';
      std::cout << '+' << std::endl;

      for (row = 0, row_ptr = glyph.bitmap; 
           row < glyph.bitmap_height; 
           row++, row_ptr += row_size) {
        std::cout << '|';
        for (col = 0; col < glyph.bitmap_width; col++) {
          std::cout << ((row_ptr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
        }
        std::cout << '|';
        std::cout << std::endl;
      }

      std::cout << '+';
      for (col = 0; col < glyph.bitmap_width; col++) std::cout << '-';
      std::cout << '+' << std::endl << std::endl;

      return true;
    }

    inline bool is_initialized() { return initialized; }
};