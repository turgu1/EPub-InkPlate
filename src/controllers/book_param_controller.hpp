// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "controllers/book_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "models/epub.hpp"
#include "viewers/form_viewer.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"

class BookParamController {
private:
  static constexpr char const *TAG = "BookParamController";

  bool bookParamsFormIsShown{false};
  bool waitForKeyAfterWifi{false};
  bool deleteCurrentBook{false};

  EPubPtr epub{nullptr};

public:
  BookParamController() = default;

  inline void setOwnershipOfBook(EPubPtr &epubPtr) { epub = std::move(epubPtr); }

  MenuViewerPtr menuViewer;
  FormViewerPtr formViewer;

  MsgViewer::ConfirmDataPtr confirmData;

  void inputEvent(const EventMgr::Event &event);
  void enter();
  void leave(bool goingToDeepSleep = false);
  void setFontCount(uint8_t count);

  inline void setBookParamsFormIsShown() { bookParamsFormIsShown = true; }
  inline void setWaitForKeyAfterWifi() { waitForKeyAfterWifi = true; }
  inline void setDeleteCurrentBook() { deleteCurrentBook = true; }

  void bookParameters();
  void revertToDefaults();
  void booksList();
  void deleteBook();
  void tocCtrl();
  void wifiMode();
  void powerOff();
};

#if __BOOK_PARAM_CONTROLLER__
  BookParamController bookParamController;
#else
  extern BookParamController bookParamController;
#endif
