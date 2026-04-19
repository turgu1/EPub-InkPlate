#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/epub.hpp"
#include "models/page_locs.hpp"
#include "viewers/html_interpreter.hpp"

using PageLocsInterpreterPtr = himemUniquePtr<class PageLocsInterpreter>;
class PageLocsInterpreter : public HTMLInterpreter {
private:
  PageLocsInterpreter(EPubPtr &theEpub, PagePtr &thePage, DOMPtr &theDom,
                      Page::ComputeMode theCompMode, const EPub::ItemInfo &theItem)
      : HTMLInterpreter(theEpub, thePage, theDom, theCompMode, theItem), itemInfo(theItem) {}

public:
  ~PageLocsInterpreter() {}

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend auto makeUniqueHimem(Args &&...args) -> himemUniquePtr<T>;

  static inline auto Make(EPubPtr &theEpub, PagePtr &thePage, DOMPtr &theDom,
                          Page::ComputeMode theCompMode, const EPub::ItemInfo &theItem) {
    return makeUniqueHimem<PageLocsInterpreter>(theEpub, thePage, theDom, theCompMode, theItem);
  }

  auto inline docEnd(const Page::Format &fmt) -> void { pageEnd(fmt); }

private:
  const EPub::ItemInfo &itemInfo;

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

      // sleep(1);

      pageLocs.insert(pageId, pageInfo);

      #if DEBUGGING
        std::cout << pageId.offset << '|' << pageId.offset + pageInfo.size << ", "
                  << pageInfo.pageNumber << ", " << pageInfo.size << std::endl;
      #endif
    }

    // Gives the chance to book_viewer to show a page if required
    std::this_thread::yield();

    startOffset = currentOffset;

    page->start(fmt); // Start a new page

    return res;
  }
};
