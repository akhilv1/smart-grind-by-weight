#pragma once

#include <lvgl.h>
#include <cstdint>

// Nyan Cat screensaver — pixel-art Pop-Tart cat with rainbow trail
class NyanCatScreensaver {
public:
    void create(lv_obj_t* parent);
    void destroy();
    void show();
    void hide();
    bool is_active() const { return visible_; }

private:
    static constexpr int PIXEL_SCALE = 6;         // Each pixel = 6x6 on screen
    static constexpr int SPRITE_W    = 26;        // Sprite width  (tail + tart body + head)
    static constexpr int SPRITE_H    = 18;        // Sprite height
    static constexpr int RAINBOW_BANDS = 6;
    static constexpr int RAINBOW_BAND_H = 2 * PIXEL_SCALE;
    static constexpr int STAR_COUNT = 14;
    static constexpr uint32_t FRAME_PERIOD_MS = 150;

    lv_obj_t* overlay_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_obj_t* rainbow_bars_[RAINBOW_BANDS] = {};
    lv_obj_t* stars_[STAR_COUNT] = {};
    lv_timer_t* anim_timer_ = nullptr;
    uint8_t current_frame_ = 0;
    uint32_t star_seed_ = 42;
    bool visible_ = false;

    lv_color_t* canvas_buf_ = nullptr;

    void draw_frame(int frame_index);
    void update_rainbow(int frame_index);
    void update_stars(int frame_index);
    static void anim_timer_cb(lv_timer_t* timer);
    uint32_t next_random();
};
