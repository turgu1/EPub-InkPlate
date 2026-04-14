// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"
#include "himem.hpp"

#include "controllers/event_mgr.hpp"
#include "screen.hpp"
#include "viewers/page.hpp"

/**
 * @brief Message presentation class
 *
 * This class supply simple alert/info messages presentation to the user.
 *
 */
class MsgViewer {

private:
  static constexpr char const *TAG  = "MsgViewer";
  static constexpr uint16_t HEIGHT  = 300;
  static constexpr uint16_t HEIGHT2 = 450;

public:
  enum MsgType {
    INFO, ALERT, BUG, BOOK, WIFI, NTP_CLOCK, CONFIRM
  };
  static constexpr char iconChar[7] = {'I', '!', 'H', 'E', 'S', 'Y', '!'};

  class ConfirmData {
  public:
    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK || TOUCH_TRIAL
      Pos okPos, cancelPos;
      Dim buttonsDim;
    #endif
    bool ok{false};

    ConfirmData()  = default;
    ~ConfirmData() = default;

    static inline auto Make() { return makeUniqueHimem<ConfirmData>(); }
  };
  using ConfirmDataPtr = himemUniquePtr<class ConfirmData>;

  class ProgressData {
  public:
    Dim dim{0, 0};
    Pos pos{0, 0};
    uint16_t previousWidth{0};
    ProgressData()  = default;
    ~ProgressData() = default;
    static inline auto Make() { return makeUniqueHimem<ProgressData>(); }
  };
  using ProgressDataPtr = himemUniquePtr<class ProgressData>;

  static auto show(MsgType msgType, bool pressAKey, bool clearScreen, const char *title,
                   const char *fmtStr, ...) -> ConfirmDataPtr;

  /**
   * @brief Process a user input event for a confirmation dialog.
   *
   * For touch targets, this checks whether the event falls inside the OK or
   * CANCEL button rectangles. For key-based targets, SELECT is treated as OK.
   *
   * @param event Input event to evaluate.
   * @param confirmData Dialog state returned by show().
   * @return Pair containing:
   * - first: true when the dialog decision is complete, false otherwise.
   * - second: updated confirmation state (including the selected OK/CANCEL value).
   */
  static auto confirm(const EventMgr::Event &event, const ConfirmDataPtr confirmData)
      -> std::pair<bool, ConfirmDataPtr>;

  static auto showProgress(const char *title, ...) -> std::pair<PagePtr, ProgressDataPtr>;
  static auto updateProgress(PagePtr page, ProgressDataPtr progressData, uint16_t percent)
      -> std::pair<PagePtr, ProgressDataPtr>;

  /**
   * @brief Handle an unrecoverable out-of-memory condition.
   *
   * The method shows a blocking alert to the user, performs platform-specific
   * cleanup, and then powers down (device build) or terminates (Linux build).
   * This call does not return.
   *
   * @param reason Short description of the allocation failure cause.
   */
  static void outOfMemory(const char *reason);
};
