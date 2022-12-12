#pragma once

#include "common/app_platform.h"

// #define DEBUG_UI
#define EXIT_APP    0
#define POWEROFF    1
#define REBOOT      2

#if FOX_V7
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   376
#define SCREEN_INFO     "240x376-32bpp"
#elif FOX_V8
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   432
#define SCREEN_INFO     "240x432-32bpp"
#elif FOX_V9
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   320
#define SCREEN_INFO     "240x320-32bpp"
#elif FOX_V10
#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   640
#define SCREEN_INFO     "480x640-32bpp"
#elif FOX_V11
#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   854
#define SCREEN_INFO     "480x854-32bpp"
#elif FOX_C26A
#define SCREEN_WIDTH    480
#define SCREEN_HEIGHT   640
#define SCREEN_INFO     "480x640-32bpp"
#else
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   376
#define SCREEN_INFO     "240x376-32bpp"
#endif
