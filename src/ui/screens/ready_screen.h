#pragma once
#include <lvgl.h>
#include "../../config/constants.h"
#include "../../controllers/grind_mode.h"

// Which action buttons the AUTO tab shows under its labels
enum class AutoPageAction {
    NONE,
    ASK_LABEL,  // SINGLE / DOUBLE pills: unknown setup
    START       // START button plus a pill to flip the guess
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
    lv_obj_t* auto_value_label;
    lv_obj_t* auto_status_label;
    lv_obj_t* auto_label_buttons[2];   // SINGLE / DOUBLE (shown when asking the user to label)
    lv_obj_t* auto_start_button;       // Starts the guessed profile
    lv_obj_t* auto_swap_button;        // Flips the guess ("Single?" / "Double?")
    lv_obj_t* auto_swap_label;
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
    // AUTO tab: big value line, status line underneath, and the action buttons.
    // swap_text labels the guess-flip pill (START only); start_color matches the grind mode.
    void update_auto_page(const char* value_text, const char* status_text, AutoPageAction action,
                          const char* swap_text = nullptr, uint32_t start_color = THEME_COLOR_PRIMARY);

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
    void create_page_dots();
};
