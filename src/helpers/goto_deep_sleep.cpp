#if EINK_BUILD

  #include "goto_deep_sleep.hpp"

  #include "platform.hpp"
  
  #include "screen.hpp"

  #include "controllers/app_controller.hpp"
  #include "controllers/event_mgr.hpp"
  #include "models/page_locs.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "viewers/screen_saver.hpp"

  #if WAKEUP_BUTTON_PRESENT
    #define WAKEUP_MSG "Please press the WakeUp Button to restart the device."
  #elif OLD_INKPLATE
    #define WAKEUP_MSG "Please press a key to restart the device."
    #define LEVEL 1
    #if EXTENDED_CASE
      #define INT_PIN PressKeys::INTERRUPT_PIN
    #else
      #define INT_PIN TouchKeys::INTERRUPT_PIN
    #endif
  #endif

  void gotoDeepSleep(int8_t timeOutDuration) {
    if (eventMgr.stayingOn()) {
      screen.forceFullUpdate();
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Waiting for Power OFF",
                      "Waiting for background tasks to complete before going to Deep Sleep.");
      while (eventMgr.stayingOn()) {
        #if EINK_BUILD
          ESP::delay(5000);
          if (pageLocs.isControlTaskReadyToBeStopped()) { pageLocs.stopControlTask(); }
        #endif
      }
    } else {
      if (pageLocs.isControlTaskReadyToBeStopped()) { pageLocs.stopControlTask(); }
    }

    screen.forceFullUpdate();
    if (timeOutDuration > 0) {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
                      "Timeout period exceeded (%d minutes). The device is now entering into Deep "
                      "Sleep mode. " WAKEUP_MSG,
                      timeOutDuration);
    } else {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Power OFF",
                      "Entering Deep Sleep. " WAKEUP_MSG);
    }

    appController.goingToDeepSleep();

    auto screenSaver = ScreenSaver::Make();
    screenSaver->show();

    ESP::delay(5000);

    platform.deepSleep();
  }

#endif