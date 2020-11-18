// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#ifndef LOG_LOCAL_LEVEL
  #if DEBUGGING
    #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
  #else
    #define LOG_LOCAL_LEVEL ESP_LOG_ERROR
  #endif
#endif

#include "esp_log.h"

#define LOG_E(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)