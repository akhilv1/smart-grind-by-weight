#pragma once
#include <lvgl.h>
#include "../../config/constants.h"
#include "../../controllers/portafilter_detector.h"

// Learn Portafilters tool (Menu -> Tools): place each handle/basket/funnel setup,
// tag it SINGLE or DOUBLE, and see how well every learned setup is separated from
// the setups with the other label.
class PortafilterLearnScreen {
public:
    void create();
    void show();
    void hide();

    // Top section: instruction line, big measured weight, match hint underneath
    void set_status(const char* status_text);
    void set_measurement(const char* weight_text, const char* match_text);
    // ask: show SINGLE/DOUBLE; suggested: which one the detector thinks it is (-1 none)
    void set_label_buttons(bool ask, int suggested_shot);
    // Rebuild the learned-setup list (sorted by weight, colored by separation)
    void update_setup_list(const PortafilterDetector& detector);

    bool is_visible() const { return visible_; }
    lv_obj_t* get_screen() const { return screen_; }

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* weight_label_ = nullptr;
    lv_obj_t* match_label_ = nullptr;
    lv_obj_t* label_buttons_[2] = {nullptr, nullptr};
    lv_obj_t* summary_label_ = nullptr;
    lv_obj_t* setup_list_ = nullptr;
    bool visible_ = false;
};
