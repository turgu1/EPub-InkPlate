#if EPUB_INKPLATE_BUILD

  #include "goto_deep_sleep.hpp"

  #include "screen.hpp"

  #include "controllers/app_controller.hpp"
  #include "controllers/event_mgr.hpp"
  #include "models/page_locs.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "viewers/screen_saver.hpp"

  #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
    #define MSG "Please press the WakeUp Button to restart the device."
    #define INT_PIN TouchScreen::INTERRUPT_PIN
    #define LEVEL 0
  #else
    #define MSG "Please press a key to restart the device."
    #define LEVEL 1
    #if EXTENDED_CASE
      #define INT_PIN PressKeys::INTERRUPT_PIN
    #else
      #define INT_PIN TouchKeys::INTERRUPT_PIN
    #endif
  #endif

void gotoDeepSleep(int8_t lightSleepDuration) {
  if (eventMgr.stayingOn()) {
    screen.forceFullUpdate();
    MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Waiting for Power OFF",
                    "Waiting for background tasks to complete before going to Deep Sleep.");
    while (eventMgr.stayingOn()) {
  #if EPUB_INKPLATE_BUILD
      ESP::delay(5000);
      if (pageLocs.isControlTaskReadyToBeStopped()) pageLocs.stopControlTask();
  #endif
    }
  } else {
    if (pageLocs.isControlTaskReadyToBeStopped()) pageLocs.stopControlTask();
  }

  screen.forceFullUpdate();
  if (lightSleepDuration > 0) {
    MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
                    "Timeout period exceeded (%d minutes). The device is now entering into Deep "
                    "Sleep mode. " MSG,
                    lightSleepDuration);
  } else {
    MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
                    "Entering Deep Sleep. " MSG);
  }

  auto screen_saver = ScreenSaver::Make();
  screen_saver->show();

  ESP::delay(5000);

  appController.goingToDeepSleep();

  inkplate_platform.deep_sleep(INT_PIN, LEVEL);
}

#endif