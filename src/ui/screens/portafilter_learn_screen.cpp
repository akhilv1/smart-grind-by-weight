#include "portafilter_learn_screen.h"

#include <cstdio>
#include "../event_bridge_lvgl.h"
#include "../ui_helpers.h"

static uint32_t separation_color(PortafilterSeparation separation) {
    switch (separation) {
        case PortafilterSeparation::CONFLICT: return THEME_COLOR_ERROR;
        case PortafilterSeparation::CLOSE:    return THEME_COLOR_WARNING;
        case PortafilterSeparation::CLEAR:
        default:                              return THEME_COLOR_TEXT_PRIMARY;
    }
}

void PortafilterLearnScreen::create() {
    screen_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen_, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_style_pad_all(screen_, 0, 0);
    lv_obj_set_style_pad_bottom(screen_, 8, 0);
    layout_below_menubar(screen_);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(screen_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen_, 4, 0);

    status_label_ = lv_label_create(screen_);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_label_, LV_PCT(92));
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(THEME_COLOR_ACCENT), 0);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(status_label_, 6, 0);

    weight_label_ = lv_label_create(screen_);
    lv_obj_set_style_text_font(weight_label_, &lv_font_montserrat_56, 0);
    lv_obj_set_style_text_color(weight_label_, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);

    match_label_ = lv_label_create(screen_);
    lv_label_set_long_mode(match_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(match_label_, LV_PCT(92));
    lv_obj_set_style_text_font(match_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(match_label_, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(match_label_, LV_TEXT_ALIGN_CENTER, 0);

    // Fixed-height row so the list below doesn't jump when the buttons appear
    lv_obj_t* button_row = lv_obj_create(screen_);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_size(button_row, LV_SIZE_CONTENT, 80);
    lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_row, 20, 0);

    using ET = EventBridgeLVGL::EventType;
    const char* names[2] = {"SINGLE", "DOUBLE"};
    const ET events[2] = {ET::LEARN_LABEL_SINGLE, ET::LEARN_LABEL_DOUBLE};
    for (int i = 0; i < 2; i++) {
        label_buttons_[i] = create_button(button_row, names[i], lv_color_hex(THEME_COLOR_PRIMARY), 120, 80,
                                          &lv_font_montserrat_24);
        lv_obj_set_style_radius(label_buttons_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(label_buttons_[i], lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
        lv_obj_add_event_cb(label_buttons_[i], EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(events[i])));
        lv_obj_add_flag(label_buttons_[i], LV_OBJ_FLAG_HIDDEN);
    }

    summary_label_ = lv_label_create(screen_);
    lv_label_set_long_mode(summary_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(summary_label_, LV_PCT(92));
    lv_obj_set_style_text_font(summary_label_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(summary_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(summary_label_, 6, 0);

    // Scrollable list of learned setups fills the rest of the screen
    setup_list_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(setup_list_);
    lv_obj_set_width(setup_list_, LV_PCT(92));
    lv_obj_set_flex_grow(setup_list_, 1);
    lv_obj_set_layout(setup_list_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(setup_list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(setup_list_, 2, 0);
    lv_obj_set_scroll_dir(setup_list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(setup_list_, LV_SCROLLBAR_MODE_AUTO);

    visible_ = false;
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
}

void PortafilterLearnScreen::show() {
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_HIDDEN);
    visible_ = true;
}

void PortafilterLearnScreen::hide() {
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
    visible_ = false;
}

void PortafilterLearnScreen::set_status(const char* status_text) {
    lv_label_set_text(status_label_, status_text ? status_text : "");
}

void PortafilterLearnScreen::set_measurement(const char* weight_text, const char* match_text) {
    lv_label_set_text(weight_label_, weight_text ? weight_text : "");
    lv_label_set_text(match_label_, match_text ? match_text : "");
}

void PortafilterLearnScreen::set_label_buttons(bool ask, int suggested_shot) {
    for (int i = 0; i < 2; i++) {
        if (!label_buttons_[i]) {
            continue;
        }
        if (!ask) {
            lv_obj_add_flag(label_buttons_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(label_buttons_[i], LV_OBJ_FLAG_HIDDEN);
        // White ring on the label the detector suggests
        lv_obj_set_style_border_width(label_buttons_[i], (i == suggested_shot) ? 4 : 0, 0);
    }
}

void PortafilterLearnScreen::update_setup_list(const PortafilterDetector& detector) {
    lv_obj_clean(setup_list_);

    const int count = detector.cluster_count();
    if (count == 0) {
        lv_label_set_text(summary_label_, "No setups learned yet");
        lv_obj_set_style_text_color(summary_label_, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);
        return;
    }

    // Sort by weight so neighbors (the pairs most likely to be confused) sit together
    int order[USER_PF_MAX_CLUSTERS];
    for (int i = 0; i < count; ++i) {
        order[i] = i;
    }
    for (int i = 1; i < count; ++i) {
        const int key = order[i];
        int j = i - 1;
        while (j >= 0 && detector.cluster(order[j]).mean_g > detector.cluster(key).mean_g) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    int conflicts = 0;
    int close = 0;
    for (int n = 0; n < count; ++n) {
        const int index = order[n];
        const PortafilterCluster& cluster = detector.cluster(index);
        const PortafilterSeparation separation = detector.separation(index);
        if (separation == PortafilterSeparation::CONFLICT) conflicts++;
        if (separation == PortafilterSeparation::CLOSE) close++;

        lv_obj_t* row = lv_obj_create(setup_list_);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(row, 2, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

        // snprintf, not lv_label_set_text_fmt: LVGL's printf is built without float support
        char text[32];
        const lv_color_t color = lv_color_hex(separation_color(separation));
        lv_obj_t* name_label = lv_label_create(row);
        snprintf(text, sizeof(text), "%s %.1fg %ux",
                 cluster.shot_type == static_cast<uint8_t>(ShotType::DOUBLE) ? "Dbl" : "Sgl",
                 static_cast<double>(cluster.mean_g), static_cast<unsigned>(cluster.count));
        lv_label_set_text(name_label, text);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(name_label, color, 0);

        // Delete button; the setup index rides in the button's user data
        lv_obj_t* delete_button = lv_btn_create(row);
        lv_obj_set_size(delete_button, 48, 40);
        lv_obj_set_style_radius(delete_button, 8, 0);
        lv_obj_set_style_shadow_width(delete_button, 0, 0);
        lv_obj_set_style_bg_color(delete_button, lv_color_hex(0x333333), 0);
        lv_obj_set_ext_click_area(delete_button, 6);
        lv_obj_set_user_data(delete_button, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
        lv_obj_t* trash = lv_label_create(delete_button);
        lv_label_set_text(trash, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(trash, lv_color_hex(THEME_COLOR_ERROR), 0);
        lv_obj_center(trash);
        lv_obj_add_event_cb(delete_button, EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(EventBridgeLVGL::EventType::LEARN_FORGET)));
    }

    // Each problem pair is counted from both of its setups
    if (conflicts > 0) {
        lv_label_set_text_fmt(summary_label_, "%d setups too close to tell apart", conflicts);
        lv_obj_set_style_text_color(summary_label_, lv_color_hex(THEME_COLOR_ERROR), 0);
    } else if (close > 0) {
        lv_label_set_text_fmt(summary_label_, "%d setups close; may ask sometimes", close);
        lv_obj_set_style_text_color(summary_label_, lv_color_hex(THEME_COLOR_WARNING), 0);
    } else {
        lv_label_set_text_fmt(summary_label_, "%d setups, all clearly separated", count);
        lv_obj_set_style_text_color(summary_label_, lv_color_hex(THEME_COLOR_SUCCESS), 0);
    }
}
