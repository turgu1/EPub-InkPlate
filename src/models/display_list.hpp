#pragma once
#include "global.hpp"

#include <iostream>
#include <iterator>
#include <variant>

#include "memory_pool.hpp"
#include "picture.hpp"

#include "models/fonts.hpp"

enum class DisplayListCommand {
  GLYPH = 1, PICTURE, HIGHLIGHT, CLEAR_HIGHLIGHT, CLEAR_REGION, SET_REGION, ROUNDED, CLEAR_ROUNDED
};

struct GlyphEntry {      ///< Used for GLYPH
  Glyph *glyph{nullptr}; ///< Glyph
  int16_t kern{0};
  bool isSpace{false};
};

struct PictureEntry { ///< Used for PICTURE
  PicturePtr picture{nullptr};
  int16_t advance{0}; ///< Horizontal advance on the baseline
};

struct RegionEntry { ///< Used for HIGHLIGHT, CLEAR_HIGHLIGHT, SET_REGION and CLEAR_REGION
  Dim dim{0, 0};     ///< Region dimensions
};

class DisplayListEntry {
private:
  static constexpr char const *TAG = "DisplayListEntry";

public:
  DisplayListEntry *next{nullptr};

  DisplayListCommand command{DisplayListCommand::GLYPH}; ///< Command
  std::variant<std::monostate, GlyphEntry, PictureEntry, RegionEntry> v;
  Pos pos{0, 0}; ///< Screen coordinates

  DisplayListEntry() = default;
  ~DisplayListEntry() {
    // LOG_D("DisplayListEntry destructor called for command %d at pos (%d, %d)",
    //       static_cast<int>(command), pos.x, pos.y);
    if (command == DisplayListCommand::PICTURE && std::holds_alternative<PictureEntry>(v)) {
      std::get<PictureEntry>(v).picture.reset();
    }
  }
};

using DisplayListPool = MemoryPool<DisplayListEntry>;
using DisplayListPtr  = himemUniquePtr<class DisplayList>;

class DisplayList {
private:
  static constexpr char const *TAG = "DisplayList";

  DisplayListEntry *head{nullptr};
  DisplayListEntry *tail{nullptr};

  // For memory management of DisplayListEntry objects, the entryPool must be made
  // available by the Page class instance  to ensure that all DisplayList instances used
  // by the Page class share the same pool, allowing for efficient reuse and moving of
  // DisplayListEntry objects across different DisplayList instances.
  DisplayListPool &entryPool;

  DisplayList(DisplayListPool &pool) : entryPool(pool) {}

public:
  friend class Page;

  template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
  friend himemUniquePtr<T> makeUniqueHimem(Args &&...args);

  static inline auto Make(DisplayListPool &pool) { return makeUniqueHimem<DisplayList>(pool); }
  ~DisplayList() { clear(); }

  // Iterator class (nested)
  class Iterator {
  private:
    DisplayListEntry *current;

  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = DisplayListEntry;
    using pointer           = DisplayListEntry *;
    using reference         = DisplayListEntry &;

    Iterator(DisplayListEntry *entry) : current(entry) {}

    // Dereference operator
    DisplayListEntry *operator*() const { return current; }
    pointer operator->() const { return current; }

    // Prefix increment operator
    Iterator &operator++() {
      if (current) current = current->next;
      return *this;
    }

    // Postfix increment operator
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // Comparison operators
    bool operator==(const Iterator &a) const { return current == a.current; }
    bool operator!=(const Iterator &a) const { return current != a.current; }
  };

  // Methods to get iterators for range-based for loops
  Iterator begin() { return Iterator(head); }
  Iterator end() { return Iterator(nullptr); } // The "end" iterator points to nullptr

  // Utility function to add elements (for testing)
  void pushBack(DisplayListEntry *entry) {
    // if ((size_t)entry < 0x900000) {
    //   LOG_E("Invalid entry pointer: %p", reinterpret_cast<void *>(entry));
    // }

    // if (entry == nullptr) {
    //   LOG_E("Cannot add null entry to DisplayList");
    //   return;
    // }

    if (std::holds_alternative<std::monostate>(entry->v)) {
      LOG_E("Cannot add entry with no content");
      return;
    }
    if (empty()) {
      head = tail = entry;
    } else {
      tail->next = entry;
      tail       = entry;
    }
    entry->next = nullptr;
  }

  // Destructor to free memory (essential for manual memory management)
  void clear() {
    // NOTE: do NOT use the iterator here — deleteElement() recycles the nodemake
    // back into the pool's free-list, overwriting entry->next before ++it
    // can read it, causing a use-after-free.

    DisplayListEntry *current = head;
    while (current) {
      DisplayListEntry *next = current->next; // save before freeing
      entryPool.deleteElement(current);
      current = next;
    }
    head = tail = nullptr;
  }

  auto getNewEntry() -> DisplayListEntry *;

  void merge(DisplayList &other) {

    if (other.empty()) return;

    if (empty()) {
      head = other.head;
      tail = other.tail;
    } else {
      tail->next = other.head;
      tail       = other.tail;
    }
    other.head = other.tail = nullptr; // Clear the other list
  }

  void removeLast() {
    if (empty()) return;

    if (head == tail) {
      entryPool.deleteElement(head);
      head = tail = nullptr;
    } else {
      DisplayListEntry *current = head;
      while ((current) && (current->next != tail)) {
        // std::cout << " " << current;
        current = current->next;
      }
      if (!current) {
        LOG_E("Error in removeLast: tail (%p) not found in list", reinterpret_cast<void *>(tail));
      } else {
        entryPool.deleteElement(tail);
        tail       = current;
        tail->next = nullptr;
      }
    }
  }

  #if 0 // DEBUGGING
    #include <iostream>
    void show(const char *title) {
      std::cout << "--- " << title << " ---" << std::endl;
      for (auto *entry : *this) {
        switch (entry->command) {
        case DisplayListCommand::GLYPH: {
          auto &e = std::get<GlyphEntry>(entry->v);
          std::cout << "GLYPH" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.glyph->dim.width << " k:" << e.kern
                    << " h:" << e.glyph->dim.height << " isSpace:" << e.isSpace << std::endl;
        } break;
        case DisplayListCommand::PICTURE: {
          auto &e = std::get<PictureEntry>(entry->v);
          std::cout << "PICTURE" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.picture->getDim().width << " h:" << e.picture->getDim().height
                    << " advance:" << e.advance << std::endl;
        } break;
        case DisplayListCommand::HIGHLIGHT: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "HIGHLIGHT" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        case DisplayListCommand::CLEAR_HIGHLIGHT: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "CLEAR_HIGHLIGHT" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        case DisplayListCommand::ROUNDED: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "ROUNDED" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        case DisplayListCommand::CLEAR_ROUNDED: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "CLEAR_ROUNDED" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        case DisplayListCommand::CLEAR_REGION: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "CLEAR_REGION" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        case DisplayListCommand::SET_REGION: {
          auto &e = std::get<RegionEntry>(entry->v);
          std::cout << "SET_REGION" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.dim.width << " h:" << e.dim.height << std::endl;
        } break;
        default:
          std::cout << "UNKNOWN COMMAND" << std::endl;
        }
      }
      std::cout << "--- End ---" << std::endl;
    }
  #endif

  inline auto empty() const -> bool { return head == nullptr; }
  inline auto last() const -> DisplayListEntry * { return tail; }
};