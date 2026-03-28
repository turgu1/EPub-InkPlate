#pragma once
#include "global.hpp"

#include <iterator>
#include <variant>

#include "image.hpp"
#include "memory_pool.hpp"

#include "models/fonts.hpp"
#include "viewers/msg_viewer.hpp"

enum class DisplayListCommand {
  GLYPH = 1, IMAGE, HIGHLIGHT, CLEAR_HIGHLIGHT, CLEAR_REGION, SET_REGION, ROUNDED, CLEAR_ROUNDED
};

struct GlyphEntry {      ///< Used for GLYPH
  Glyph *glyph{nullptr}; ///< Glyph
  int16_t kern{0};
  bool is_space{false};
};

struct ImageEntry { ///< Used for IMAGE
  ImagePtr image{nullptr};
  int16_t advance{0}; ///< Horizontal advance on the baseline
};

struct RegionEntry { ///< Used for HIGHLIGHT, CLEAR_HIGHLIGHT, SET_REGION and CLEAR_REGION
  Dim dim{0, 0};     ///< Region dimensions
};

class DisplayListEntry {
public:
  DisplayListEntry *next{nullptr};

  DisplayListCommand command{DisplayListCommand::GLYPH}; ///< Command
  std::variant<GlyphEntry, ImageEntry, RegionEntry> v;
  Pos pos{0, 0}; ///< Screen coordinates

  DisplayListEntry() = default;
  ~DisplayListEntry() {
    if (command == DisplayListCommand::IMAGE && std::holds_alternative<ImageEntry>(v)) {
      std::get<ImageEntry>(v).image.reset();
    }
  }
};

class DisplayList {
private:
  DisplayListEntry *head{nullptr};
  DisplayListEntry *tail{nullptr};

  static MemoryPool<DisplayListEntry> entry_pool;

public:
  DisplayList() = default;
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
  void push_back(DisplayListEntry *entry) {
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
    // NOTE: do NOT use the iterator here — deleteElement() recycles the node
    // back into the pool's free-list, overwriting entry->next before ++it
    // can read it, causing a use-after-free.
    DisplayListEntry *current = head;
    while (current) {
      DisplayListEntry *next = current->next; // save before freeing
      entry_pool.deleteElement(current);
      current = next;
    }
    head = tail = nullptr;
  }

  DisplayListEntry *get_new_entry() {
    DisplayListEntry *entry = entry_pool.newElement();
    if (entry == nullptr) msg_viewer.out_of_memory("display list allocation");
    // Not expected to come back here, but just in case, we return nullptr if the allocation
    // failed
    return entry;
  }

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

  void remove_last() {
    if (empty()) return;

    if (head == tail) {
      entry_pool.deleteElement(head);
      head = tail = nullptr;
    } else {
      DisplayListEntry *current = head;
      while (current->next != tail) {
        current = current->next;
      }
      entry_pool.deleteElement(tail);
      tail       = current;
      tail->next = nullptr;
    }
  }

  #if 1 // DEBUGGING
    void show(const char *title) {
      std::cout << "--- " << title << " ---" << std::endl;
      for (auto *entry : *this) {
        switch (entry->command) {
        case DisplayListCommand::GLYPH: {
          auto &e = std::get<GlyphEntry>(entry->v);
          std::cout << "GLYPH" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.glyph->dim.width << " k:" << e.kern
                    << " h:" << e.glyph->dim.height << " is_space:" << e.is_space << std::endl;
        } break;
        case DisplayListCommand::IMAGE: {
          auto &e = std::get<ImageEntry>(entry->v);
          std::cout << "IMAGE" << " x:" << entry->pos.x << " y:" << entry->pos.y
                    << " w:" << e.image->get_dim().width << " h:" << e.image->get_dim().height
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