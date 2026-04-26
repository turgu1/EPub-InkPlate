// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOOK_PARAM_CONTROLLER__ 1
#include "controllers/book_param_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/book_controller.hpp"
#include "controllers/books_dir_controller.hpp"
#include "controllers/common_actions.hpp"
#include "controllers/toc_controller.hpp"
#include "controllers/web_server.hpp"
#include "models/books_dir.hpp"
#include "models/config.hpp"
#include "models/epub.hpp"
#include "models/fonts.hpp"
#include "models/fonts_db.hpp"
#include "models/page_locs.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "eink.hpp"
  #include "esp.hpp"
  #include "esp_system.h"
  // #include "soc/rtc.h"
#endif

#include "book_param_controller.hpp"
#include <sys/stat.h>

static int8_t showPictures;
static int8_t fontSize;
static int8_t useFontsInBook;
static int8_t font;
static int8_t doneRes;

static int8_t oldFontSize;
static int8_t oldShowPictures;
static int8_t oldUseFontsInBook;
static int8_t oldFont;

#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 5;
#else
  static constexpr int8_t BOOK_PARAMS_FORM_SIZE = 4;
#endif
static FormEntry bookParamsFormEntries[BOOK_PARAMS_FORM_SIZE] = {
    {.caption   = "Font Size:",
     .u         = {.ch = {.value       = &fontSize,
                          .choiceCount = 4,
                          .choices     = FormChoiceField::fontSizeChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption   = "Use fonts in book:",
     .u         = {.ch = {.value       = &useFontsInBook,
                          .choiceCount = 2,
                          .choices     = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
    {.caption = "Font:",
     .u       = {.ch = {.value = &font, .choiceCount = 8, .choices = FormChoiceField::fontChoices}},
     .entryType = FormEntryType::VERTICAL},
    {.caption   = "Show Images in book:",
     .u         = {.ch = {.value       = &showPictures,
                          .choiceCount = 2,
                          .choices     = FormChoiceField::yesNoChoices}},
     .entryType = FormEntryType::HORIZONTAL},
#if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
    {.caption   = " DONE ",
     .u         = {.ch = {.value = &doneRes, .choiceCount = 0, .choices = nullptr}},
     .entryType = FormEntryType::DONE}
#endif
};

static constexpr char const *BOOK_PARAMS_CAPTION = "Current e-book parameters";

auto BookParamController::bookParameters() -> void {
  BookParams *bookParams = epub->getBookParams();

  bookParams->get(BookParams::Ident::SHOW_PICTURES, &showPictures);
  bookParams->get(BookParams::Ident::FONT_SIZE, &fontSize);
  bookParams->get(BookParams::Ident::USE_FONTS_IN_BOOK, &useFontsInBook);
  bookParams->get(BookParams::Ident::FONT, &font);

  if (showPictures == -1) config.get(Config::Ident::SHOW_PICTURES, &showPictures);
  if (fontSize == -1) config.get(Config::Ident::FONT_SIZE, &fontSize);
  if (useFontsInBook == -1) config.get(Config::Ident::USE_FONTS_IN_BOOKS, &useFontsInBook);
  if (font == -1) config.get(Config::Ident::DEFAULT_FONT, &font);

  oldShowPictures   = showPictures;
  oldUseFontsInBook = useFontsInBook;
  oldFont           = font;
  oldFontSize       = fontSize;
  doneRes           = 1;

  formViewer->show(BOOK_PARAMS_CAPTION, bookParamsFormEntries, BOOK_PARAMS_FORM_SIZE,
                   "(Any item change will trigger book refresh)");

  bookParamController.setBookParamsFormIsShown();
}

auto BookParamController::revertToDefaults() -> void {
  pageLocs.stopControlTask();

  EPub::BookFormatParams *bookFormatParams = epub->getBookFormatParams();

  BookParams *bookParams = epub->getBookParams();

  oldUseFontsInBook = bookFormatParams->useFontsInBook;
  oldFont           = bookFormatParams->font;

  constexpr int8_t defaultValue = -1;

  bookParams->put(BookParams::Ident::SHOW_PICTURES, defaultValue);
  bookParams->put(BookParams::Ident::FONT_SIZE, defaultValue);
  bookParams->put(BookParams::Ident::FONT, defaultValue);
  bookParams->put(BookParams::Ident::USE_FONTS_IN_BOOK, defaultValue);

  epub->updateBookFormatParams();

  bookParams->save();

  MsgViewer::show(MsgViewer::MsgType::INFO, false, false, "E-book parameters reverted",
                  "E-book parameters reverted to default values.");

  if (oldUseFontsInBook != bookFormatParams->useFontsInBook) {
    pageLocs.stopControlTask();
    if (bookFormatParams->useFontsInBook) {
      epub->loadFonts();
    } else {
      epub->getFonts().clear();
      epub->getFonts().clearGlyphCaches();
    }
  }

  if (oldFont != bookFormatParams->font) {
    pageLocs.stopControlTask();
    epub->getFonts().adjustDefaultFont(bookFormatParams->font);
  }
}

auto BookParamController::booksList() -> void {
  appController.setController(AppController::Ctrl::DIR);
}

auto BookParamController::deleteBook() -> void {
  confirmData =
      MsgViewer::show(MsgViewer::MsgType::CONFIRM, true, false, "Delete e-book",
                      "The e-book \"%s\" will be deleted. Are you sure?", epub->getTitle());
  bookParamController.setDeleteCurrentBook();
}

auto BookParamController::returnToBook() -> void {
  bookController.becomeOwnerOfBook(std::move(epub));
  appController.setController(AppController::Ctrl::BOOK);
}

auto BookParamController::tocCtrl() -> void {
  tocController.becomeOwnerOfBook(std::move(epub));
  appController.setController(AppController::Ctrl::TOC);
}

auto BookParamController::wifiMode() -> void {
  pageLocs.stopControlTask();

  #if EPUB_INKPLATE_BUILD
    epub->closeFile();
    appFonts.clear(true);
    appFonts.clearGlyphCaches();

    eventMgr.setStayOn(true); // DO NOT sleep

    if (startWebServer()) {
      bookParamController.setWaitForKeyAfterWifi();
    }
  #endif
}

auto BookParamController::powerOff() -> void {
  booksDirController.saveLastBook(bookController.getCurrentPageId(), true);
  pageLocs.stopControlTask();

  CommonActions::powerItOff();
}

// IMPORTANT!!
// The first (menu[0]) and the last menu entry (the one before END_MENU) MUST ALWAYS BE VISIBLE!!!

// clang-format off
static MenuViewer::MenuEntry menu[10] = {
  {MenuViewer::Icon::RETURN,      true,  true, nullptr, "Return to the e-books reader"},
  {MenuViewer::Icon::TOC,         false, true, nullptr, "Table of Content"},
  {MenuViewer::Icon::BOOK_LIST,   true,  true, nullptr, "E-Books list"},
  {MenuViewer::Icon::FONT_PARAMS, true,  true, nullptr, BOOK_PARAMS_CAPTION},
  {MenuViewer::Icon::REVERT,      true,  true, nullptr, "Revert e-book parameters to default values"},
  {MenuViewer::Icon::DELETE,      true,  true, nullptr, "Delete the current e-book"},
  {MenuViewer::Icon::WIFI,        true,  true, nullptr, "WiFi Access to the e-books folder"},
  {MenuViewer::Icon::INFO,        true,  true, nullptr, "About the EPub-InkPlate application"},
  // This entry must be the last one before END_MENU and MUST ALWAYS BE VISIBLE!!
  {MenuViewer::Icon::POWEROFF,    true,  true, nullptr, "Power OFF (Deep Sleep)"},
  {MenuViewer::Icon::END_MENU,    false, true, nullptr, nullptr}};
// clang-format on

auto BookParamController::setFontCount(uint8_t count) -> void {
  bookParamsFormEntries[2].u.ch.choiceCount = count;
}

auto BookParamController::enter() -> void {
  // Check if the TOC file exists for the current book.
  // If it doesn't, the TOC menu entry will not be shown.
  auto filePath = epub->getCurrentFilename();
  struct stat fileStat;
  auto dotPos = filePath.find_last_of('.');
  filePath.replace(dotPos, 5, ".toc");
  menu[1].visible = stat(filePath.c_str(), &fileStat) != -1;

  FontsDB *fontsDB{nullptr};
  config.get(Config::Ident::FONTS_DB, &fontsDB);
  setFontCount(fontsDB ? fontsDB->getStandardFontCount() : 0);

  menuViewer = MenuViewer::Make();
  formViewer = FormViewer::Make();

  if (menuViewer) {
    menu[0].func = [this]() { this->returnToBook(); };
    menu[1].func = [this]() { this->tocCtrl(); };
    menu[2].func = [this]() { this->booksList(); };
    menu[3].func = [this]() { this->bookParameters(); };
    menu[4].func = [this]() { this->revertToDefaults(); };
    menu[5].func = [this]() { this->deleteBook(); };
    menu[6].func = [this]() { this->wifiMode(); };
    menu[7].func = CommonActions::about;
    menu[8].func = [this]() { this->powerOff(); };

    menuViewer->show(menu);
  }
  bookParamsFormIsShown = false;
}

auto BookParamController::leave(bool goingToDeepSleep) -> void {
  menuViewer.reset();
  formViewer.reset();
}

auto BookParamController::inputEvent(const EventMgr::Event &event) -> void {
  if (bookParamsFormIsShown) {
    if (formViewer->event(event)) {
      bookParamsFormIsShown = false;
      // if (ok) {
      BookParams *bookParams = epub->getBookParams();

      if (showPictures != oldShowPictures)
        bookParams->put(BookParams::Ident::SHOW_PICTURES, showPictures);
      if (fontSize != oldFontSize) bookParams->put(BookParams::Ident::FONT_SIZE, fontSize);
      if (font != oldFont) bookParams->put(BookParams::Ident::FONT, font);
      if (useFontsInBook != oldUseFontsInBook)
        bookParams->put(BookParams::Ident::USE_FONTS_IN_BOOK, useFontsInBook);

      if (bookParams->isModified()) epub->updateBookFormatParams();

      bookParams->save();

      if (oldUseFontsInBook != useFontsInBook) {
        if (useFontsInBook) {
          epub->loadFonts();
        } else {
          epub->getFonts().clear();
          epub->getFonts().clearGlyphCaches();
        }
      }

      if (oldFont != font) {
        epub->getFonts().adjustDefaultFont(font);
      }
      // }
      // menuViewer.clearHighlight();
      menuViewer->show(menu, 3, true);
    }
  } else if (deleteCurrentBook) {
    if (confirmData) {

      bool result;
      std::tie(result, confirmData) = MsgViewer::confirm(event, std::move(confirmData));
      if (result) {
        if (confirmData->ok) {
          const HimemString &filePath = epub->getCurrentFilename();
          struct stat fileStat;

          if (stat(filePath.c_str(), &fileStat) != -1) {
            LOG_I("Deleting %s...", filePath.c_str());

            epub->closeFile();
            unlink(filePath.c_str());

            int16_t dotPos = filePath.find_last_of('.');

            HimemString paramsFilePath = filePath;
            paramsFilePath.replace(dotPos, 5, ".pars");

            if (stat(paramsFilePath.c_str(), &fileStat) != -1) {
              LOG_I("Deleting file : %s", paramsFilePath.c_str());
              unlink(paramsFilePath.c_str());
            }

            HimemString locsFilePath = filePath;
            locsFilePath.replace(dotPos, 5, ".locs");

            if (stat(locsFilePath.c_str(), &fileStat) != -1) {
              LOG_I("Deleting file : %s", locsFilePath.c_str());
              unlink(locsFilePath.c_str());
            }

            HimemString tocFilePath = filePath;
            tocFilePath.replace(dotPos, 5, ".toc");

            if (stat(tocFilePath.c_str(), &fileStat) != -1) {
              LOG_I("Deleting file : %s", tocFilePath.c_str());
              unlink(tocFilePath.c_str());
            }

            int16_t refreshIndex;
            booksDir.refresh(nullptr, refreshIndex, false);

            appController.setController(AppController::Ctrl::DIR);
          }
        } else {
          MsgViewer::show(MsgViewer::MsgType::INFO, false, false, "Canceled",
                          "The e-book was not deleted.");
        }
        deleteCurrentBook = false;
      }
    }
  }
  #if EPUB_INKPLATE_BUILD
  else if (waitForKeyAfterWifi) {
      MsgViewer::show(MsgViewer::MsgType::INFO, false, true, "Restarting",
                      "The device is now restarting. Please wait.");
      waitForKeyAfterWifi = false;
      stopWebServer();
      inkplate_platform.restart();
    }
  #endif
    else {
    if (menuViewer->event(event)) {
      LOG_I("Returning to book controller with updated EPub...");
      bookController.becomeOwnerOfBook(std::move(epub));
      appController.setController(AppController::Ctrl::BOOK);
    }
  }
}
