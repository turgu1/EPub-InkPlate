#if EPUB_INKPLATE_BUILD

  #define __SCREEN_SAVER__ 1
  #include "screen_saver.hpp"

  extern "C" {
  #include <dirent.h>
  }

  #include "image.hpp"
  #include "image_factory.hpp"
  #include "screen.hpp"

  #include "viewers/page.hpp"

  #include "esp_random.h"

  #define SCREEN_SAVER_FOLDER MAIN_FOLDER "/screen_saver"

  auto ScreenSaver::setup() -> bool {

    DIR *dir = nullptr;

    image_filenames.clear();

    if ((dir = opendir(SCREEN_SAVER_FOLDER)) != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
          std::string filename = std::string(SCREEN_SAVER_FOLDER) + "/" + entry->d_name;
          std::string ext      = filename.substr(filename.find_last_of(".") + 1);
          if ((ext == "jpg") || (ext == "jpeg")) {
            image_filenames.push_back(filename);
          }
        }
      }
      closedir(dir);
    }

    LOG_D("Found %d images.", image_filenames.size());
    return image_filenames.size() > 0;
  }

  void ScreenSaver::show() {
    LOG_D("Showing screen saver...");
    if (setup()) {
      if (image_filenames.size() > 0) {
        int index = (esp_random() * (unsigned long long)image_filenames.size()) >> 32;
        if (index < 0) index = -index;
        if (index >= image_filenames.size()) index = image_filenames.size() - 1;
        LOG_D("Showing image at index %d: %s", index, image_filenames[index].c_str());
        auto img = ImageFactory::create(image_filenames[index],
                                        Dim(Screen::get_width(), Screen::get_height()), true, true);
        if (img) {
          // img->show();
          page.show_cover(std::move(img));
        } else {
          LOG_E("Unable to load image file %s", image_filenames[index].c_str());
        }
      }
    }
  }

#endif