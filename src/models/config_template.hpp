// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once
#include "global.hpp"

#include <array>
#include <fstream>
#include <inttypes.h>

template <class IdType, int cfg_size>
class ConfigBase {
public:
  using Ident = IdType;
  enum class EntryType {
    STRING, INT, INT64, BYTE
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

  // const CfgType     & cfg;
  static CfgType cfg;
  const std::string config_filename;

  bool modified;
  bool comment;

  auto parseLine(char *buff, char **caption, char **value) -> bool;

public:
  ConfigBase(const std::string &conf_filename, bool show_comment)
      : config_filename(conf_filename), modified(false), comment(show_comment) {};
  // ConfigBase(const CfgType & conf, const std::string & conf_filename)
  //   : cfg(conf), config_filename(conf_filename), f(nullptr), modified(false) { };

  auto get(IdType id, int32_t *val) -> bool;
  auto get(IdType id, int8_t *val) -> bool;
  auto get(IdType id, int64_t *val) -> bool;
  auto get(IdType id, std::string &val) -> bool;
  void put(IdType id, int32_t val);
  void put(IdType id, int8_t val);
  void put(IdType id, int64_t val);
  void put(IdType id, std::string &val);

  auto read() -> bool;
  auto save(bool force = false) -> bool;

  inline auto isModified() -> bool { return modified; }

  #if DEBUGGING
    void show();
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
auto ConfigBase<IdType, cfg_size>::get(IdType id, std::string &val) -> bool {
  for (auto &entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::STRING)) {
      val.assign(((char *)entry.value));
      return true;
    }
  }
  return false;
}

// ----- put(int32_t) -----

template <class IdType, int cfg_size>
void ConfigBase<IdType, cfg_size>::put(IdType id, int32_t val) {
  for (auto entry : cfg) {
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
  for (auto entry : cfg) {
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
void ConfigBase<IdType, cfg_size>::put(IdType id, std::string &val) {
  for (auto entry : cfg) {
    if ((entry.ident == id) && (entry.type == EntryType::STRING)) {
      strlcpy((char *)entry.value, val.c_str(), entry.max_size);
      modified = true;
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
    if ((size > 0) && (buff[size - 1] == '\n')) buff[size - 1] = 0;

    // Get rid of blank lines, spaces at beginning of line and comments

    char *str = buff;
    while (*str == ' ') str++;
    if ((*str == '#') || (*str == 0)) break;

    // isolate caption

    *caption = str;
    while ((*str != 0) && (*str != ' ') && (*str != '=')) str++;
    if (*str == 0) break;
    if (*str == '=') {
      *str++ = 0;
    } else {
      *str++ = 0;
      while (*str == ' ') str++;
      if (*str++ != '=') break;
    }
    while (*str == ' ') str++;
    if (*str == 0) break;

    // isolate value

    if (*str == '"') {
      str++;
      *value = str;
      while ((*str != 0) && (*str != '"')) str++;
      if (*str == '"')
        *str = 0;
      else
        break;
    } else {
      *value = str;
      while (*str != 0) str++;
      str--;
      while (*str == ' ') str--;
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
  // First, initialize all configs to default values
  for (auto &entry : cfg) {
    if (entry.type == EntryType::STRING) {
      strlcpy((char *)entry.value, (char *)entry.default_value, entry.max_size);
    } else if (entry.type == EntryType::INT) {
      *((int32_t *)entry.value) = *((int32_t *)entry.default_value);
    } else if (entry.type == EntryType::INT64) {
      *((int64_t *)entry.value) = *((int64_t *)entry.default_value);
    } else {
      *((int8_t *)entry.value) = *((int8_t *)entry.default_value);
    }
  }

  std::ifstream *file = new std::ifstream(config_filename);

  if (!file->is_open()) {
    delete file;
    return save(true);
  }

  char *buff = new char[128];
  char *caption;
  char *value;

  while (!file->eof()) {
    file->getline(buff, 128);
    if (parseLine(buff, &caption, &value)) {
      LOG_D("Caption: %s, value: %s", caption, value);
      for (auto &entry : cfg) {
        if (strcmp(caption, entry.caption) == 0) {
          if (entry.type == EntryType::STRING) {
            strlcpy((char *)entry.value, value, entry.max_size);
          } else if (entry.type == EntryType::INT) {
            *((int32_t *)entry.value) = atoi(value);
          } else if (entry.type == EntryType::INT64) {
            *((int64_t *)entry.value) = atol(value);
          } else {
            *((int8_t *)entry.value) = atoi(value);
          }
          break;
        }
      }
    }
  }

  file->close();

  delete file;
  delete[] buff;

  return true;
}

// ----- save() -----

template <class IdType, int cfg_size>
auto ConfigBase<IdType, cfg_size>::save(bool force) -> bool {
  if (force || modified) {
    std::ofstream *file = new std::ofstream(config_filename);
    if (!file->is_open()) return false;

    if (comment) {
      *file << "# EPub-InkPlate Config File\n"
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
        *file << "#      " << entry.caption << std::endl;
      }

      *file << "# ---\n\n";
    }

    for (auto &entry : cfg) {
      if (entry.type == EntryType::STRING) {
        *file << entry.caption << " = \"" << (char *)entry.value << '"' << std::endl;
      } else if (entry.type == EntryType::INT) {
        *file << entry.caption << " = " << *(int32_t *)entry.value << std::endl;
      } else if (entry.type == EntryType::INT64) {
        *file << entry.caption << " = " << *(int64_t *)entry.value << std::endl;
      } else {
        *file << entry.caption << " = " << +*(int8_t *)entry.value << std::endl;
      }
    }
    file->close();
    delete file;
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
      if (entry.type == EntryType::STRING) {
        LOG_D("%s = \"%s\"", entry.caption, (char *)entry.value);
      } else if (entry.type == EntryType::INT) {
        LOG_D("%s = %" PRIi32, entry.caption, *(int32_t *)entry.value);
      } else if (entry.type == EntryType::INT64) {
        LOG_D("%s = %" PRIi64, entry.caption, *(int64_t *)entry.value);
      } else {
        LOG_D("%s = %" PRIi8, entry.caption, *(int8_t *)entry.value);
      }
    }
    LOG_D("---");
  }
#endif
