#pragma once
#include <lvgl.h>
#include <cstdint>
#include "grinding_screen_base.h"
#include "../../config/constants.h"

// Weight/flow chart with a true time-based X axis.
//
// t=0 is pinned to the left edge and the trace grows to the right. Samples are
// bucketed by their grind-controller timestamp into a session history buffer,
// so queue jitter or dropped UI events never distort the time scale. When the
// grind outlasts the visible span, the span widens and the whole session is
// re-rendered from history, so the full session is always in view.
class GrindingScreenChart : public IGrindingScreen {
private:
    lv_obj_t* screen;
    lv_obj_t* profile_label;
    lv_obj_t* weight_spangroup;
    lv_obj_t* chart;
    lv_obj_t* target_line;
    lv_obj_t* axis_start_label;
    lv_obj_t* axis_end_label;
    lv_chart_series_t* weight_series;
    lv_chart_series_t* flow_rate_series;
    bool visible;
    bool time_mode;

    // Layout
    static constexpr int32_t CHART_HEIGHT_PX = 140;
    static constexpr int32_t CHART_BORDER_PX = 1;
    static constexpr uint16_t CHART_DISPLAY_POINTS = 200;

    // Session history (time-bucketed, values scaled x10)
    static constexpr uint32_t HISTORY_BUCKET_MS = 50;
    static constexpr uint32_t HISTORY_MAX_MS = (GRIND_TIMEOUT_SEC + 10) * 1000UL;
    static constexpr uint16_t HISTORY_MAX_BUCKETS = HISTORY_MAX_MS / HISTORY_BUCKET_MS;
    int16_t* weight_history;
    int16_t* flow_history;
    uint16_t history_len;

    // Timeline
    static constexpr uint32_t PAUSE_GAP_MS = 500;           // Larger sample gaps are paused time (e.g. purge confirm)
    static constexpr uint32_t MIN_SPAN_MS = 4000;
    static constexpr float SPAN_HEADROOM = 1.2f;            // Extra room beyond the predicted/observed duration
    static constexpr float REFERENCE_FLOW_RATE_GPS = 1.6f;  // Fallback flow rate for span prediction
    uint32_t chart_start_time_ms;
    uint32_t last_sample_time_ms;
    uint32_t paused_time_ms;
    uint32_t span_ms;
    uint16_t last_rendered_index;

    // Y axes (values scaled x10)
    static constexpr float FLOW_AXIS_MAX_GPS = 2.5f;
    static constexpr float TIME_MODE_INITIAL_Y_MAX_G = 5.0f;
    float target_weight_value;
    float target_time_seconds;
    float y_max_g;

    void allocate_history();
    uint32_t predicted_span_ms() const;
    void apply_span(uint32_t desired_span_ms);
    void apply_y_range(float max_g);
    void update_target_line();
    void update_axis_labels();
    void render_points(uint16_t from_index, uint16_t to_index);
    void render_all();
    uint16_t display_index_for_elapsed(uint32_t elapsed_ms) const;

public:
    void create() override;
    void show() override;
    void hide() override;
    void update_profile_name(const char* name) override;
    void update_target_weight(float weight) override;
    void update_target_weight_text(const char* text) override;
    void update_target_time(float seconds);
    void update_current_weight(float weight) override;
    void update_tare_display() override;
    void update_progress(int percent) override;
    void add_chart_data_point(float current_weight, float flow_rate, uint32_t sample_time_ms) override;
    void reset_chart_data();
    void set_time_mode(bool enabled);

    bool is_visible() const override { return visible; }
    lv_obj_t* get_screen() const override { return screen; }
};
