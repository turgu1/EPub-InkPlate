// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "jpeg_picture.hpp"

#include "himem.hpp"
#include "unzip.hpp"
// #include "viewers/msg_viewer.hpp"

#include "tjpgdec.hpp"

#include <iomanip>
#include <iostream>

#include <cstdio>

// static uint32_t load_start_time;
// static bool waiting_msg_shown;
FILE *jpegFile                   = nullptr;
static constexpr char const *TAG = "JPegPicture";
uint32_t fileLocation            = 0;

// static bool first = false;

static size_t inFunc(               /* Returns number of bytes read (zero on error) */
                     JDEC *jd,      /* Decompression object */
                     uint8_t *buff, /* Pointer to the read buffer (null to remove data) */
                     size_t nbyte   /* Number of bytes to read/remove */
) {
  if (buff) { /* Read data from imput stream */
    uint32_t size = nbyte;
    size_t res    = unzip.getStreamData((char *)buff, size); // ? size : 0;
    LOG_D("Read %zu bytes from input stream at location %xH", res, fileLocation);
    fileLocation += res;
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
  } else { /* Remove data from input stream */
    LOG_D("Skipping %zu bytes from input stream at location %xH", nbyte, fileLocation);
    fileLocation += nbyte;
    return unzip.streamSkip(nbyte) ? nbyte : 0;
  }
}

static size_t fileInFunc(               /* Returns number of bytes read (zero on error) */
                         JDEC *jd,      /* Decompression object */
                         uint8_t *buff, /* Pointer to the read buffer (null to remove data) */
                         size_t nbyte   /* Number of bytes to read/remove */
) {
  if (buff) { /* Read data from imput file */
    size_t res = fread(buff, 1, nbyte, jpegFile);
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
  } else { /* Remove data from input file */
    return fseek(jpegFile, nbyte, SEEK_CUR) == 0 ? nbyte : 0;
  }
}

/*------------------------------*/
/* User defined output funciton */
/*------------------------------*/

static int outFunc(              /* Returns 1 to continue, 0 to abort */
                   JDEC *jd,     /* Decompression object */
                   void *bitmap, /* Bitmap data to be output */
                   JRECT *rect   /* Rectangular region of output picture */
) {

  auto data = (JPegPicture::PictureData *)jd->device;
  uint8_t *src;
  uint16_t y, bws, bwd;

  // #if EPUB_INKPLATE_BUILD
  //   if (!waiting_msg_shown && ((ESP::millis() - load_start_time) > 2000)) {
  //     waiting_msg_shown = true;

  //     msg_viewer.show(MsgViewer::MsgType::INFO, false, false, "Retrieving Picture",
  //                     "The application is retrieving picture(s) from the e-book file. Please
  //                     wait.");
  //   }
  // #endif

  if ((rect->right >= data->dim.width) || (rect->bottom >= data->dim.height)) {
    LOG_E("Rect outside of picture dimensions!");
    return 0;
  }

  /* Copy the output picture rectangle to the frame buffer (assuming BW output) */
  src = (uint8_t *)bitmap;
  if (src == nullptr) return 0;

  auto dst = data->bitmap +
             (rect->top * data->dim.width + rect->left); /* Left-top of destination rectangular */
  bws      = (rect->right - rect->left + 1);             /* Width of output rectangular [byte] */
  bwd      = data->dim.width;                            /* Width of frame buffer [byte] */
  for (y = rect->top; y <= rect->bottom; y++) {
    memcpy(dst, src, bws); /* Copy a line */
    src += bws;
    dst += bwd; /* Next line */
  }

  return 1; /* Continue to decompress */
}

JPegPicture::JPegPicture(const HimemString &filename, Dim max, bool loadBitmap, bool fromFile)
    : Picture() {

  LOG_D("Loading picture file %s", filename.c_str());
  fileLocation = 0;

  if (fromFile) {
    loadFromFile(filename, max, loadBitmap);
  } else {
    if (unzip.openStreamFile(filename.c_str(), fileSize)) {
      JRESULT res; /* Result code of TJpgDec API */
      JDEC jdec;   /* Decompression object */
      auto work = makeUniqueHimem<uint8_t[]>(WORK_SIZE);
      if (work == nullptr) {
        LOG_E("Unable to allocate JPEG decoder work buffer.");
        unzip.closeStreamFile();
        return;
      }

      /* Prepare to decompress */
      res = jdec_prepare(&jdec, inFunc, work.get(), WORK_SIZE, &pictureData);
      if (res == JDR_OK) {
        uint8_t scale  = 0;
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
        width  = jdec.width >> scale;
        height = jdec.height >> scale;

        LOG_D("Picture size: [%d, %d] %d bytes.", width, height, width * height);

        if (loadBitmap) {
          if ((bitmap = makeUniqueHimem<uint8_t[]>(width * height)) != nullptr) {
            dim = Dim(width, height);
            // first = true;
            // #if EPUB_INKPLATE_BUILD
            //   load_start_time   = ESP::millis();
            //   waiting_msg_shown = false;
            // #endif
            pictureData = {.dim = dim, .bitmap = bitmap.get()};
            res         = jdec_decomp(&jdec, outFunc, scale);
            LOG_D("Decompression result: %d", res);
          }
        } else {
          dim = Dim(width, height);
        }
      } else {
        LOG_E("Unable to load picture. Error code: %d", res);
      }

      unzip.closeStreamFile();
    }
  }
}

auto JPegPicture::loadFromFile(const HimemString &filename, Dim max, bool loadBitmap) -> void {
  LOG_D("Loading picture from normal file %s", filename.c_str());

  jpegFile = fopen(filename.c_str(), "rb");
  if (jpegFile == nullptr) {
    LOG_E("Unable to open JPEG file: %s", filename.c_str());
    return;
  }

  fseek(jpegFile, 0L, SEEK_END);
  fileSize = ftell(jpegFile);
  fseek(jpegFile, 0L, SEEK_SET);

  JRESULT res; /* Result code of TJpgDec API */
  JDEC jdec;   /* Decompression object */
  auto work = makeUniqueHimem<uint8_t[]>(WORK_SIZE);
  if (work == nullptr) {
    LOG_E("Unable to allocate JPEG decoder work buffer.");
    fclose(jpegFile);
    return;
  }

  /* Prepare to decompress */
  res = jdec_prepare(&jdec, fileInFunc, work.get(), WORK_SIZE, &pictureData);
  if (res == JDR_OK) {
    uint8_t scale  = 0;
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
    width  = jdec.width >> scale;
    height = jdec.height >> scale;

    LOG_D("Picture size: [%d, %d] %d bytes.", width, height, width * height);

    if (loadBitmap) {
      if ((bitmap = makeUniqueHimem<uint8_t[]>(width * height)) != nullptr) {
        dim = Dim(width, height);
        // first = true;
        // #if EPUB_INKPLATE_BUILD
        //   load_start_time   = ESP::millis();
        //   waiting_msg_shown = false;
        // #endif
        pictureData = {.dim = dim, .bitmap = bitmap.get()};
        res         = jdec_decomp(&jdec, outFunc, scale);
      }
    } else {
      dim = Dim(width, height);
    }

  } else {
    LOG_E("Unable to prepare JPEG decoder. Error code: %d", res);
  }

  fclose(jpegFile);
}