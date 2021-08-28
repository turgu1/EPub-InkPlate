#include "models/image.hpp"
#include "logging.hpp"

#include "alloc.hpp"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_SATURATE_INT

#include "stb_image_resize.h"

void 
Image::resize(Dim new_dim)
{
  LOG_D("Resize to [%d, %d] %d bytes.", new_dim.width, new_dim.height, new_dim.width * new_dim.height);

  if (image_data.bitmap != nullptr) {
    uint8_t * resized_bitmap = (uint8_t *) allocate(new_dim.width * new_dim.height);

    stbir_resize_uint8(image_data.bitmap, image_data.dim.width, image_data.dim.height, 0,
                       resized_bitmap,    new_dim.width,        new_dim.height,        0,
                       1);    

    free(image_data.bitmap);

    image_data.bitmap = resized_bitmap;
    image_data.dim    = new_dim; 
  }
}
