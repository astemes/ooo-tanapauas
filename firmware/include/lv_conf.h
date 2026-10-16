// LVGL 8.3 configuration for the GIGA Display Shield. Selected via
// LV_CONF_PATH in platformio.ini so it wins over the copy bundled with the
// core's Arduino_H7_Video library (which disables the larger fonts). Anything not set here
// falls back to LVGL's defaults (lv_conf_internal.h).
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Arduino_H7_Video renders RGB565.
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// Use the system heap instead of LVGL's fixed 48 KB pool.
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

// Same tick source as the core's bundled LVGL 8 config.
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#define LV_USE_CANVAS 1
#define LV_USE_LOG 0

#endif  // LV_CONF_H
#endif
