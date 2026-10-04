#include "grinding_screen_chart.h"
#include "arduino_compat.h"
#include "../../config/constants.h"
#include <lvgl.h>
#include <widgets/span/lv_span.h>

void GrindingScreenChart::create() {
    screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(80));
    lv_obj_align(screen, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0); // Keep transparent
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE); // Make the parent screen container clickable

    // Use flex layout for centering
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen, 15, 0);

    // Profile name label
    profile_label = lv_label_create(screen);
    lv_label_set_text(profile_label, "DOUBLE");
    lv_obj_set_style_text_font(profile_label, THEME_FONT_DISPLAY_NAME, 0);
    lv_obj_set_style_text_color(profile_label, lv_color_hex(THEME_COLOR_SECONDARY), 0);

    // Create chart - use full screen width
    chart = lv_chart_create(screen);
    lv_obj_set_size(chart, LV_PCT(100), CHART_HEIGHT_PX);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR); // Points are written by index, t=0 stays left
    lv_chart_set_point_count(chart, CHART_DISPLAY_POINTS);

    // Chart styling - dark background, faint vertical lines on whole-second ticks
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, CHART_BORDER_PX, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x262626), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);

    // Series in z-order: weight (red) on primary Y, flow rate (green) on secondary Y
    weight_series = lv_chart_add_series(chart, lv_color_hex(THEME_COLOR_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);
    flow_rate_series = lv_chart_add_series(chart, lv_color_hex(THEME_COLOR_SUCCESS), LV_CHART_AXIS_SECONDARY_Y);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, static_cast<int32_t>(FLOW_AXIS_MAX_GPS * 10));

    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);

    // Remove data point markers
    lv_obj_set_style_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(chart, 0, LV_PART_INDICATOR);

    // Dim horizontal marker at the target weight (weight mode only)
    target_line = lv_obj_create(chart);
    lv_obj_remove_style_all(target_line);
    lv_obj_set_size(target_line, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(target_line, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_bg_opa(target_line, LV_OPA_40, 0);
    lv_obj_clear_flag(target_line, LV_OBJ_FLAG_CLICKABLE);

    // Time axis labels in the bottom corners
    axis_start_label = lv_label_create(chart);
    lv_label_set_text(axis_start_label, "0s");
    axis_end_label = lv_label_create(chart);
    lv_obj_t* axis_labels[2] = {axis_start_label, axis_end_label};
    for (lv_obj_t* label : axis_labels) {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(THEME_COLOR_NEUTRAL), 0);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_align(axis_start_label, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    lv_obj_align(axis_end_label, LV_ALIGN_BOTTOM_RIGHT, -4, -2);

    // Initialize data tracking
    target_weight_value = 18.0f;
    target_time_seconds = 0.0f;
    time_mode = false;
    span_ms = MIN_SPAN_MS;
    y_max_g = 0.0f;
    weight_history = nullptr;
    flow_history = nullptr;
    allocate_history();
    reset_chart_data();

    // Current/Target weight display with mixed font sizes using spangroup
    weight_spangroup = lv_spangroup_create(screen);
    lv_obj_set_width(weight_spangroup, LV_PCT(100));
    lv_obj_set_style_text_align(weight_spangroup, LV_TEXT_ALIGN_CENTER, 0);
    lv_spangroup_set_align(weight_spangroup, LV_TEXT_ALIGN_CENTER);
    lv_spangroup_set_overflow(weight_spangroup, LV_SPAN_OVERFLOW_CLIP);
    lv_spangroup_set_indent(weight_spangroup, 0);
    lv_spangroup_set_mode(weight_spangroup, LV_SPAN_MODE_BREAK);
    
    // Create initial spans using correct API
    lv_span_t* current_span = lv_spangroup_add_span(weight_spangroup);
    lv_style_set_text_font(lv_span_get_style(current_span), THEME_FONT_DISPLAY_VALUE);
    lv_style_set_text_color(lv_span_get_style(current_span), lv_color_hex(THEME_COLOR_TEXT_PRIMARY));
    lv_span_set_text(current_span, "0.0g");
    
    lv_span_t* separator_span = lv_spangroup_add_span(weight_spangroup);
    lv_style_set_text_font(lv_span_get_style(separator_span), THEME_FONT_STATUS);
    lv_style_set_text_color(lv_span_get_style(separator_span), lv_color_hex(THEME_COLOR_TEXT_SECONDARY));
    lv_span_set_text(separator_span, " / 18.0g");
    
    lv_spangroup_refresh(weight_spangroup);
    
    // MODIFIED: Ensure all child widgets pass click events to the parent screen
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(screen); i++) {
        lv_obj_clear_flag(lv_obj_get_child(screen, i), LV_OBJ_FLAG_CLICKABLE);
    }

    visible = false;
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
}

void GrindingScreenChart::show() {
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
    visible = true;
}

void GrindingScreenChart::hide() {
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    visible = false;
}

void GrindingScreenChart::update_profile_name(const char* name) {
    lv_label_set_text(profile_label, name);
}

void GrindingScreenChart::update_target_weight(float weight) {
    target_weight_value = weight;

    if (!time_mode) {
        // Update the weight display spans for current/target format
        char current_text[16], target_text[16];
        snprintf(current_text, sizeof(current_text), "0.0g");
        snprintf(target_text, sizeof(target_text), " / " SYS_WEIGHT_DISPLAY_FORMAT, weight);

        lv_span_t* current_span = lv_spangroup_get_child(weight_spangroup, 0);
        lv_span_t* separator_span = lv_spangroup_get_child(weight_spangroup, 1);

        if (current_span && separator_span) {
            lv_span_set_text(current_span, current_text);
            lv_span_set_text(separator_span, target_text);
            lv_spangroup_refresh(weight_spangroup);
        }
    }

    // Axes are only (re)derived from targets before the session has data;
    // afterwards they grow from observed samples so the trace never jumps.
    if (history_len == 0) {
        reset_chart_data();
    } else {
        update_target_line();
    }
}

void GrindingScreenChart::update_target_weight_text(const char* text) {
    lv_span_t* current_span = lv_spangroup_get_child(weight_spangroup, 0);
    lv_span_t* separator_span = lv_spangroup_get_child(weight_spangroup, 1);

    if (current_span && separator_span) {
        const char* target_text = text ? text : "";
        char formatted_text[48];
        if (target_text[0] && target_text[0] != ' ' && target_text[0] != '/' && target_text[0] != '\n') {
            snprintf(formatted_text, sizeof(formatted_text), "\n%s", target_text);
        } else {
            snprintf(formatted_text, sizeof(formatted_text), "%s", target_text);
        }
        lv_span_set_text(separator_span, formatted_text);
        lv_spangroup_refresh(weight_spangroup);
    }
}

void GrindingScreenChart::update_target_time(float seconds) {
    target_time_seconds = seconds;
    lv_span_t* current_span = lv_spangroup_get_child(weight_spangroup, 0);
    lv_span_t* separator_span = lv_spangroup_get_child(weight_spangroup, 1);

    if (current_span && separator_span) {
        // Keep the current weight span untouched; show time on a new line without slash
        char target_text[48];
        snprintf(target_text, sizeof(target_text), "\nTime: %.1fs", seconds);
        lv_span_set_text(separator_span, target_text);
        lv_spangroup_refresh(weight_spangroup);
    }

    if (history_len == 0) {
        apply_span(predicted_span_ms());
    }
}

void GrindingScreenChart::update_current_weight(float weight) {
    char current_text[16], target_text[16];
    snprintf(current_text, sizeof(current_text), SYS_WEIGHT_DISPLAY_FORMAT, weight);
    snprintf(target_text, sizeof(target_text), " / " SYS_WEIGHT_DISPLAY_FORMAT, target_weight_value);
    
    // Update spans
    lv_span_t* current_span = lv_spangroup_get_child(weight_spangroup, 0);
    lv_span_t* separator_span = lv_spangroup_get_child(weight_spangroup, 1);
    
    if (current_span && separator_span) {
        lv_span_set_text(current_span, current_text);
        if (time_mode) {
            char time_text[48];
            snprintf(time_text, sizeof(time_text), "\nTime: %.1fs", target_time_seconds);
            lv_span_set_text(separator_span, time_text);
        } else {
            lv_span_set_text(separator_span, target_text);
        }
        lv_spangroup_refresh(weight_spangroup);
    }
}

void GrindingScreenChart::update_tare_display() {
    char target_text[16];
    snprintf(target_text, sizeof(target_text), " / " SYS_WEIGHT_DISPLAY_FORMAT, target_weight_value);
    
    // Update spans for tare display
    lv_span_t* current_span = lv_spangroup_get_child(weight_spangroup, 0);
    lv_span_t* separator_span = lv_spangroup_get_child(weight_spangroup, 1);
    
    if (current_span && separator_span) {
        lv_span_set_text(current_span, "TARE");
        if (time_mode) {
            char time_text[48];
            snprintf(time_text, sizeof(time_text), "\nTime: %.1fs", target_time_seconds);
            lv_span_set_text(separator_span, time_text);
        } else {
            lv_span_set_text(separator_span, target_text);
        }
        lv_spangroup_refresh(weight_spangroup);
    }
}

void GrindingScreenChart::update_progress(int percent) {
    // Progress is now visualized through the chart data
    // This method is kept for compatibility but chart updates happen via add_chart_data_point
}

void GrindingScreenChart::add_chart_data_point(float current_weight, float flow_rate, uint32_t sample_time_ms) {
    if (!chart || !weight_history || !flow_history) {
        return;
    }

    if (history_len == 0) {
        chart_start_time_ms = sample_time_ms;
        last_sample_time_ms = sample_time_ms;
        paused_time_ms = 0;
    }

    // Samples arrive every grind-control cycle; a long gap means charting was
    // suspended (e.g. purge confirm), so cut that time out of the timeline.
    if (sample_time_ms < last_sample_time_ms) {
        sample_time_ms = last_sample_time_ms;
    }
    const uint32_t gap_ms = sample_time_ms - last_sample_time_ms;
    if (gap_ms > PAUSE_GAP_MS) {
        paused_time_ms += gap_ms - HISTORY_BUCKET_MS;
    }
    last_sample_time_ms = sample_time_ms;

    const uint32_t elapsed_ms = sample_time_ms - chart_start_time_ms - paused_time_ms;
    uint32_t bucket = elapsed_ms / HISTORY_BUCKET_MS;
    if (bucket >= HISTORY_MAX_BUCKETS) {
        bucket = HISTORY_MAX_BUCKETS - 1;
    }

    const float chart_weight = (current_weight > 0.0f) ? current_weight : 0.0f;
    const float chart_flow = (flow_rate < 0.0f) ? 0.0f : ((flow_rate > FLOW_AXIS_MAX_GPS) ? FLOW_AXIS_MAX_GPS : flow_rate);
    const int16_t weight_x10 = static_cast<int16_t>(chart_weight > 3000.0f ? 30000 : chart_weight * 10.0f);
    const int16_t flow_x10 = static_cast<int16_t>(chart_flow * 10.0f);

    // Hold the previous value across any buckets skipped by dropped events
    const int16_t hold_weight = history_len ? weight_history[history_len - 1] : weight_x10;
    const int16_t hold_flow = history_len ? flow_history[history_len - 1] : flow_x10;
    for (uint32_t b = history_len; b < bucket; ++b) {
        weight_history[b] = hold_weight;
        flow_history[b] = hold_flow;
    }
    weight_history[bucket] = weight_x10;
    flow_history[bucket] = flow_x10;
    if (bucket + 1 > history_len) {
        history_len = static_cast<uint16_t>(bucket + 1);
    }

    bool needs_full_render = false;
    if (chart_weight > y_max_g * 0.95f) {
        apply_y_range(chart_weight * 1.15f);
        needs_full_render = true;
    }
    if (elapsed_ms > span_ms && span_ms < HISTORY_MAX_MS) {
        apply_span(static_cast<uint32_t>(elapsed_ms * SPAN_HEADROOM));
        needs_full_render = true;
    }

    const uint16_t current_index = display_index_for_elapsed(elapsed_ms);
    if (needs_full_render) {
        render_all();
    } else {
        render_points(last_rendered_index, current_index);
    }
    last_rendered_index = current_index;

    lv_chart_refresh(chart);
}

void GrindingScreenChart::reset_chart_data() {
    chart_start_time_ms = 0;
    last_sample_time_ms = 0;
    paused_time_ms = 0;
    history_len = 0;
    last_rendered_index = 0;

    if (!chart) {
        return;
    }

    const float initial_y_max = time_mode
        ? TIME_MODE_INITIAL_Y_MAX_G
        : ((target_weight_value * 1.1f > target_weight_value + 1.0f) ? target_weight_value * 1.1f
                                                                      : target_weight_value + 1.0f);
    apply_y_range(initial_y_max);
    apply_span(predicted_span_ms());

    if (weight_series) {
        lv_chart_set_all_values(chart, weight_series, LV_CHART_POINT_NONE);
    }
    if (flow_rate_series) {
        lv_chart_set_all_values(chart, flow_rate_series, LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(chart);
}

void GrindingScreenChart::set_time_mode(bool enabled) {
    time_mode = enabled;
    if (time_mode) {
        update_target_time(target_time_seconds);
    } else {
        // Revert to weight display formatting using the last known target weight
        update_target_weight(target_weight_value);
    }
    update_target_line();
}

void GrindingScreenChart::allocate_history() {
    // Session history lives in PSRAM via the LVGL allocator (see lv_mem_core_psram.cpp)
    const size_t bytes = sizeof(int16_t) * HISTORY_MAX_BUCKETS;
    weight_history = static_cast<int16_t*>(lv_malloc(bytes));
    flow_history = static_cast<int16_t*>(lv_malloc(bytes));
    if (!weight_history || !flow_history) {
        lv_free(weight_history);
        lv_free(flow_history);
        weight_history = nullptr;
        flow_history = nullptr;
    }
}

uint32_t GrindingScreenChart::predicted_span_ms() const {
    // Time mode knows its duration; weight mode estimates it from the target weight
    const float predicted_ms = (time_mode && target_time_seconds > 0.0f)
        ? target_time_seconds * 1000.0f
        : 1000.0f + (target_weight_value / REFERENCE_FLOW_RATE_GPS) * 1000.0f;
    return static_cast<uint32_t>(predicted_ms * SPAN_HEADROOM);
}

void GrindingScreenChart::apply_span(uint32_t desired_span_ms) {
    if (desired_span_ms < MIN_SPAN_MS) {
        desired_span_ms = MIN_SPAN_MS;
    }

    // Pick a tick spacing that keeps grid lines readable at this span
    const uint32_t tick_ms = (desired_span_ms <= 10000) ? 1000 : ((desired_span_ms <= 20000) ? 2000 : 5000);
    uint32_t rounded_span_ms = ((desired_span_ms + tick_ms - 1) / tick_ms) * tick_ms;
    if (rounded_span_ms > HISTORY_MAX_MS) {
        rounded_span_ms = HISTORY_MAX_MS;
    }

    span_ms = rounded_span_ms;
    if (chart) {
        // N vertical lines divide the plot into N-1 equal tick intervals
        lv_chart_set_div_line_count(chart, 0, span_ms / tick_ms + 1);
    }
    update_axis_labels();
}

void GrindingScreenChart::apply_y_range(float max_g) {
    y_max_g = (max_g > 1.0f) ? max_g : 1.0f;
    if (chart) {
        lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, static_cast<int32_t>(y_max_g * 10.0f));
    }
    update_target_line();
}

void GrindingScreenChart::update_target_line() {
    if (!target_line) {
        return;
    }

    if (time_mode || target_weight_value <= 0.0f || target_weight_value > y_max_g) {
        lv_obj_add_flag(target_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const int32_t content_height = CHART_HEIGHT_PX - 2 * CHART_BORDER_PX;
    const int32_t y = content_height - static_cast<int32_t>((target_weight_value / y_max_g) * content_height);
    lv_obj_set_pos(target_line, 0, y);
    lv_obj_clear_flag(target_line, LV_OBJ_FLAG_HIDDEN);
}

void GrindingScreenChart::update_axis_labels() {
    if (!axis_end_label) {
        return;
    }
    lv_label_set_text_fmt(axis_end_label, "%lus", static_cast<unsigned long>(span_ms / 1000));
}

uint16_t GrindingScreenChart::display_index_for_elapsed(uint32_t elapsed_ms) const {
    if (span_ms == 0) {
        return 0;
    }
    uint32_t index = (static_cast<uint64_t>(elapsed_ms) * (CHART_DISPLAY_POINTS - 1)) / span_ms;
    if (index >= CHART_DISPLAY_POINTS) {
        index = CHART_DISPLAY_POINTS - 1;
    }
    return static_cast<uint16_t>(index);
}

void GrindingScreenChart::render_points(uint16_t from_index, uint16_t to_index) {
    if (!chart || !weight_series || !flow_rate_series || span_ms == 0) {
        return;
    }

    int32_t* weight_points = lv_chart_get_series_y_array(chart, weight_series);
    int32_t* flow_points = lv_chart_get_series_y_array(chart, flow_rate_series);
    if (!weight_points || !flow_points) {
        return;
    }

    if (to_index >= CHART_DISPLAY_POINTS) {
        to_index = CHART_DISPLAY_POINTS - 1;
    }

    for (uint32_t i = from_index; i <= to_index; ++i) {
        const uint32_t point_time_ms = (static_cast<uint64_t>(i) * span_ms) / (CHART_DISPLAY_POINTS - 1);
        const uint32_t bucket = point_time_ms / HISTORY_BUCKET_MS;
        if (bucket < history_len) {
            weight_points[i] = weight_history[bucket];
            flow_points[i] = flow_history[bucket];
        } else {
            weight_points[i] = LV_CHART_POINT_NONE;
            flow_points[i] = LV_CHART_POINT_NONE;
        }
    }
}

void GrindingScreenChart::render_all() {
    render_points(0, CHART_DISPLAY_POINTS - 1);
}
