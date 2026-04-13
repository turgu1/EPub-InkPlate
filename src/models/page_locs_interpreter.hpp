#pragma once

#include "global.hpp"
#include "himem.hpp"

#include "models/epub.hpp"
#include "viewers/html_interpreter.hpp"

using PageLocsInterpreterPtr = himem_unique_ptr<class PageLocsInterpreter>;
class PageLocsInterpreter : public HTMLInterpreter {
private:
  PageLocsInterpreter(EPubPtr &the_epub, PagePtr &the_page, DOMPtr &the_dom,
                      Page::ComputeMode the_comp_mode, const EPub::ItemInfo &the_item)
      : HTMLInterpreter(the_epub, the_page, the_dom, the_comp_mode, the_item), item_info(the_item) {
  }

public:
  ~PageLocsInterpreter() {}

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himem_unique_ptr<T> make_unique_himem(Args &&...args);

  static inline auto Make(EPubPtr &the_epub, PagePtr &the_page, DOMPtr &the_dom,
                          Page::ComputeMode the_comp_mode, const EPub::ItemInfo &the_item) {
    return make_unique_himem<PageLocsInterpreter>(the_epub, the_page, the_dom, the_comp_mode,
                                                  the_item);
  }

  void doc_end(const Page::Format &fmt) { page_end(fmt); }

private:
  const EPub::ItemInfo &item_info;

protected:
  bool page_end(const Page::Format &fmt);
};
