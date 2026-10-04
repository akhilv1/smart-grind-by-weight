#pragma once
#include <lvgl.h>
#include "../../config/constants.h"
#include "../../controllers/grind_mode.h"

// What the AUTO tab's action slot shows under its labels
enum class AutoPageAction {
    LOGO,       // Waiting for a placement: blue arrows around an "A"
    DETECTING,  // Reading a placement: yellow, arrows spinning
    START,      // Matched: closed ring with shot dots over a play glyph; tapping starts the grind
    LOCKED,     // Portafilter still on after a grind: same ring without the play glyph, not tappable
    ASK_LABEL   // Unknown setup: SINGLE / DOUBLE pills
};

class ReadyScreen {
public:
    static constexpr int kTabCount = 6;     // AUTO, Single, Double, Custom, MENU, Scale

private:
    lv_obj_t* screen;
    lv_obj_t* tabview;
    lv_obj_t* profile_tabs[4];
    lv_obj_t* weight_labels[3];
    lv_obj_t* menu_tab;
    lv_obj_t* scale_tab;
    lv_obj_t* auto_tab;
    lv_obj_t* auto_name_label;         // "AUTO", or the matched shot ("DOUBLE" / "SINGLE")
    lv_obj_t* auto_value_label;
    lv_obj_t* auto_status_label;
    lv_obj_t* auto_label_buttons[2];   // SINGLE / DOUBLE (shown when asking the user to label)
    lv_obj_t* auto_swap_button;        // Flips the guess ("Single?" / "Double?")
    lv_obj_t* auto_swap_label;
    // AUTO mark: a ring layer (arrows or closed circle; spins while detecting) under a
    // glyph layer ("A", or shot dots once matched) and a play symbol, inside a
    // container that doubles as the START control
    lv_obj_t* auto_logo;
    lv_obj_t* auto_logo_ring;
    lv_obj_t* auto_logo_glyph;
    lv_obj_t* auto_logo_play_label;
    AutoPageAction auto_logo_action = AutoPageAction::ASK_LABEL;  // Forces the first draw
    uint8_t auto_logo_shots = 0;
    lv_obj_t* scale_weight_label;
    lv_obj_t* scale_tare_button;
    lv_obj_t* scale_grind_button;
    // In-tab action buttons (slide with the swipe; replace the READY-state floater)
    lv_obj_t* profile_action_buttons[3];
    lv_obj_t* menu_action_button;
    lv_obj_t* page_dots_container;
    lv_obj_t* page_dots[kTabCount];
    bool visible;

public:
    void create();
    void show();
    void hide();
    void update_profile_values(const float values[3], GrindMode mode);
    void set_active_tab(int tab);
    void set_profile_long_press_handler(lv_event_cb_t handler);
    void update_scale_weight(float weight);
    void update_page_dots(int active_index);
    // AUTO tab: name line, big value line, status line underneath, and the action slot.
    // Once matched (START / LOCKED) the logo shows `shots` dots; swap_text labels the
    // guess-flip pill (START only).
    void update_auto_page(const char* name_text, const char* value_text, const char* status_text,
                          AutoPageAction action, uint8_t shots = 0, const char* swap_text = nullptr);

    bool is_visible() const { return visible; }
    lv_obj_t* get_screen() const { return screen; }
    lv_obj_t* get_tabview() const { return tabview; }
    lv_obj_t* get_menu_tab() const { return menu_tab; }
    lv_obj_t* get_scale_tare_button() const { return scale_tare_button; }
    lv_obj_t* get_scale_grind_button() const { return scale_grind_button; }
    lv_obj_t* get_auto_label_button(int shot_index) const { return auto_label_buttons[shot_index]; }

private:
    void create_profile_page(lv_obj_t* parent, int profile_index, const char* profile_name, float weight);
    void create_menu_page(lv_obj_t* parent);
    void create_scale_page(lv_obj_t* parent);
    void create_auto_page(lv_obj_t* parent);
    void create_auto_logo(lv_obj_t* parent);
    void set_auto_logo(AutoPageAction action, uint8_t shots);
    void create_page_dots();
};
