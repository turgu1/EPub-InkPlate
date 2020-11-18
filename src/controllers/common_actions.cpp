#define __COMMON_ACTIONS__ 1
#include "controllers/common_actions.hpp"

#include "controllers/app_controller.hpp"
#include "viewers/msg_viewer.hpp"
#include "models/books_dir.hpp"
#include "inkplate6_ctrl.hpp"
#include "esp.hpp"

void
CommonActions::return_to_last()
{
  app_controller.set_controller(AppController::LAST);
}

void
CommonActions::refresh_books_dir()
{
  books_dir.refresh();
}

void
CommonActions::power_off()
{
  MsgViewer::show(MsgViewer::INFO, false, true, "Power OFF",
    "Entering Deep Sleep mode. Please press a key to restart the device.");
  ESP::delay(500);
  inkplate6_ctrl.deep_sleep();
}

void
CommonActions::wifi_mode()
{
  MsgViewer::show_progress_bar("Petit test %d livres...", 31);
  MsgViewer::set_progress_bar(100);
}

void
CommonActions::about()
{
  MsgViewer::show(
    MsgViewer::BOOK, 
    false,
    false,
    "About EPub-InkPlate", 
    "EBook Reader Version %s for the InkPlate-6 e-paper display. "
    "Made by Guy Turcotte, Quebec, QC, Canada. "
    "With great support from E-Radionica.",
    APP_VERSION);
}
