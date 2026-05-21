#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/html_interpreter.hpp"

#if EPUB_LINUX_BUILD
  #include <chrono>
  #include <thread>
#endif

#include <atomic>

using PageLocsInterpreterPtr = HimemUniquePtr<class PageLocsInterpreter>;
class PageLocsInterpreter : public HTMLInterpreter {
private:
  using PendingRequestCheck = auto (*)(void *) -> bool;

  PageLocsInterpreter(EPubPtr &theEpub, PagePtr &thePage, DOMPtr &theDom,
                      Page::ComputeMode theCompMode, const EPub::ItemInfo &theItem,
                      const std::atomic<bool> &abortFlag, PendingRequestCheck pendingRequestCheck,
                      void *pendingRequestContext)
      : HTMLInterpreter(theEpub, thePage, theDom, theCompMode, theItem), itemInfo(theItem),
        abortFlag(abortFlag), pendingRequestCheck(pendingRequestCheck),
        pendingRequestContext(pendingRequestContext) {}

public:
  ~PageLocsInterpreter() {}

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> HimemUniquePtr<T>;

  static inline auto Make(EPubPtr &theEpub, PagePtr &thePage, DOMPtr &theDom,
                          Page::ComputeMode theCompMode, const EPub::ItemInfo &theItem,
                          const std::atomic<bool> &abortFlag,
                          PendingRequestCheck pendingRequestCheck, void *pendingRequestContext) {
    return makeUniqueHimem<PageLocsInterpreter>(theEpub, thePage, theDom, theCompMode, theItem,
                                                abortFlag, pendingRequestCheck,
                                                pendingRequestContext);
  }

  auto inline docEnd(const Page::Format &fmt) -> void { pageEnd(fmt); }

private:
  const EPub::ItemInfo &itemInfo;
  const std::atomic<bool> &abortFlag;
  PendingRequestCheck pendingRequestCheck;
  void *pendingRequestContext;

protected:
  [[nodiscard]] inline auto pageEnd(const Page::Format &fmt) -> bool {
    bool res = true;

    PageId pageId = PageId(itemInfo.itemrefIndex, startOffset);

    PageLocs::PageInfo pageInfo = PageLocs::PageInfo(currentOffset - startOffset, -1);

    if ((pageInfo.size > 0) || ((pageId.itemrefIndex == 0) && (pageId.offset == 0))) {
      if (pageInfo.size == 0) {
        // Patch for the case when it's the title page and no picture is to be shown
        pageInfo.size = 1;
      }
      if ((itemInfo.itemrefIndex > 0) && (page->isEmpty())) {
        pageInfo.size = -pageInfo.size; // The page will not be counted nor displayed
      }

#if EPUB_LINUX_BUILD
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif

      pageLocs.insert(pageId, pageInfo);

#if DEBUGGING
      std::cout << pageId.offset << '|' << pageId.offset + pageInfo.size << ", "
                << pageInfo.pageNumber << ", " << pageInfo.size << std::endl;
#endif
    }

// Gives the chance to book_viewer to show a page if required
#if EPUB_LINUX_BUILD
    std::this_thread::yield();
#else
    vTaskDelay(pdMS_TO_TICKS(1));
#endif

    startOffset = currentOffset;

    page->start(fmt); // Start a new page

    // Abort at page boundaries when control has signalled an interrupt or when a queued
    // retriever request (typically GET_ASAP or STOP) is waiting to be handled next.
    // Partial pages already inserted into pagesMap are safe — insert() is idempotent.
    if (abortFlag.load(std::memory_order_relaxed) ||
        ((pendingRequestCheck != nullptr) && pendingRequestCheck(pendingRequestContext))) {
      return false;
    }

    return res;
  }
};
