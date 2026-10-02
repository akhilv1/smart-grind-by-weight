#pragma once
#include <lvgl.h>
#include "../../controllers/placement_tracker.h"
#include "../../controllers/portafilter_detector.h"

class UIManager;

// Drives the Learn Portafilters tool: each placement is measured with the same
// PlacementTracker the AUTO tab uses, and tagging it SINGLE/DOUBLE saves the sample
// straight into the shared PortafilterDetector, with no grind required.
class PortafilterLearnController {
public:
    explicit PortafilterLearnController(UIManager* manager);

    void register_events();
    // return_to_auto: back arrow goes to the AUTO home tab instead of Settings
    void start(bool return_to_auto = false);
    void update();
    void handle_cancel();

private:
    enum class Phase {
        CLEAR_SCALE,   // Waiting for a settled empty-scale baseline
        WAIT_FOR_PLACEMENT,
        SETTLING,
        ASK_LABEL,     // Measured; waiting for SINGLE/DOUBLE
        SAVED          // Sample stored; waiting for the setup to be lifted
    };

    void handle_label(ShotType shot_type);
    void handle_forget(lv_event_t* e);
    void resume();
    void show_measurement(float step_g);
    void enter_phase(Phase phase);
    PortafilterDetector* detector() const;

    UIManager* ui_manager_;
    PlacementTracker tracker_;
    Phase phase_ = Phase::CLEAR_SCALE;
    float step_g_ = 0.0f;
    bool return_to_auto_ = false;
    char saved_text_[48] = "";
    PortafilterDetection detection_;
};
