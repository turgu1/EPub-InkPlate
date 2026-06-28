#include "display_list.hpp"

auto DisplayList::getNewEntry() -> DisplayListEntry * {
  DisplayListEntry *entry = entryPool.newElement();
  if (entry == nullptr) {
    LOG_E("Failed to allocate a new DisplayListEntry from the pool");
    // MsgViewer::outOfMemory("display list allocation");
  }
  // Not expected to come back here, but just in case, we return nullptr if the allocation
  // failed
  return entry;
}