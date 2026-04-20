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

  inline auto becomeOwnerOfBook(EPubPtr &epubPtr) -> void { epub = std::move(epubPtr); }

  MenuViewerPtr menuViewer;
  FormViewerPtr formViewer;

  MsgViewer::ConfirmDataPtr confirmData;

  auto inputEvent(const EventMgr::Event &event) -> void;
  auto enter() -> void;
  auto leave(bool goingToDeepSleep = false) -> void;
  auto setFontCount(uint8_t count) -> void;

  inline auto setBookParamsFormIsShown() -> void { bookParamsFormIsShown = true; }
  inline auto setWaitForKeyAfterWifi() -> void { waitForKeyAfterWifi = true; }
  inline auto setDeleteCurrentBook() -> void { deleteCurrentBook = true; }

  auto bookParameters() -> void;
  auto revertToDefaults() -> void;
  auto booksList() -> void;
  auto deleteBook() -> void;
  auto returnToBook() -> void;
  auto tocCtrl() -> void;
  auto wifiMode() -> void;
  auto powerOff() -> void;
};

#if __BOOK_PARAM_CONTROLLER__
  BookParamController bookParamController;
#else
  extern BookParamController bookParamController;
#endif
