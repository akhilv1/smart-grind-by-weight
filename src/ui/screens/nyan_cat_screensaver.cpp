#include "nyan_cat_screensaver.h"
#include "../../config/constants.h"
#include <esp_heap_caps.h>
#include <cstring>

// ---------------------------------------------------------------------------
// Nyan Cat sprite — Pop-Tart IS the cat's torso (one unified body).
// Grey head/ears on the right, stubby galloping legs below, tail on the left,
// and the rainbow flows directly out of the body's left edge (see create()).
// Compact, chubby proportions. 26x18 sprite, 2 animation frames.
// ---------------------------------------------------------------------------

// RGB565 palette
static constexpr uint16_t _  = 0x0000;  // transparent (black bg)
static constexpr uint16_t OL = 0x2104;  // dark outline (visible on black)
static constexpr uint16_t TN = 0xFE09;  // tan Pop-Tart crust  (#FFC07A)
static constexpr uint16_t PK = 0xFCF3;  // pink frosting        (#FF99C0)
static constexpr uint16_t SP = 0xF26B;  // dark pink sprinkle   (#F04D58)
static constexpr uint16_t GY = 0x9CF3;  // grey cat body        (#999999)
static constexpr uint16_t WT = 0xFFFF;  // white (eyes)
static constexpr uint16_t CK = 0xFCB2;  // cheek pink           (#FF9590)

// Sprite is 26 wide × 18 tall.  cols 0-2 tail, 3-15 tart body, 16-24 head.
static constexpr int SPR_W = 26;
static constexpr int SPR_H = 18;

// ---- Frame 0: tail down, legs extended (gallop pose A) ----
static const uint16_t frame0[18][26] = {
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,OL,OL, _, _,OL,OL, _, _, _}, // 0  ear tips
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,OL,GY,GY,OL,OL,GY,GY,OL, _, _}, // 1  ears
  { _, _, _,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _}, // 2  tart top + head top
  { _, _, _,OL,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 3  crust + face
  { _, _, _,OL,TN,PK,PK,SP,PK,PK,PK,SP,PK,PK,TN,OL,GY,WT,WT,GY,GY,WT,WT,GY,OL, _}, // 4  frosting + eyes
  { _, _, _,OL,TN,PK,PK,PK,PK,SP,PK,PK,PK,PK,TN,OL,GY,WT,OL,GY,GY,WT,OL,GY,OL, _}, // 5  frosting + pupils
  { _, _, _,OL,TN,PK,SP,PK,PK,PK,PK,PK,SP,PK,TN,OL,GY,CK,GY,GY,GY,GY,CK,GY,OL, _}, // 6  frosting + cheeks
  { _, _, _,OL,TN,PK,PK,PK,SP,PK,PK,PK,PK,PK,TN,OL,GY,GY,GY,OL,OL,GY,GY,GY,OL, _}, // 7  frosting + mouth
  { _, _,OL,OL,TN,PK,PK,PK,PK,PK,SP,PK,PK,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 8  tail + frosting
  { _,OL,GY,OL,TN,PK,SP,PK,PK,PK,PK,PK,SP,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 9  tail + frosting
  {OL,GY,GY,OL,TN,PK,PK,PK,PK,SP,PK,PK,PK,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 10 tail + frosting
  { _,OL,OL,OL,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _}, // 11 tail end + crust + face bottom
  { _, _, _,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _, _, _, _, _, _, _, _, _, _}, // 12 body bottom
  { _, _, _, _, _,GY, _, _,GY, _, _,GY, _,GY, _, _, _, _, _, _, _, _, _, _, _, _}, // 13 legs
  { _, _, _, _, _,GY, _, _,GY, _, _,GY, _,GY, _, _, _, _, _, _, _, _, _, _, _, _}, // 14 legs
  { _, _, _, _, _,OL, _, _,OL, _, _,OL, _,OL, _, _, _, _, _, _, _, _, _, _, _, _}, // 15 paws
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _}, // 16
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _}, // 17
};

// ---- Frame 1: tail up, legs tucked (gallop pose B) ----
static const uint16_t frame1[18][26] = {
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,OL,OL, _, _,OL,OL, _, _, _}, // 0  ear tips
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,OL,GY,GY,OL,OL,GY,GY,OL, _, _}, // 1  ears
  { _, _, _,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _}, // 2  tart top + head top
  {OL,GY,GY,OL,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 3  tail up + crust + face
  { _,OL,GY,OL,TN,PK,PK,SP,PK,PK,PK,SP,PK,PK,TN,OL,GY,WT,WT,GY,GY,WT,WT,GY,OL, _}, // 4  tail + frosting + eyes
  { _, _,OL,OL,TN,PK,PK,PK,PK,SP,PK,PK,PK,PK,TN,OL,GY,WT,OL,GY,GY,WT,OL,GY,OL, _}, // 5  tail tip + frosting + pupils
  { _, _, _,OL,TN,PK,SP,PK,PK,PK,PK,PK,SP,PK,TN,OL,GY,CK,GY,GY,GY,GY,CK,GY,OL, _}, // 6  frosting + cheeks
  { _, _, _,OL,TN,PK,PK,PK,SP,PK,PK,PK,PK,PK,TN,OL,GY,GY,GY,OL,OL,GY,GY,GY,OL, _}, // 7  frosting + mouth
  { _, _, _,OL,TN,PK,PK,PK,PK,PK,SP,PK,PK,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 8  frosting
  { _, _, _,OL,TN,PK,SP,PK,PK,PK,PK,PK,SP,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 9  frosting
  { _, _, _,OL,TN,PK,PK,PK,PK,SP,PK,PK,PK,PK,TN,OL,GY,GY,GY,GY,GY,GY,GY,GY,OL, _}, // 10 frosting
  { _, _, _,OL,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,TN,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _}, // 11 crust + face bottom
  { _, _, _,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL,OL, _, _, _, _, _, _, _, _, _, _}, // 12 body bottom
  { _, _, _, _, _, _,GY, _, _,GY, _, _,GY, _,GY, _, _, _, _, _, _, _, _, _, _, _}, // 13 legs tucked
  { _, _, _, _, _, _,OL, _, _,OL, _, _,OL, _,OL, _, _, _, _, _, _, _, _, _, _, _}, // 14 paws
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _}, // 15
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _}, // 16
  { _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _}, // 17
};

static const uint16_t (*frames[])[26] = { frame0, frame1 };
static constexpr int ACTUAL_FRAMES = 2;

// Rainbow colours (top to bottom)
static const lv_color_t rainbow_colors[6] = {
    lv_color_hex(0xFF0000),
    lv_color_hex(0xFF9900),
    lv_color_hex(0xFFFF00),
    lv_color_hex(0x33FF00),
    lv_color_hex(0x0099FF),
    lv_color_hex(0x6633CC),
};

// ---------------------------------------------------------------------------
uint32_t NyanCatScreensaver::next_random() {
    star_seed_ ^= star_seed_ << 13;
    star_seed_ ^= star_seed_ >> 17;
    star_seed_ ^= star_seed_ << 5;
    return star_seed_;
}

void NyanCatScreensaver::create(lv_obj_t* parent) {
    if (overlay_) return;

    overlay_ = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay_);
    lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);

    int canvas_w = SPR_W * PIXEL_SCALE;
    int canvas_h = SPR_H * PIXEL_SCALE;
    // Push the cat to the right so the rainbow trail has room on the left.
    int cat_x = (HW_DISPLAY_WIDTH_PX - canvas_w) / 2 + 40;
    int cat_y = (HW_DISPLAY_HEIGHT_PX - canvas_h) / 2;

    // Rainbow flows directly OUT of the Pop-Tart body. It overlaps slightly
    // under the body's left edge (col 4) and is centered on the body's vertical
    // midline (the tart spans rows 2-12, center ~row 7).
    int rainbow_right = cat_x + 4 * PIXEL_SCALE;
    int poptart_center_y = cat_y + 7 * PIXEL_SCALE;
    int total_rainbow_h = RAINBOW_BANDS * RAINBOW_BAND_H;
    int rainbow_top = poptart_center_y - total_rainbow_h / 2;

    for (int i = 0; i < RAINBOW_BANDS; i++) {
        rainbow_bars_[i] = lv_obj_create(overlay_);
        lv_obj_remove_style_all(rainbow_bars_[i]);
        lv_obj_set_style_bg_color(rainbow_bars_[i], rainbow_colors[i], 0);
        lv_obj_set_style_bg_opa(rainbow_bars_[i], LV_OPA_COVER, 0);
        lv_obj_set_size(rainbow_bars_[i], rainbow_right, RAINBOW_BAND_H);
        lv_obj_set_pos(rainbow_bars_[i], 0, rainbow_top + i * RAINBOW_BAND_H);
        lv_obj_clear_flag(rainbow_bars_[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // Stars
    for (int i = 0; i < STAR_COUNT; i++) {
        stars_[i] = lv_label_create(overlay_);
        lv_label_set_text(stars_[i], LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(stars_[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(stars_[i], &lv_font_montserrat_14, 0);
        int sx = next_random() % HW_DISPLAY_WIDTH_PX;
        int sy = next_random() % HW_DISPLAY_HEIGHT_PX;
        lv_obj_set_pos(stars_[i], sx, sy);
        lv_obj_clear_flag(stars_[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // Canvas (PSRAM)
    size_t buf_size = canvas_w * canvas_h * sizeof(lv_color_t);
    canvas_buf_ = static_cast<lv_color_t*>(
        heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!canvas_buf_)
        canvas_buf_ = static_cast<lv_color_t*>(malloc(buf_size));

    if (canvas_buf_) {
        canvas_ = lv_canvas_create(overlay_);
        lv_canvas_set_buffer(canvas_, canvas_buf_, canvas_w, canvas_h, LV_COLOR_FORMAT_NATIVE);
        lv_obj_set_pos(canvas_, cat_x, cat_y);
        lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
        draw_frame(0);
    }

    anim_timer_ = lv_timer_create(anim_timer_cb, FRAME_PERIOD_MS, this);
    lv_timer_pause(anim_timer_);
}

void NyanCatScreensaver::destroy() {
    if (anim_timer_) { lv_timer_delete(anim_timer_); anim_timer_ = nullptr; }
    if (overlay_) {
        lv_obj_delete(overlay_); overlay_ = nullptr; canvas_ = nullptr;
        memset(rainbow_bars_, 0, sizeof(rainbow_bars_));
        memset(stars_, 0, sizeof(stars_));
    }
    if (canvas_buf_) { heap_caps_free(canvas_buf_); canvas_buf_ = nullptr; }
    visible_ = false;
}

void NyanCatScreensaver::show() {
    if (!overlay_) return;
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_);
    current_frame_ = 0;
    if (anim_timer_) lv_timer_resume(anim_timer_);
    visible_ = true;
}

void NyanCatScreensaver::hide() {
    if (!overlay_) return;
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    if (anim_timer_) lv_timer_pause(anim_timer_);
    visible_ = false;
}

void NyanCatScreensaver::draw_frame(int frame_index) {
    if (!canvas_ || !canvas_buf_) return;

    int fi = frame_index % ACTUAL_FRAMES;
    const uint16_t (*sprite)[26] = frames[fi];

    lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);

    for (int y = 0; y < SPR_H; y++) {
        for (int x = 0; x < SPR_W; x++) {
            uint16_t c = sprite[y][x];
            if (c == 0x0000) continue;

            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = lv_color_make(
                (c >> 11) << 3, ((c >> 5) & 0x3F) << 2, (c & 0x1F) << 3);
            rect_dsc.bg_opa = LV_OPA_COVER;
            rect_dsc.radius = 0;
            rect_dsc.border_width = 0;

            lv_area_t area = {
                .x1 = (int32_t)(x * PIXEL_SCALE),
                .y1 = (int32_t)(y * PIXEL_SCALE),
                .x2 = (int32_t)((x + 1) * PIXEL_SCALE - 1),
                .y2 = (int32_t)((y + 1) * PIXEL_SCALE - 1),
            };
            lv_draw_rect(&layer, &rect_dsc, &area);
        }
    }

    lv_canvas_finish_layer(canvas_, &layer);
}

void NyanCatScreensaver::update_rainbow(int frame_index) {
    if (!overlay_) return;

    int canvas_h = SPR_H * PIXEL_SCALE;
    int cat_y = (HW_DISPLAY_HEIGHT_PX - canvas_h) / 2;
    int center_y = cat_y + 7 * PIXEL_SCALE;
    int total_h = RAINBOW_BANDS * RAINBOW_BAND_H;
    int base_top = center_y - total_h / 2;

    for (int i = 0; i < RAINBOW_BANDS; i++) {
        if (!rainbow_bars_[i]) continue;
        int wave = ((frame_index + i) % 4);
        if (wave >= 2) wave = 4 - wave;
        lv_obj_set_y(rainbow_bars_[i], base_top + i * RAINBOW_BAND_H + (wave - 1) * 2);
    }
}

void NyanCatScreensaver::update_stars(int frame_index) {
    for (int i = 0; i < STAR_COUNT; i++) {
        if (!stars_[i]) continue;
        bool vis = ((frame_index + i * 3) % 7) > 1;
        if (vis) lv_obj_clear_flag(stars_[i], LV_OBJ_FLAG_HIDDEN);
        else     lv_obj_add_flag(stars_[i], LV_OBJ_FLAG_HIDDEN);

        int32_t sx = lv_obj_get_x(stars_[i]) - 3;
        if (sx < -20) {
            sx = HW_DISPLAY_WIDTH_PX + (next_random() % 60);
            lv_obj_set_pos(stars_[i], sx, next_random() % HW_DISPLAY_HEIGHT_PX);
        } else {
            lv_obj_set_x(stars_[i], sx);
        }
    }
}

void NyanCatScreensaver::anim_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<NyanCatScreensaver*>(lv_timer_get_user_data(timer));
    if (!self || !self->visible_) return;
    self->current_frame_++;
    self->draw_frame(self->current_frame_);
    self->update_rainbow(self->current_frame_);
    self->update_stars(self->current_frame_);
}
