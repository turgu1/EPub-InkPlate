// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/jpeg_image.hpp"

#include "helpers/unzip.hpp"
#include "viewers/msg_viewer.hpp"

#include "alloc.hpp"

#include "tjpgdec.hpp"

#include <iostream>
#include <iomanip>

static uint32_t  load_start_time;
static bool      waiting_msg_shown;

// static bool first = false;

static size_t in_func (     /* Returns number of bytes read (zero on error) */
    JDEC    * jd,    /* Decompression object */
    uint8_t * buff,  /* Pointer to the read buffer (null to remove data) */
    size_t    nbyte  /* Number of bytes to read/remove */
)
{
  if (buff) { /* Read data from imput stream */
    uint32_t size = nbyte;
    size_t res = unzip.get_stream_data((char *) buff, size) ? size : 0;
    // if (first) {
    //   first = false;
    //   std::cout << "----- Unzip content -----" << std::endl;
    //   std::cout << "Size read: " << size << std::endl;
    //   for (int i = 0; i < 200; i++) {
    //     std::cout << std::hex << std::setw(2) << +buff[i];
    //   }
    //   std::cout << std::endl << "-----" << std::endl;    
    // }
    return res;
  } else {    /* Remove data from input stream */
    return unzip.stream_skip(nbyte) ? nbyte : 0;
  }
}

/*------------------------------*/
/* User defined output funciton */
/*------------------------------*/

static int out_func (       /* Returns 1 to continue, 0 to abort */
    JDEC  * jd,      /* Decompression object */
    void  * bitmap,  /* Bitmap data to be output */
    JRECT * rect     /* Rectangular region of output image */
)
{
  static constexpr char const * TAG = "JPegImageOutFunc";
  
  Image::ImageData * image_data = (Image::ImageData *) jd->device;
  uint8_t * src, * dst;
  uint16_t y, bws, bwd;

  if (!waiting_msg_shown && ((ESP::millis() - load_start_time) > 2000)) {
    waiting_msg_shown = true;

    msg_viewer.show(
      MsgViewer::INFO, 
      false, false, 
      "Retrieving Image", 
      "The application is retrieving image(s) from the e-book file. Please wait."
    );
  }

  if ((rect->right >= image_data->dim.width) || (rect->bottom >= image_data->dim.height)) {
    LOG_E("Rect outside of image dimensions!");
    return 0;
  }
  /* Copy the output image rectangle to the frame buffer (assuming BW output) */
  src = (uint8_t *) bitmap;
  if (src == nullptr) return 0;

  dst = image_data->bitmap + (rect->top * image_data->dim.width + rect->left);  /* Left-top of destination rectangular */
  bws = (rect->right - rect->left + 1);     /* Width of output rectangular [byte] */
  bwd = image_data->dim.width;              /* Width of frame buffer [byte] */
  for (y = rect->top; y <= rect->bottom; y++) {
    memcpy(dst, src, bws);   /* Copy a line */
    src += bws; dst += bwd;  /* Next line */
  }

  return 1;    /* Continue to decompress */
}

JPegImage::JPegImage(std::string filename, Dim max, bool load_bitmap) : Image(filename)
{
  LOG_D("Loading image file %s", filename.c_str());

  if (unzip.open_stream_file(filename.c_str(), file_size)) {
    JRESULT   res;                /* Result code of TJpgDec API */
    JDEC      jdec;               /* Decompression object */
    uint8_t * work;
    size_t    sz_work = WORK_SIZE;

    /* Prepare to decompress */
    work = (uint8_t *) allocate(sz_work);
    res  = jdec_prepare(&jdec, in_func, work, sz_work, &image_data);
    if (res == JDR_OK) {
      uint8_t scale = 0;
      uint16_t width = jdec.width;
      while (max.width < width) {
        scale += 1;
        width >>= 1;
      }
      uint16_t height = jdec.height >> scale;
      while (max.height < height) {
        scale += 1;
        height >>= 1;
      }
      if (scale > 3) scale = 3;
      width  = jdec.width  >> scale;
      height = jdec.height >> scale;

      LOG_D("Image size: [%d, %d] %d bytes.", width, height, width * height);
      
      if (load_bitmap) {
        if ((image_data.bitmap = (uint8_t *) allocate(width * height)) != nullptr) {
          image_data.dim    = Dim(width, height);
          // first = true;
          load_start_time   = ESP::millis();
          waiting_msg_shown = false;
          res = jdec_decomp(&jdec, out_func, scale);
        }
      }
      else {
        image_data.dim = Dim(width, height);
      }
    }
    else {
      LOG_E("Unable to load image. Error code: %d", res);
    }

    free(work);
    unzip.close_stream_file();
  }
}
