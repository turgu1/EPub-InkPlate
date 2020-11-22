#define __COMMON_ACTIONS__ 1
#include "controllers/common_actions.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "viewers/msg_viewer.hpp"
#include "models/books_dir.hpp"

#if EPUB_INKPLATE6_BUILD
  #include "inkplate6_ctrl.hpp"
  #include "esp.hpp"
#endif

void
CommonActions::return_to_last()
{
  app_controller.set_controller(AppController::LAST);
}

void
CommonActions::show_last_book()
{
  books_dir_controller.show_last_book();
}

void
CommonActions::refresh_books_dir()
{
  int16_t temp;

  books_dir.refresh(nullptr, temp);
}

void
CommonActions::power_off()
{
  app_controller.going_to_deep_sleep();
  #if EPUB_INKPLATE6_BUILD
    msg_viewer.show(MsgViewer::INFO, false, true, "Power OFF",
      "Entering Deep Sleep mode. Please press a key to restart the device.");
    ESP::delay(500);
    inkplate6_ctrl.deep_sleep();
  #else
    exit(0);
  #endif
}

void
CommonActions::about()
{
  msg_viewer.show(
    MsgViewer::BOOK, 
    false,
    false,
    "About EPub-InkPlate", 
    "EPub EBook Reader Version %s for the InkPlate-6 e-paper display device. "
    "This application was made by Guy Turcotte, Quebec, QC, Canada, "
    "with great support from e-radionica.",
    APP_VERSION);
}
