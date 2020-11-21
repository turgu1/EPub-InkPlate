#define __CONFIG__ 1
#include "models/config.hpp"

#include "strlcpy.hpp"
#include "logging.hpp"

static int32_t  version;
static char     ssid[32];
static char     pwd[16];
static int32_t  port;
static int32_t  battery;
static char     orientation[7];

static int32_t  the_version     =  1;
static int32_t  default_port    = 80;
static int32_t  default_battery =  0;

std::array<Config::ConfigDescr, 6> Config::cfg = {{
 { VERSION,     INT,    "version",     &version,    &the_version,     0 },
 { SSID,        STRING, "wifi_ssid",   ssid,        "NONE",          32 },
 { PWD,         STRING, "wifi_pwd",    pwd,         "NONE",          16 },
 { PORT,        INT,    "http_port",   &port,       &default_port,    0 },
 { BATTERY,     INT,    "battery",     &battery,    &default_battery, 0 },
 { ORIENTATION, STRING, "orientation", orientation, "LEFT",           7 }
}};

bool 
Config::get(Ident id, int32_t * val)
{
  for (auto entry : cfg) {
    if((entry.ident == id) && (entry.type == INT)) {
      *val = * ((int32_t *) entry.value);
      return true;
    }
  }
  return false;
}

bool 
Config::get(Ident id, std::string & val)
{
  for (auto entry : cfg) {
    if ((entry.ident == id) && (entry.type == STRING)) {
      val.assign(((char *) entry.value));
      return true;
    }
  }
  return false;
}

void 
Config::put(Ident id, int32_t val)
{
  for (auto entry : cfg) {
    if ((entry.ident == id) && (entry.type == INT)) {
      *((int32_t *) entry.value) = val;
      modified = true;
      return;
    }
  }
}

void 
Config::put(Ident id, std::string & val)
{
  for (auto entry : cfg) {
    if ((entry.ident == id) && (entry.type == STRING)) {
      strlcpy((char *) entry.value, val.c_str(), entry.max_size);
      modified = true;
      return;
    }
  }
}

bool 
Config::parse_line(char * buff, uint16_t max_size, char ** caption, char ** value)
{
  while (1) {
    if (fgets(buff, max_size, f) == nullptr) return false;
    uint8_t size = strlen(buff);
    if (buff[size - 1] == '\n') buff[size - 1] = 0;

    // Get rid of blank lines, spaces at beginning of line and comments

    char * str = buff;
    while (*str == ' ') str++;
    if ((*str == '#') || (*str == 0)) continue;

    // isolate caption

    *caption = str;
    while ((*str != 0) && (*str != ' ') && (*str != '=')) str++;
    if (*str == 0) return false;
    if (*str == '=') {
      *str++ = 0;
    }
    else {
      *str++ = 0;
      while (*str == ' ') str++;
      if (*str++ != '=') return false;
    }
    while (*str == ' ') str++;
    if (*str == 0) return false;

    // isolate value

    if (*str == '"') {
      str++;
      *value = str;
      while ((*str != 0) && (*str != '"')) str++;
      if (*str == '"') *str = 0; else return false;
    }
    else {
      *value = str;
      while (*str != 0) str++;
      str--;
      while (*str == ' ') str--;
      *++str = 0;
    }

    break;
  }
  return true;
}

bool 
Config::read()
{
  // First, initialize all configs to default values

  for (auto entry : cfg) {
    if (entry.type == STRING) {
      strlcpy((char *) entry.value, (char *) entry.default_value, entry.max_size);
    }
    else {
      *((int32_t *) entry.value) = *((int32_t *) entry.default_value);
    }
  }

  if ((f = fopen(CONFIG_FILE, "r")) == nullptr) {
    return save(true);
  }

  char buff[128];
  char * caption;
  char * value;

  while (parse_line(buff, 128, &caption, &value)) {
    LOG_D("Caption: %s, value: %s", caption, value);
    for (auto entry : cfg) {
      if (strcmp(caption, entry.caption) == 0) {
        if (entry.type == STRING) {
          strlcpy((char *) entry.value, value, entry.max_size);
        }
        else {
          *((int32_t *) entry.value) = atoi(value);
        }
        break;
      }
    }
  }

  fclose(f);

  return true;
}

bool 
Config::save(bool force)
{
  if (force || modified) {
    if ((f = fopen(CONFIG_FILE, "w")) == nullptr) return false;

    fprintf(f,
      "# EPub-InkPlate Config File\n"
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
      "#\n"
    );

    for (auto entry : cfg) {
      fprintf(f, "#      %s\n", entry.caption);
    }
    
    fprintf(f, "# ---\n\n");

    for (auto entry : cfg) {
      if (entry.type == STRING) {
        fprintf(f, "%s = \"%s\"\n", entry.caption, (char *) entry.value);
      }
      else {
        fprintf(f, "%s = %d\n", entry.caption, *(int32_t *) entry.value);
      }
    }
    fclose(f);
    modified = false;
  }
  return true;
}

#if DEBUGGING
void
Config::show()
{
  LOG_D("Configuration");
  LOG_D("-------------");
  for (auto entry : cfg) {
    if (entry.type == STRING) {
      LOG_D("%s = \"%s\"", entry.caption, (char *) entry.value);
    }
    else {
      LOG_D("%s = %d", entry.caption, *(int32_t *) entry.value);
    }
  }
  LOG_D("---");
}
#endif
