#pragma once
// LVGL assert hook for the browser build (see lv_conf.h). Plain C so it can be
// included from LVGL's C sources and its extern "C" headers alike.
#ifdef __cplusplus
extern "C" {
#endif
void sim_lvgl_assert_failed(void);
#ifdef __cplusplus
}
#endif
