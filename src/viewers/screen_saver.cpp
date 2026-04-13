#if EPUB_INKPLATE_BUILD

  #define __SCREEN_SAVER__ 1
  #include "screen_saver.hpp"

  extern "C" {
  #include <dirent.h>
  }

  #include "picture.hpp"
  #include "picture_factory.hpp"
  #include "screen.hpp"

  #include "esp_random.h"

  #define SCREEN_SAVER_FOLDER MAIN_FOLDER "/screen_saver"

  auto ScreenSaver::setup() -> bool {

    DIR *dir = nullptr;

    picture_filenames.clear();

    if ((dir = opendir(SCREEN_SAVER_FOLDER)) != nullptr) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
          std::string filename = std::string(SCREEN_SAVER_FOLDER) + "/" + entry->d_name;
          std::string ext      = filename.substr(filename.find_last_of(".") + 1);
          if ((ext == "jpg") || (ext == "jpeg")) {
            picture_filenames.push_back(filename);
          }
        }
      }
      closedir(dir);
    }

    LOG_D("Found %d pictures.", picture_filenames.size());
    return picture_filenames.size() > 0;
  }

  void ScreenSaver::show() {
    LOG_D("Showing screen saver...");
    if (setup()) {
      if (picture_filenames.size() > 0) {
        int index = (esp_random() * (unsigned long long)picture_filenames.size()) >> 32;
        if (index < 0) index = -index;
        if (index >= picture_filenames.size()) index = picture_filenames.size() - 1;
        LOG_D("Showing picture at index %d: %s", index, picture_filenames[index].c_str());
        auto pict = PictureFactory::create(
            picture_filenames[index], Dim(Screen::get_width(), Screen::get_height()), true, true);
        if (pict) {
          // pict->show();
          page->show_cover(pict);
        } else {
          LOG_E("Unable to load picture file %s", picture_filenames[index].c_str());
        }
      }
    }
  }

#endif