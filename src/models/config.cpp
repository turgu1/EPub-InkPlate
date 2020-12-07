#define __CONFIG__ 1
#include "models/config.hpp"

#include "strlcpy.hpp"
#include "logging.hpp"

static int8_t   version;
static char     ssid[32];
static char     pwd[32];
static int32_t  port;
static int8_t   battery;
static int8_t   orientation;
static int8_t   timeout;
static int8_t   font_size;
static int8_t   use_fonts_in_books;
static int8_t   default_font;
static int8_t   show_images;
static int8_t   resolution;

static int32_t  default_port               = 80;
static int8_t   default_battery            =  2;  // 0 = NONE, 1 = PERCENT, 2 = VOLTAGE, 3 = ICON
static int8_t   default_orientation        =  1;  // 0 = LEFT, 1 = RIGHT, 2 = BOTTOM
static int8_t   default_font_size          = 12;  // 8, 10, 12, 15 pts
static int8_t   default_timeout            = 15;  // 5, 15, 30 minutes
static int8_t   default_show_images        =  0;  // 0 = NO, 1 = YES
static int8_t   default_use_fonts_in_books =  1;  // 0 = NO, 1 = YES
static int8_t   default_default_font       =  1;  // 0 = CALADEA, 1 = CRIMSON, 2 = RED HAT, 3 = ASAP
static int8_t   default_resolution         =  0;  // 0 = 1bit, 1 = 3bits
static int8_t   the_version                =  1;

std::array<Config::ConfigDescr, 12> Config::cfg = {{
 { VERSION,            BYTE,   "version",            &version,            &the_version,                0 },
 { SSID,               STRING, "wifi_ssid",          ssid,                "NONE",                     32 },
 { PWD,                STRING, "wifi_pwd",           pwd,                 "NONE",                     32 },
 { PORT,               INT,    "http_port",          &port,               &default_port,               0 },
 { BATTERY,            BYTE,   "battery",            &battery,            &default_battery,            0 },
 { TIMEOUT,            BYTE,   "timeout",            &timeout,            &default_timeout,            0 },
 { FONT_SIZE,          BYTE,   "font_size",          &font_size,          &default_font_size,          0 },
 { DEFAULT_FONT,       BYTE,   "default_font",       &default_font,       &default_default_font,       0 },
 { USE_FONTS_IN_BOOKS, BYTE,   "use_fonts_in_books", &use_fonts_in_books, &default_use_fonts_in_books, 0 },
 { SHOW_IMAGES,        BYTE,   "show_images",        &show_images,        &default_show_images,        0 },
 { ORIENTATION,        BYTE,   "orientation",        &orientation,        &default_orientation,        0 },
 { PIXEL_RESOLUTION,   BYTE,   "resolution",         &resolution,         &default_resolution,         0 }
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
Config::get(Ident id, int8_t * val)
{
  for (auto entry : cfg) {
    if((entry.ident == id) && (entry.type == BYTE)) {
      *val = * ((int8_t *) entry.value);
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
Config::put(Ident id, int8_t val)
{
  for (auto entry : cfg) {
    if ((entry.ident == id) && (entry.type == BYTE)) {
      *((int8_t *) entry.value) = val;
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
    else if (entry.type == INT) {
      *((int32_t *) entry.value) = *((int32_t *) entry.default_value);
    }
    else {
      *((int8_t *) entry.value) = *((int8_t *) entry.default_value);
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
        else if (entry.type == INT) {
          *((int32_t *) entry.value) = atoi(value);
        }
        else  {
          *((int8_t *) entry.value) = atoi(value);
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
      else if (entry.type == INT) {
        fprintf(f, "%s = %d\n", entry.caption, *(int32_t *) entry.value);
      }
      else {
        fprintf(f, "%s = %d\n", entry.caption, *(int8_t *) entry.value);
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
    else if (entry.type == INT) {
      LOG_D("%s = %d", entry.caption, *(int32_t *) entry.value);
    }
    else {
      LOG_D("%s = %d", entry.caption, *(int8_t *) entry.value);
    }
  }
  LOG_D("---");
}
#endif
