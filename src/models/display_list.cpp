#include "display_list.hpp"

#include "viewers/msg_viewer.hpp"

auto DisplayList::get_new_entry() -> DisplayListEntry * {
  DisplayListEntry *entry = entry_pool.newElement();
  if (entry == nullptr) {
    LOG_E("Failed to allocate a new DisplayListEntry from the pool");
    MsgViewer::out_of_memory("display list allocation");
  }
  // Not expected to come back here, but just in case, we return nullptr if the allocation
  // failed
  return entry;
}