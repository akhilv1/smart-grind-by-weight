#pragma once
// The simulator uses the firmware's own LVGL configuration, then overrides the
// few settings that only make sense on the ESP32 (include/lv_conf.h is left
// untouched, so the firmware build never sees these).
#include "../include/lv_conf.h"

// The firmware routes LVGL's heap to PSRAM through a custom allocator
// (src/hardware/lv_mem_core_psram.cpp); in the browser plain malloc is right.
#undef LV_USE_STDLIB_MALLOC
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

// Display and mouse come from LVGL's SDL driver, which Emscripten maps onto the
// page's <canvas>.
#undef LV_USE_SDL
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
// Software rendering: SDL's accelerated renderer logs a vsync error under
// Emscripten, and both render pixel-identically.
#define LV_SDL_ACCELERATED 0

// The firmware's assert handler spins forever, which in a browser freezes the
// tab. Log the failing assert with a C stack trace and stop instead.
#undef LV_ASSERT_HANDLER_INCLUDE
#define LV_ASSERT_HANDLER_INCLUDE "sim_assert.h"
#undef LV_ASSERT_HANDLER
#define LV_ASSERT_HANDLER sim_lvgl_assert_failed();

// Same RGB565 depth as the panel, but without the byte swap the firmware's SPI
// display path expects: SDL wants native-order RGB565, and a swapped buffer shows
// blue as green and fringes antialiased text.
#undef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 0
