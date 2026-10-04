#include "ready_screen.h"
#include <cmath>
#include "arduino_compat.h"
#include "../../config/constants.h"
#include "../../controllers/grind_mode_traits.h"
#include "../event_bridge_lvgl.h"
#include "../ui_helpers.h"

// Circular action button matching the main grind/pulse buttons (defined below)
static lv_obj_t* create_round_button(lv_obj_t* parent, const char* text, uint32_t color_hex);

// Bottom-anchor an action button inside a flex-column tab page: a growing spacer
// pushes it down so it slides with the page content during swipes.
static lv_obj_t* add_tab_action_button(lv_obj_t* parent, const char* symbol, uint32_t color_hex,
                                       EventBridgeLVGL::EventType event) {
    lv_obj_t* spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, LV_PCT(100));
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t* btn = create_round_button(parent, symbol, color_hex);
    lv_obj_add_event_cb(btn, EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(event)));
    return btn;
}

void ReadyScreen::create() {
    screen = lv_obj_create(lv_scr_act());
    // Full height (below the nav bar) so in-tab action buttons sit near the bottom of
    // the panel; a bottom pad keeps them clear of the page-indicator dots.
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
    lv_obj_align(screen, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_pad_bottom(screen, 22, 0);
    layout_below_menubar(screen);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Create tabview
    tabview = lv_tabview_create(screen);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_align(tabview, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(tabview, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_add_flag(tabview, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Hide tab buttons for swipe-only interface
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_add_flag(tab_btns, LV_OBJ_FLAG_HIDDEN);

    // Transparent background
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, 0);

    // AUTO first: it detects the portafilter and grinds the matching profile
    auto_tab = lv_tabview_add_tab(tabview, "Auto");

    // Add profile tabs
    profile_tabs[0] = lv_tabview_add_tab(tabview, "Single");
    profile_tabs[1] = lv_tabview_add_tab(tabview, "Double");
    profile_tabs[2] = lv_tabview_add_tab(tabview, "Custom");
    menu_tab = lv_tabview_add_tab(tabview, "MENU");
    profile_tabs[3] = menu_tab;
    scale_tab = lv_tabview_add_tab(tabview, "Scale");

    // Default weights
    float default_weights[3] = {USER_SINGLE_ESPRESSO_WEIGHT_G, USER_DOUBLE_ESPRESSO_WEIGHT_G, USER_CUSTOM_PROFILE_WEIGHT_G};
    const char* names[3] = {"SINGLE", "DOUBLE", "CUSTOM"};
    
    for (int i = 0; i < 3; i++) {
        create_profile_page(profile_tabs[i], i, names[i], default_weights[i]);
    }

    create_auto_page(auto_tab);

    // Create menu tab page
    create_menu_page(menu_tab);

    // Create scale tab page (live weight + manual grind)
    create_scale_page(scale_tab);

    // iOS-style page-indicator dots across the bottom
    create_page_dots();

    update_profile_values(default_weights, GrindMode::WEIGHT);

    visible = false;
}

void ReadyScreen::create_profile_page(lv_obj_t* parent, int profile_index, const char* profile_name, float weight) {
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 0, 0);
    lv_obj_set_style_pad_bottom(parent, 16, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Top spacer mirrors the bottom one so the labels stay centered above the button
    lv_obj_t* top_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(top_spacer);
    lv_obj_set_width(top_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(top_spacer, 1);

    lv_obj_t* name_label;
    (void)create_profile_label(parent, &name_label, &weight_labels[profile_index]);
    lv_label_set_text(name_label, profile_name);
    lv_obj_add_flag(name_label, LV_OBJ_FLAG_CLICKABLE);

    char weight_text[16];
    snprintf(weight_text, sizeof(weight_text), SYS_WEIGHT_DISPLAY_FORMAT, weight);
    lv_label_set_text(weight_labels[profile_index], weight_text);
    lv_obj_add_flag(weight_labels[profile_index], LV_OBJ_FLAG_CLICKABLE);

    // In-tab grind button — slides with the page instead of floating over it
    profile_action_buttons[profile_index] =
        add_tab_action_button(parent, LV_SYMBOL_PLAY, THEME_COLOR_PRIMARY,
                              EventBridgeLVGL::EventType::GRIND_BUTTON);
}

void ReadyScreen::create_auto_page(lv_obj_t* parent) {
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 0, 0);
    lv_obj_set_style_pad_bottom(parent, 16, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Same spacer / label / bottom-row structure as the profile pages so AUTO lines up with them
    lv_obj_t* top_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(top_spacer);
    lv_obj_set_width(top_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(top_spacer, 1);

    (void)create_profile_label(parent, &auto_name_label, &auto_value_label);
    lv_label_set_text(auto_name_label, "AUTO");
    lv_label_set_text(auto_value_label, "--");

    // Long-press anywhere on the page (not its buttons) opens Learn Portafilters
    lv_obj_add_event_cb(parent, EventBridgeLVGL::dispatch_event, LV_EVENT_LONG_PRESSED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(EventBridgeLVGL::EventType::AUTO_LONG_PRESS)));

    auto_status_label = lv_label_create(parent);
    lv_label_set_long_mode(auto_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(auto_status_label, LV_PCT(90));
    lv_obj_set_style_text_font(auto_status_label, THEME_FONT_STATUS, 0);
    lv_obj_set_style_text_color(auto_status_label, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(auto_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(auto_status_label, 6, 0);
    lv_label_set_text(auto_status_label, "Place portafilter");

    lv_obj_t* bottom_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(bottom_spacer);
    lv_obj_set_width(bottom_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(bottom_spacer, 1);

    // Fixed-height row so the labels don't jump when the label buttons appear
    lv_obj_t* button_row = lv_obj_create(parent);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_size(button_row, LV_SIZE_CONTENT, 100);
    lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_row, 20, 0);

    using ET = EventBridgeLVGL::EventType;
    const char* names[2] = {"SINGLE", "DOUBLE"};
    const ET events[2] = {ET::AUTO_LABEL_SINGLE, ET::AUTO_LABEL_DOUBLE};
    for (int i = 0; i < 2; i++) {
        // Pill instead of a circle so the 24pt label fits
        auto_label_buttons[i] = create_round_button(button_row, names[i], THEME_COLOR_PRIMARY);
        lv_obj_set_size(auto_label_buttons[i], 120, 80);
        lv_obj_add_event_cb(auto_label_buttons[i], EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(events[i])));
        lv_obj_add_flag(auto_label_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }

    create_auto_logo(button_row);

    auto_swap_button = create_round_button(button_row, "", THEME_COLOR_NEUTRAL);
    lv_obj_set_size(auto_swap_button, 120, 60);
    auto_swap_label = lv_obj_get_child(auto_swap_button, 0);
    lv_obj_add_event_cb(auto_swap_button, EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(ET::AUTO_SWAP_GUESS)));
    lv_obj_add_flag(auto_swap_button, LV_OBJ_FLAG_HIDDEN);
}

// ----------------------------------------------------------------------------
// AUTO mark
// ----------------------------------------------------------------------------
// Two canvas layers in a 100x100 slot (same footprint as the other tabs' action
// buttons): a ring layer that shows two clockwise arrows (searching) or a closed
// circle (matched), and a glyph layer. While searching the glyph is an "A" stroked
// at the ring's weight; once matched it becomes one or two shot dots above a play
// symbol, so the slot reads as the same start control as the profile tabs. Color
// carries state: blue at rest and when matched, yellow while a placement is being
// read. Canvas buffers come from the LVGL heap (PSRAM).
static void set_visible(lv_obj_t* obj, bool visible) {
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static constexpr int32_t kAutoLogoSize = 100;
static constexpr float kAutoLogoCenter = kAutoLogoSize / 2.0f;
static constexpr float kRingRadius = 41.0f;     // Ring centerline
static constexpr int32_t kRingStroke = 7;
static constexpr int32_t kGlyphStroke = 8;

static lv_point_precise_t point_on_circle(float cx, float cy, float angle_deg, float radius) {
    // LVGL angle convention: 0 deg at 3 o'clock, increasing clockwise (screen y is down)
    const float rad = angle_deg * 3.14159265f / 180.0f;
    lv_point_precise_t point;
    point.x = static_cast<lv_value_precise_t>(cx + radius * cosf(rad));
    point.y = static_cast<lv_value_precise_t>(cy + radius * sinf(rad));
    return point;
}

// Arc along a centerline radius (lv_draw_arc's radius is the outer edge)
static void draw_stroke_arc(lv_layer_t* layer, uint32_t color, float cx, float cy, float radius,
                            int32_t stroke, float start_deg, float end_deg) {
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = lv_color_hex(color);
    arc.width = stroke;
    arc.center.x = static_cast<int32_t>(cx);
    arc.center.y = static_cast<int32_t>(cy);
    arc.radius = static_cast<uint16_t>(radius + stroke / 2.0f);
    arc.start_angle = start_deg;
    arc.end_angle = end_deg;
    arc.rounded = 1;
    lv_draw_arc(layer, &arc);
}

static void draw_stroke_line(lv_layer_t* layer, uint32_t color, float x1, float y1, float x2, float y2) {
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(color);
    line.width = kGlyphStroke;
    line.round_start = 1;
    line.round_end = 1;
    line.p1.x = x1;
    line.p1.y = y1;
    line.p2.x = x2;
    line.p2.y = y2;
    lv_draw_line(layer, &line);
}

static void draw_ring_arrow(lv_layer_t* layer, uint32_t color, float start_deg, float end_deg) {
    constexpr float kHeadHalfWidth = 8.0f;   // Radial half-width of the arrowhead base
    constexpr float kHeadLengthDeg = 18.0f;

    draw_stroke_arc(layer, color, kAutoLogoCenter, kAutoLogoCenter, kRingRadius, kRingStroke, start_deg, end_deg);

    // Arrowhead continues the arc clockwise from its end
    lv_draw_triangle_dsc_t head;
    lv_draw_triangle_dsc_init(&head);
    head.color = lv_color_hex(color);
    head.p[0] = point_on_circle(kAutoLogoCenter, kAutoLogoCenter, end_deg, kRingRadius - kHeadHalfWidth);
    head.p[1] = point_on_circle(kAutoLogoCenter, kAutoLogoCenter, end_deg, kRingRadius + kHeadHalfWidth);
    head.p[2] = point_on_circle(kAutoLogoCenter, kAutoLogoCenter, end_deg + kHeadLengthDeg, kRingRadius);
    lv_draw_triangle(layer, &head);
}

static void redraw_ring(lv_obj_t* canvas, uint32_t color, bool closed) {
    lv_canvas_fill_bg(canvas, lv_color_hex(THEME_COLOR_BACKGROUND), LV_OPA_TRANSP);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    if (closed) {
        draw_stroke_arc(&layer, color, kAutoLogoCenter, kAutoLogoCenter, kRingRadius, kRingStroke, 0.0f, 360.0f);
    } else {
        // Two arrows chasing each other clockwise, gaps at 3 and 9 o'clock
        draw_ring_arrow(&layer, color, 195.0f, 330.0f);
        draw_ring_arrow(&layer, color, 15.0f, 150.0f);
    }
    lv_canvas_finish_layer(canvas, &layer);
}

static void draw_dot(lv_layer_t* layer, uint32_t color, float cx, float cy) {
    constexpr float kDotRadius = 4.0f;
    draw_stroke_arc(layer, color, cx, cy, kDotRadius / 2.0f, static_cast<int32_t>(kDotRadius), 0.0f, 360.0f);
}

// shots == 0 draws the "A"; 1 or 2 draws that many shot dots above the play symbol
static void redraw_glyph(lv_obj_t* canvas, uint32_t color, uint8_t shots) {
    lv_canvas_fill_bg(canvas, lv_color_hex(THEME_COLOR_BACKGROUND), LV_OPA_TRANSP);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    if (shots == 0) {
        draw_stroke_line(&layer, color, 50.0f, 27.0f, 33.0f, 71.0f);   // Left leg
        draw_stroke_line(&layer, color, 50.0f, 27.0f, 67.0f, 71.0f);   // Right leg
        draw_stroke_line(&layer, color, 39.5f, 56.0f, 60.5f, 56.0f);   // Crossbar
    } else if (shots == 1) {
        draw_dot(&layer, color, 50.0f, 27.0f);
    } else {
        draw_dot(&layer, color, 44.5f, 27.0f);
        draw_dot(&layer, color, 55.5f, 27.0f);
    }
    lv_canvas_finish_layer(canvas, &layer);
}

static lv_obj_t* create_logo_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, int32_t y_offset) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y_offset);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    return label;
}

static lv_obj_t* create_logo_layer(lv_obj_t* parent) {
    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_draw_buf_t* buffer = lv_draw_buf_create(kAutoLogoSize, kAutoLogoSize, LV_COLOR_FORMAT_ARGB8888, 0);
    if (buffer) {
        lv_canvas_set_draw_buf(canvas, buffer);
    } else {
        lv_obj_set_size(canvas, kAutoLogoSize, kAutoLogoSize);  // Slot stays empty without a buffer
    }
    lv_obj_center(canvas);
    return canvas;
}

void ReadyScreen::create_auto_logo(lv_obj_t* parent) {
    // The container is the START control when matched: its fill is the press target
    auto_logo = lv_obj_create(parent);
    lv_obj_remove_style_all(auto_logo);
    lv_obj_set_size(auto_logo, kAutoLogoSize, kAutoLogoSize);
    lv_obj_set_style_radius(auto_logo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(auto_logo, lv_color_hex(THEME_COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(auto_logo, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(auto_logo, kAutoLogoSize / 2, 0);
    lv_obj_set_style_transform_pivot_y(auto_logo, kAutoLogoSize / 2, 0);
    lv_obj_clear_flag(auto_logo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(auto_logo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(auto_logo, EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(EventBridgeLVGL::EventType::AUTO_START)));

    auto_logo_ring = create_logo_layer(auto_logo);
    auto_logo_glyph = create_logo_layer(auto_logo);

    // Play symbol under the shot dots: the same glyph as the profile tabs' grind button
    auto_logo_play_label = create_logo_label(auto_logo, THEME_FONT_SYMBOL, THEME_COLOR_TEXT_PRIMARY, 8);
    lv_label_set_text(auto_logo_play_label, LV_SYMBOL_PLAY);

    set_auto_logo(AutoPageAction::LOGO, 0);
}

static void logo_spin_anim_cb(void* obj, int32_t value) {
    lv_image_set_rotation(static_cast<lv_obj_t*>(obj), value);
}

static void logo_pop_anim_cb(void* obj, int32_t value) {
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(obj), value, 0);
}

void ReadyScreen::set_auto_logo(AutoPageAction action, uint8_t shots) {
    if (!auto_logo || !auto_logo_ring || !auto_logo_glyph) {
        return;
    }

    const bool matched = (action == AutoPageAction::START || action == AutoPageAction::LOCKED);
    const uint8_t shown_shots = matched ? shots : 0;
    if (action == auto_logo_action && shown_shots == auto_logo_shots) {
        return;
    }
    const AutoPageAction previous = auto_logo_action;
    const bool was_matched = (previous == AutoPageAction::START || previous == AutoPageAction::LOCKED);
    auto_logo_action = action;
    auto_logo_shots = shown_shots;

    const bool detecting = (action == AutoPageAction::DETECTING);
    const uint32_t color = detecting ? THEME_COLOR_DETECTING : THEME_COLOR_ACCENT;
    redraw_ring(auto_logo_ring, color, matched);
    redraw_glyph(auto_logo_glyph, color, shown_shots);
    // Only a tappable ring shows the play symbol; a still-on portafilter gets the bare ring
    set_visible(auto_logo_play_label, action == AutoPageAction::START);

    // Arrows spin only while reading a placement; an idle home screen stays still
    lv_anim_delete(auto_logo_ring, logo_spin_anim_cb);
    lv_image_set_rotation(auto_logo_ring, 0);
    if (detecting) {
        lv_anim_t spin;
        lv_anim_init(&spin);
        lv_anim_set_var(&spin, auto_logo_ring);
        lv_anim_set_exec_cb(&spin, logo_spin_anim_cb);
        lv_anim_set_values(&spin, 0, 3600);
        lv_anim_set_duration(&spin, 1200);
        lv_anim_set_repeat_count(&spin, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&spin);
    }

    // Matched + tappable: a faint fill marks it as the START control, deeper when pressed
    const bool tappable = (action == AutoPageAction::START);
    lv_obj_set_style_bg_opa(auto_logo, tappable ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(auto_logo, tappable ? LV_OPA_50 : LV_OPA_TRANSP, LV_STATE_PRESSED);
    if (tappable) {
        lv_obj_add_flag(auto_logo, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(auto_logo, LV_OBJ_FLAG_CLICKABLE);
    }

    // The ring closing is the moment of recognition: give it a short pop
    lv_anim_delete(auto_logo, logo_pop_anim_cb);
    lv_obj_set_style_transform_scale(auto_logo, LV_SCALE_NONE, 0);
    if (matched && !was_matched) {
        lv_anim_t pop;
        lv_anim_init(&pop);
        lv_anim_set_var(&pop, auto_logo);
        lv_anim_set_exec_cb(&pop, logo_pop_anim_cb);
        lv_anim_set_values(&pop, LV_SCALE_NONE * 82 / 100, LV_SCALE_NONE);
        lv_anim_set_duration(&pop, 220);
        lv_anim_set_path_cb(&pop, lv_anim_path_overshoot);
        lv_anim_start(&pop);
    }
}

void ReadyScreen::update_auto_page(const char* name_text, const char* value_text, const char* status_text,
                                   AutoPageAction action, uint8_t shots, const char* swap_text) {
    if (auto_name_label) {
        lv_label_set_text(auto_name_label, name_text ? name_text : "AUTO");
    }
    if (auto_value_label) {
        lv_label_set_text(auto_value_label, value_text ? value_text : "");
    }
    if (auto_status_label) {
        lv_label_set_text(auto_status_label, status_text ? status_text : "");
    }

    const bool ask = (action == AutoPageAction::ASK_LABEL);
    const bool start = (action == AutoPageAction::START);
    for (lv_obj_t* button : auto_label_buttons) {
        set_visible(button, ask);
    }
    set_visible(auto_swap_button, start);
    if (start) {
        lv_label_set_text(auto_swap_label, swap_text ? swap_text : "");
    }

    set_visible(auto_logo, !ask);
    if (!ask) {
        set_auto_logo(action, shots);
    }
}

void ReadyScreen::create_menu_page(lv_obj_t* parent) {
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 20, 0);
    lv_obj_set_style_pad_bottom(parent, 16, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Top spacer mirrors the bottom one so the label stays centered above the button
    lv_obj_t* top_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(top_spacer);
    lv_obj_set_width(top_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(top_spacer, 1);

    // Info label
    lv_obj_t* info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "MAIN\nMENU");
    lv_obj_set_style_text_font(info_label, THEME_FONT_DISPLAY_NAME, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);

    // In-tab settings button — opens the Settings menu (GRIND_BUTTON handler routes
    // tab 3 to UIState::MENU), sliding with the page like every other tab action
    menu_action_button =
        add_tab_action_button(parent, LV_SYMBOL_SETTINGS, THEME_COLOR_NEUTRAL,
                              EventBridgeLVGL::EventType::GRIND_BUTTON);
}

// Circular action button matching the main grind/pulse buttons
static lv_obj_t* create_round_button(lv_obj_t* parent, const char* text, uint32_t color_hex) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 100, 100);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color_hex), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, THEME_FONT_ROW, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(label);
    return btn;
}

void ReadyScreen::create_scale_page(lv_obj_t* parent) {
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 10, 0);
    lv_obj_set_style_pad_bottom(parent, 16, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Top spacer centres the weight block (mirrors the profile/custom screen)
    lv_obj_t* top_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(top_spacer);
    lv_obj_set_width(top_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(top_spacer, 1);

    lv_obj_t* subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "Live weight");
    lv_obj_set_style_text_font(subtitle, THEME_FONT_STATUS, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(THEME_COLOR_TEXT_SECONDARY), 0);

    scale_weight_label = lv_label_create(parent);
    lv_label_set_text(scale_weight_label, "0.0g");
    lv_obj_set_style_text_font(scale_weight_label, THEME_FONT_DISPLAY_VALUE, 0);
    lv_obj_set_style_text_color(scale_weight_label, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_align(scale_weight_label, LV_TEXT_ALIGN_CENTER, 0);

    // Bottom spacer pins the button row near the bottom of the page
    lv_obj_t* bottom_spacer = lv_obj_create(parent);
    lv_obj_remove_style_all(bottom_spacer);
    lv_obj_set_width(bottom_spacer, LV_PCT(100));
    lv_obj_set_flex_grow(bottom_spacer, 1);

    // Two round buttons side by side: TARE (zero) + GRIND (hold-to-run)
    lv_obj_t* button_row = lv_obj_create(parent);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_size(button_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_row, 20, 0);

    using ET = EventBridgeLVGL::EventType;

    scale_tare_button = create_round_button(button_row, "TARE", THEME_COLOR_NEUTRAL);
    lv_obj_add_event_cb(scale_tare_button, EventBridgeLVGL::dispatch_event, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(ET::SCALE_TARE)));

    scale_grind_button = create_round_button(button_row, "GRIND", THEME_COLOR_PRIMARY);
    // LV_EVENT_ALL: the handler distinguishes PRESSED / RELEASED / PRESS_LOST for hold-to-grind
    lv_obj_add_event_cb(scale_grind_button, EventBridgeLVGL::dispatch_event, LV_EVENT_ALL,
                        reinterpret_cast<void*>(static_cast<intptr_t>(ET::SCALE_GRIND)));
}

void ReadyScreen::create_page_dots() {
    page_dots_container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(page_dots_container);
    lv_obj_set_size(page_dots_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // Lowest element on the screen, mirroring the status indicators at the top
    lv_obj_align(page_dots_container, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_clear_flag(page_dots_container, LV_OBJ_FLAG_SCROLLABLE);
    // Opaque (black) background so a repaint fully clears the strip - prevents
    // stale-pixel artifacts from the dots floating over the animating tabview.
    // Invisible against the black screen background.
    lv_obj_set_style_bg_opa(page_dots_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(page_dots_container, lv_color_hex(THEME_COLOR_BACKGROUND), 0);
    lv_obj_set_style_pad_all(page_dots_container, 4, 0);
    lv_obj_set_layout(page_dots_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page_dots_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(page_dots_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(page_dots_container, 10, 0);

    for (int i = 0; i < kTabCount; i++) {
        lv_obj_t* dot = lv_obj_create(page_dots_container);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(THEME_COLOR_NEUTRAL), 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        page_dots[i] = dot;
    }
    update_page_dots(0);
}

void ReadyScreen::update_page_dots(int active_index) {
    for (int i = 0; i < kTabCount; i++) {
        if (!page_dots[i]) {
            continue;
        }
        bool active = (i == active_index);
        lv_obj_set_style_bg_color(page_dots[i],
                                  lv_color_hex(active ? THEME_COLOR_TEXT_PRIMARY : THEME_COLOR_NEUTRAL), 0);
        lv_obj_set_style_bg_opa(page_dots[i], active ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_invalidate(page_dots[i]);
    }
    // Repaint the whole row (overlay on the active screen) so the edge dots don't
    // leave stale pixels when the tabview animates underneath them.
    if (page_dots_container) {
        lv_obj_invalidate(page_dots_container);
    }
}

void ReadyScreen::update_scale_weight(float weight) {
    if (!scale_weight_label) {
        return;
    }
    char buffer[24];
    snprintf(buffer, sizeof(buffer), SYS_WEIGHT_DISPLAY_FORMAT, weight);
    lv_label_set_text(scale_weight_label, buffer);
}

void ReadyScreen::show() {
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
    // Page dots live on the active screen (overlay), so toggle them with the home screen
    if (page_dots_container) {
        lv_obj_clear_flag(page_dots_container, LV_OBJ_FLAG_HIDDEN);
    }
    visible = true;
}

void ReadyScreen::hide() {
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    if (page_dots_container) {
        lv_obj_add_flag(page_dots_container, LV_OBJ_FLAG_HIDDEN);
    }
    visible = false;
}

void ReadyScreen::update_profile_values(const float values[3], GrindMode mode) {
    // Grind buttons are color-coded by mode, matching the old floater (blue = time)
    lv_color_t action_color = lv_color_hex(mode == GrindMode::TIME ? THEME_COLOR_ACCENT
                                                                   : THEME_COLOR_PRIMARY);
    for (int i = 0; i < 3; i++) {
        if (weight_labels[i]) {
            char text[24];
            format_ready_value(text, sizeof(text), mode, values[i]);
            lv_label_set_text(weight_labels[i], text);
        }
        if (profile_action_buttons[i]) {
            lv_obj_set_style_bg_color(profile_action_buttons[i], action_color, 0);
        }
    }
}

void ReadyScreen::set_active_tab(int tab) {
    if (tab >= 0 && tab < kTabCount) {
        lv_tabview_set_act(tabview, tab, LV_ANIM_OFF);
        update_page_dots(tab);
    }
}

void ReadyScreen::set_profile_long_press_handler(lv_event_cb_t handler) {
    for (int i = 0; i < 3; i++) {
        if (weight_labels[i]) {
            lv_obj_add_event_cb(weight_labels[i], handler, LV_EVENT_LONG_PRESSED, NULL);
        }
    }
}
