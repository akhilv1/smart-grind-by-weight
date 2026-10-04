#pragma once
#include <lvgl.h>
#include "../config/constants.h"

// Function declarations
void style_as_button(lv_obj_t* object, int32_t width = THEME_CONTENT_WIDTH_PX,
                     int32_t height = THEME_ROW_HEIGHT_PX, const lv_font_t* font = THEME_FONT_ROW);

lv_obj_t* create_button(lv_obj_t* parent, const char* text,
                       lv_color_t bg_color = lv_color_hex(THEME_COLOR_SURFACE),
                       int32_t width = THEME_CONTENT_WIDTH_PX, int32_t height = THEME_ROW_HEIGHT_PX,
                       const lv_font_t* font = THEME_FONT_ROW);

void set_label_text_int(lv_obj_t* label, int32_t value, const char* unit = nullptr);

void set_label_text_float(lv_obj_t* label, float value, const char* unit = nullptr);

lv_obj_t* create_profile_label(lv_obj_t* parent, lv_obj_t** profile_label, lv_obj_t** weight_label);

lv_obj_t* create_dual_button_row(lv_obj_t* parent, lv_obj_t** left_button, lv_obj_t** right_button, 
                                const char* left_name, const char* right_name, 
                                lv_color_t left_color = lv_color_hex(THEME_COLOR_SURFACE),
                                lv_color_t right_color = lv_color_hex(THEME_COLOR_SURFACE),
                                int height = 80, const lv_font_t* font = THEME_FONT_ROW);

lv_obj_t* create_data_label(lv_obj_t* parent, const char* name, lv_obj_t** value_label, bool stacked = false);

// Callback signature for radio button selection changes
typedef void (*radio_button_callback_t)(int selected_index, void* user_data);

// Single-choice group drawn as a segmented control: one row-height card split into
// equal segments, the selected one filled with THEME_COLOR_SELECTED. Sized for 2-3
// short labels across the Settings content width.
lv_obj_t* create_radio_button_group(
    lv_obj_t* parent,
    const char* options[],           // Segment labels
    int option_count,                // Number of options
    int initial_selection,           // Initially selected index (0-based)
    radio_button_callback_t callback, // Called when selection changes
    void* user_data                 // Passed to callback
);

// Radio button group utility functions
void radio_button_group_set_selection(lv_obj_t* group, int selected_index);
int radio_button_group_get_selection(lv_obj_t* group);

// Inset a screen root's content below the global top menubar by adding a top pad of
// UI_MENUBAR_HEIGHT_PX. LVGL aligns TOP-anchored and flex children inside the content
// area, so this shifts everything down clear of the bar while leaving BOTTOM-anchored
// rows in place. Call once during a screen's root setup (non-immersive screens only).
void layout_below_menubar(lv_obj_t* screen);
