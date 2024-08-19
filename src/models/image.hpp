// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

// Class Image
//
// This is a virtual class. It allows for the retrieval of
// images that will have a maximum size as close as possible to the
// screen width and height or smaller. Images, when loaded, are converted
// to 3 bits gray scale pixels.
//
// The ImageFactory class will instanciate the proper class (PngImage or JPegImage)
// depending on the filename extension.

class Image
{
  public:
    struct ImageData {
      uint8_t * bitmap;
      Dim       dim;
      ImageData(Dim d, uint8_t * b) : bitmap(b), dim(d)  {}
      ImageData() : bitmap(nullptr), dim(Dim(0, 0))  { }
    };

  protected:
    bool        size_retrieved;
    Dim         orig_dim;
    ImageData   image_data;
    uint32_t    file_size;

  public:
    Image(std::string & filename) :
      size_retrieved(false),
      orig_dim(Dim(0, 0)),
      file_size(0)
    { } 
    ~Image() { free_bitmap(); }

    inline void free_bitmap() { 
      if (image_data.bitmap != nullptr) {
        free(image_data.bitmap); 
        image_data.bitmap = nullptr;
      }
    }
    
    inline const Dim &         get_orig_dim() const { return orig_dim;          }
    inline const Dim &              get_dim() const { return image_data.dim;    }
    inline const uint8_t       * get_bitmap() const { return image_data.bitmap; }
    inline const ImageData * get_image_data() const { return &image_data;       }   

    void resize(Dim new_dim);
    
    void retrieve_image_data(ImageData & target) {
      target.bitmap = image_data.bitmap;
      target.dim    = image_data.dim;
      image_data.bitmap = nullptr;
      image_data.dim    = Dim(0, 0);
    }

    void retrieve_bitmap(uint8_t ** bitmap) {
      * bitmap = image_data.bitmap;
      image_data.bitmap = nullptr;
      image_data.dim    = Dim(0,0);
    }

  private:
    static constexpr char const * TAG = "Image";
};
