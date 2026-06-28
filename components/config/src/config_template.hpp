// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include "fonts_db.hpp"

#include <array>
#include <fstream>
#include <inttypes.h>

template <class IdType, int cfg_size>
class ConfigBase {
  public:
    using Ident = IdType;
    enum class EntryType {
      STRING, INT, INT64, BYTE, FONTS_DB, DOUBLE
    };
    struct ConfigDescr {
      Ident ident;
      EntryType type;
      const char *caption;
      void *value;
      const void *default_value;
      uint8_t max_size;
    };
    using CfgType = std::array<ConfigDescr, cfg_size>;

  private:
    static constexpr char const *TAG = "Config";

    static CfgType cfg;
    const std::string config_filename;

    bool modified;
    bool comment;

    auto parseLine(char *buff, char **caption, char **value) -> bool;

  public:
    ConfigBase(const HimemString &conf_filename, bool show_comment)
      : config_filename(conf_filename), modified(false), comment(show_comment) {};

    ~ConfigBase() = default;

    auto get(IdType id, int32_t *val) -> bool;
    auto get(IdType id, int8_t *val) -> bool;
    auto get(IdType id, int64_t *val) -> bool;
    auto get(IdType id, HimemString &val) -> bool;
    auto get(IdType id, FontsDB **val) -> bool;
    auto get(IdType id, double *val) -> bool;
    auto put(IdType id, int32_t val) -> void;
    auto put(IdType id, int8_t val) -> void;
    auto put(IdType id, int64_t val) -> void;
    auto put(IdType id, HimemString &val) -> void;
    auto put(IdType id, double val) -> void;

    auto read() -> bool;
    auto save(bool force = false) -> bool;

    [[nodiscard]] inline auto isModified() -> bool { return modified; }

    #if DEBUGGING
      auto show() -> void;
    #endif
};

// ----- get(int32_t) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, int32_t *val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::INT)) {
      *val = *((int32_t *)entry.value);
      return true;
    }
  }
  return false;
}

// ----- get(int64_t) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, int64_t *val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::INT64)) {
      *val = *((int64_t *)entry.value);
      return true;
    }
  }
  return false;
}

// ----- get(int8_t) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, int8_t *val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::BYTE)) {
      *val = *((int8_t *)entry.value);
      return true;
    }
  }
  return false;
}

// ----- get(std::string) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, HimemString &val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::STRING)) {
      val.assign(((char *)entry.value));
      return true;
    }
  }
  return false;
}

// ----- get(FontsDB) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, FontsDB **val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::FONTS_DB)) {
      *val = (FontsDB *)entry.value;
      return true;
    }
  }
  return false;
}

// ----- get(double) -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::get(IdType id, double *val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::DOUBLE)) {
      *val = *((double *)entry.value);
      return true;
    }
  }
  return false;
}

// ----- put(int32_t) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, int32_t val) {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::INT)) {
      *((int32_t *)entry.value) = val;
      modified                  = true;
      return;
    }
  }
}

// ----- put(int64_t) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, int64_t val) {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::INT64)) {
      *((int64_t *)entry.value) = val;
      modified                  = true;
      return;
    }
  }
}

// ----- put(int8_t) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, int8_t val) {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::BYTE)) {
      *((int8_t *)entry.value) = val;
      modified                 = true;
      return;
    }
  }
}

// ---- put(std::string) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, HimemString &val) {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::STRING)) {
      strlcpy((char *)entry.value, val.c_str(), entry.max_size);
      modified = true;
      return;
    }
  }
}

// ---- put(double) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, double val) {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::DOUBLE)) {
      *((double *)entry.value) = val;
      modified                 = true;
      return;
    }
  }
}

// ----- parseLine() -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::parseLine(char *buff, char **caption, char **value) -> bool {
  bool done = false;

  for (;;) {
    uint8_t size = strlen(buff);
    if ((size > 0) && (buff[size - 1] == '\n')) { buff[size - 1] = 0; }

    // Get rid of blank lines, spaces at beginning of line and comments

    char *str = buff;
    while (*str == ' ') ++str;
    if ((*str == '#') || (*str == 0)) { break; }

    // isolate caption

    *caption = str;
    while ((*str != 0) && (*str != ' ') && (*str != '=')) ++str;
    if (*str == 0) { break; }
    if (*str == '=') {
      *str++ = 0;
    } else {
      *str++ = 0;
      while (*str == ' ') ++str;
      if (*str++ != '=') { break; }
    }
    while (*str == ' ') ++str;
    if (*str == 0) { break; }

    // isolate value

    if (*str == '"') {
      ++str;
      *value = str;
      while ((*str != 0) && (*str != '"')) ++str;
      if (*str == '"') {
        *str = 0;
      }
      else {
        break;
      }
    } else {
      *value = str;
      while (*str != 0) ++str;
      --str;
      while (*str == ' ') --str;
      *++str = 0;
    }
    done = true;
    break;
  }

  return done;
}

// ----- read() -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::read() -> bool {
  FontsDB *fontsDB     = nullptr;
  uint8_t  fontIndex    = 0;
  int8_t * fontIndexRef = nullptr;

  // First, initialize all configs to default values
  for (auto &entry : cfg) {
    switch (entry.type) {
    case EntryType::FONTS_DB:
      fontsDB = (FontsDB *)entry.value;
      // For the FONTS_DB entry, default_value points to the byte config variable
      // that stores the selected user font index.
      fontIndexRef = (int8_t *)entry.default_value;
      fontIndex    = (fontIndexRef != nullptr) ? *fontIndexRef : 0;
      break;
    case EntryType::STRING:
      strlcpy((char *)entry.value, (char *)entry.default_value, entry.max_size);
      break;
    case EntryType::INT:
      *((int32_t *)entry.value) = *((int32_t *)entry.default_value);
      break;
    case EntryType::INT64:
      *((int64_t *)entry.value) = *((int64_t *)entry.default_value);
      break;
    case EntryType::DOUBLE:
      *((double *)entry.value) = *((double *)entry.default_value);
      break;
    case EntryType::BYTE:
      *((int8_t *)entry.value) = *((int8_t *)entry.default_value);
      break;
    default:
      break;
    }
  }

  std::ifstream file(config_filename);

  if (!file.is_open()) {
    return save(true);
  }

  std::array<char, 128> buff{};
  char *                caption;
  char *                value;

  while (!file.eof()) {
    file.getline(buff.data(), buff.size());
    if (parseLine(buff.data(), &caption, &value)) {
      LOG_D("Caption: {}, value: {}", caption, value);
      if (*caption == '\0') { continue; }
      for (auto &entry : cfg) {
        if (strcmp(caption, entry.caption) == 0) {
          switch (entry.type) {
          case EntryType::STRING:
            strlcpy((char *)entry.value, value, entry.max_size);
            break;
          case EntryType::INT:
            *((int32_t *)entry.value) = atoi(value);
            break;
          case EntryType::INT64:
            *((int64_t *)entry.value) = atol(value);
            break;
          case EntryType::FONTS_DB:
            // FontsDB is an object pointer entry, not a scalar config value.
            // Its associated selected font index is handled via default_font.
            break;
          case EntryType::DOUBLE:
            *((double *)entry.value) = atof(value);
            break;
          case EntryType::BYTE:
            *((int8_t *)entry.value) = atoi(value);
            break;
          default:
            break;
          }
          break;
        }
      }
    }
  }

  file.close();

  if (fontIndexRef != nullptr) {
    fontIndex = *fontIndexRef;
  }

  return (fontsDB != nullptr) ? fontsDB->load(fontIndex) : true;
}

// ----- save() -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::save(bool force) -> bool {
  if (force || modified) {
    std::ofstream file(config_filename);
    if (!file.is_open()) { return false; }

    if (comment) {
      file << "# EPub-InkPlate Config File\n"
        "# -------------------------\n"
        "#\n"
        "# (This file will be reinitialized automatically)\n"
        "#\n"
        "# Please respect the content format:\n"
        "#\n"
        "# - Comments start with # (no comment on parameter lines).\n"
        "# - Parameters are of the form 'param_name = value' or 'param_name = \"value\"'.\n"
        "# - Blank lines are allowed.\n"
        "# - The following parameters are recognized:\n"
        "#\n";

      for (auto &entry : cfg) {
        if (entry.type != EntryType::FONTS_DB) {
          file << "#      " << entry.caption << std::endl;
        }
      }

      file << "# ---\n\n";
    }

    for (auto &entry : cfg) {
      switch (entry.type) {
      case EntryType::STRING:
        file << entry.caption << " = \"" << (char *)entry.value << '"' << std::endl;
        break;
      case EntryType::INT:
        file << entry.caption << " = " << *(int32_t *)entry.value << std::endl;
        break;
      case EntryType::INT64:
        file << entry.caption << " = " << *(int64_t *)entry.value << std::endl;
        break;
      case EntryType::FONTS_DB:
        // Do not serialize object entries.
        break;
      case EntryType::DOUBLE:
        file << entry.caption << " = " << *(double *)entry.value << std::endl;
        break;
      case EntryType::BYTE:
        file << entry.caption << " = " << +*(int8_t *)entry.value << std::endl;
        break;
      default:
        break;
      }
    }

    file.close();
    modified = false;
  }
  return true;
}

#if DEBUGGING

  // ----- show() -----

  template <class IdType, int cfg_size>
  void ConfigBase<IdType, cfg_size>::show() {
    LOG_D("Configuration");
    LOG_D("-------------");
    for (auto entry : cfg) {
      switch (entry.type) {
      case EntryType::STRING:
        LOG_D("{} = \"{}\"", entry.caption, (char *)entry.value);
        break;
      case EntryType::INT:
        LOG_D("{} = {}", entry.caption, *(int32_t *)entry.value);
        break;
      case EntryType::INT64:
        LOG_D("{} = {}", entry.caption, *(int64_t *)entry.value);
        break;
      case EntryType::FONTS_DB:
        LOG_D("{} = <FontsDB>", entry.caption);
        break;
      case EntryType::DOUBLE:
        LOG_D("{} = {}", entry.caption, *(double *)entry.value);
        break;
      case EntryType::BYTE:
        LOG_D("{} = {}", entry.caption, *(int8_t *)entry.value);
        break;
      default:
        break;
      }
    }
    LOG_D("---");
  }
#endif
