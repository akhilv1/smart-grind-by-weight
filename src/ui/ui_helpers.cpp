#include "ui_helpers.h"
#include "arduino_compat.h"
#include <cstdio>
#include <cstdlib>

void style_as_button(lv_obj_t* object, int32_t width, int32_t height, const lv_font_t* font) {
    lv_obj_set_style_radius(object, THEME_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(THEME_COLOR_SURFACE), 0);
    lv_obj_set_style_text_color(object, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_hor(object, 20, 0);
    if (width >= 0){
        lv_obj_set_style_width(object, width, 0);
    }
    if (height >= 0){
        lv_obj_set_style_height(object, height, 0);
    }
    
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, lv_color_t bg_color, int32_t width, int32_t height, const lv_font_t* font){ 
    lv_obj_t* button = lv_btn_create(parent);
    style_as_button(button, width, height, font);
    lv_obj_set_style_bg_color(button, bg_color, 0);
    
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return button;  
}

void set_label_text_int(lv_obj_t* label, int32_t value, const char* unit) {
    if (!label) return;
    char buf[24];

    if (unit) {
        snprintf(buf, sizeof(buf), "%ld %s", value, unit);
    } else {
        snprintf(buf, sizeof(buf), "%ld", value);
    }

    lv_label_set_text(label, buf);
}

void set_label_text_float(lv_obj_t* label, float value, const char* unit) {
    if (!label) return;
    char buf[24];
    
    if (unit) {
        snprintf(buf, sizeof(buf), "%.2fg %s", value, unit);
    } else {
        snprintf(buf, sizeof(buf), "%.2f", value);
    }

    lv_label_set_text(label, buf);
}

lv_obj_t* create_profile_label(lv_obj_t* parent, lv_obj_t** profile_label, lv_obj_t** weight_label){
    lv_obj_t* label_container = lv_obj_create(parent);
    lv_obj_set_size(label_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(label_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label_container, 0, 0);
    lv_obj_set_style_pad_all(label_container, 0, 0);
    
    // Set up button container as horizontal flex
    lv_obj_set_layout(label_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(label_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(label_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(label_container, 0, 0);

    *profile_label = lv_label_create(label_container);
    lv_label_set_text(*profile_label, "DOUBLE");
    lv_obj_set_style_text_font(*profile_label, THEME_FONT_DISPLAY_NAME, 0);
    lv_obj_set_style_text_color(*profile_label, lv_color_hex(THEME_COLOR_SECONDARY), 0);
    
    *weight_label = lv_label_create(label_container);
    lv_label_set_text(*weight_label, "18.0g");
    lv_obj_set_style_text_font(*weight_label, THEME_FONT_DISPLAY_VALUE, 0);
    lv_obj_set_style_text_color(*weight_label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);

    return label_container;
}

lv_obj_t* create_dual_button_row(lv_obj_t* parent, lv_obj_t** left_button, lv_obj_t** right_button, const char* left_name, const char* right_name, lv_color_t left_color, lv_color_t right_color, int height, const lv_font_t* font){
    lv_obj_t *row_container = lv_obj_create(parent);
    lv_obj_set_size(row_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_container, 0, 0);
    lv_obj_set_style_pad_all(row_container, 0, 0);
    
    lv_obj_set_layout(row_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row_container, 10, 0);

    *left_button = create_button(row_container, left_name, left_color, -1, height, font);
    lv_obj_set_flex_grow(*left_button, 1);

    *right_button = create_button(row_container, right_name, right_color, -1, height, font);
    lv_obj_set_flex_grow(*right_button, 1);

    return row_container;
}

lv_obj_t* create_data_label(lv_obj_t* parent, const char* name, lv_obj_t** value_label, bool stacked) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    // Text aligns with the row cards' outer edge (same content width, no side inset)
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_ver(container, 4, 0);
    lv_obj_set_style_margin_all(container, 0, 0);
    lv_obj_set_size(container, THEME_CONTENT_WIDTH_PX, LV_SIZE_CONTENT);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    if (stacked) {
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    } else {
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    }

    lv_obj_t* name_label = lv_label_create(container);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    if (stacked) {
        lv_obj_set_width(name_label, LV_PCT(100));
        lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_LEFT, 0);
    }

    *value_label = lv_label_create(container);
    lv_label_set_text(*value_label, "");
    lv_obj_set_style_text_font(*value_label, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_color(*value_label, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);
    if (stacked) {
        lv_obj_set_width(*value_label, LV_PCT(100));
        lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_margin_top(*value_label, 4, 0);
    } else {
        lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_RIGHT, 0);
    }

    return container;
}

// Radio button group data structure
struct RadioButtonGroupData {
    lv_obj_t** buttons;
    int button_count;
    int selected_index;
    radio_button_callback_t callback;
    void* user_data;
};

static void style_segment(lv_obj_t* segment, bool selected) {
    lv_obj_set_style_bg_opa(segment, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    // Unselected segments preview the selection while pressed
    lv_obj_set_style_bg_opa(segment, selected ? LV_OPA_COVER : LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(segment,
                                lv_color_hex(selected ? THEME_COLOR_TEXT_PRIMARY : THEME_COLOR_TEXT_SECONDARY), 0);
}

static void apply_segment_selection(RadioButtonGroupData* data) {
    for (int i = 0; i < data->button_count; i++) {
        if (data->buttons[i] && lv_obj_is_valid(data->buttons[i])) {
            style_segment(data->buttons[i], i == data->selected_index);
        }
    }
}

// Internal event handler for segment clicks
static void radio_button_event_handler(lv_event_t* e) {
    lv_obj_t* clicked_button = (lv_obj_t*)lv_event_get_current_target(e);
    if (!clicked_button || !lv_obj_is_valid(clicked_button)) return;

    lv_obj_t* group = lv_obj_get_parent(clicked_button);
    if (!group || !lv_obj_is_valid(group)) return;

    RadioButtonGroupData* data = (RadioButtonGroupData*)lv_obj_get_user_data(group);
    if (!data || !data->buttons) return;

    int clicked_index = -1;
    for (int i = 0; i < data->button_count; i++) {
        if (data->buttons[i] == clicked_button) {
            clicked_index = i;
            break;
        }
    }
    if (clicked_index == -1 || clicked_index == data->selected_index) return;

    data->selected_index = clicked_index;
    apply_segment_selection(data);

    if (data->callback) {
        data->callback(clicked_index, data->user_data);
    }
}

// Event handler to free memory on object deletion
static void radio_button_group_delete_handler(lv_event_t* e) {
    lv_obj_t* group = (lv_obj_t*)lv_event_get_target(e);
    if (!group) {
        return;
    }
    RadioButtonGroupData* data = (RadioButtonGroupData*)lv_obj_get_user_data(group);
    if (!data) {
        return;
    }
    // Clear user data first to prevent a double free if this handler runs again
    lv_obj_set_user_data(group, nullptr);
    if (data->buttons) {
        free(data->buttons);
        data->buttons = nullptr;
    }
    free(data);
}

lv_obj_t* create_radio_button_group(
    lv_obj_t* parent,
    const char* options[],
    int option_count,
    int initial_selection,
    radio_button_callback_t callback,
    void* user_data) {

    constexpr int32_t kTrackPad = THEME_SEGMENT_TRACK_PAD_PX;  // Inset of a segment inside the track

    // Track: the same card as every other row (same radius, so the page keeps one shape)
    lv_obj_t* group_container = lv_obj_create(parent);
    lv_obj_remove_style_all(group_container);
    lv_obj_set_size(group_container, THEME_CONTENT_WIDTH_PX, THEME_ROW_HEIGHT_PX);
    lv_obj_set_style_radius(group_container, THEME_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_bg_color(group_container, lv_color_hex(THEME_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(group_container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(group_container, kTrackPad, 0);
    lv_obj_set_style_pad_column(group_container, kTrackPad, 0);
    lv_obj_set_style_margin_bottom(group_container, THEME_ROW_GAP_PX, 0);
    lv_obj_clear_flag(group_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(group_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    RadioButtonGroupData* data = (RadioButtonGroupData*)malloc(sizeof(RadioButtonGroupData));
    data->buttons = (lv_obj_t**)malloc(sizeof(lv_obj_t*) * option_count);
    data->button_count = option_count;
    data->selected_index = initial_selection;
    data->callback = callback;
    data->user_data = user_data;

    for (int i = 0; i < option_count; i++) {
        lv_obj_t* segment = lv_obj_create(group_container);
        lv_obj_remove_style_all(segment);
        lv_obj_set_height(segment, LV_PCT(100));
        lv_obj_set_flex_grow(segment, 1);
        lv_obj_set_style_radius(segment, THEME_CORNER_RADIUS_PX - kTrackPad, 0);
        lv_obj_set_style_bg_color(segment, lv_color_hex(THEME_COLOR_SELECTED), 0);
        lv_obj_set_style_text_font(segment, THEME_FONT_ROW, 0);
        lv_obj_clear_flag(segment, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(segment, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* label = lv_label_create(segment);
        lv_label_set_text(label, options[i]);
        lv_obj_center(label);

        data->buttons[i] = segment;
        lv_obj_add_event_cb(segment, radio_button_event_handler, LV_EVENT_CLICKED, nullptr);
    }
    apply_segment_selection(data);

    lv_obj_set_user_data(group_container, data);
    lv_obj_add_event_cb(group_container, radio_button_group_delete_handler, LV_EVENT_DELETE, nullptr);

    return group_container;
}

void radio_button_group_set_selection(lv_obj_t* group, int selected_index) {
    if (!group) return;
    RadioButtonGroupData* data = (RadioButtonGroupData*)lv_obj_get_user_data(group);
    if (!data || !data->buttons || selected_index < 0 || selected_index >= data->button_count) return;

    data->selected_index = selected_index;
    apply_segment_selection(data);
}

int radio_button_group_get_selection(lv_obj_t* group) {
    if (!group) return -1;
    RadioButtonGroupData* data = (RadioButtonGroupData*)lv_obj_get_user_data(group);
    return (data && data->buttons) ? data->selected_index : -1;
}

void layout_below_menubar(lv_obj_t* screen) {
    if (!screen) return;
    // Reserve the top strip for the global menubar. Set (not add) pad_top so the inset
    // is exactly the bar height regardless of the screen's existing top padding.
    lv_obj_set_style_pad_top(screen, UI_MENUBAR_HEIGHT_PX, 0);
}
