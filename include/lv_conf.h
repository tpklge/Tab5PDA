#ifndef TAB5_PDA_LV_CONF_H
#define TAB5_PDA_LV_CONF_H
#define LV_CONF_H
#include <stdint.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_USE_LARGE_COORD 1
#ifdef ARDUINO
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "pda_memory.h"
#define LV_MEM_CUSTOM_ALLOC tab_lv_malloc
#define LV_MEM_CUSTOM_FREE tab_lv_free
#define LV_MEM_CUSTOM_REALLOC tab_lv_realloc
#else
#define LV_MEM_CUSTOM 0
#endif
#define LV_MEM_SIZE (512U * 1024U)
#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 10
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 160
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_FLEX 1
#define LV_USE_GRID 0
#define LV_USE_KEYBOARD 1
#define LV_USE_CALENDAR 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0
#endif
