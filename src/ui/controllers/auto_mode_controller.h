#pragma once
#include <lvgl.h>
#include <cstdint>
#include "../../controllers/portafilter_detector.h"

class UIManager;

// Drives the AUTO home tab: detects a portafilter being placed, identifies it with
// the PortafilterDetector, and grinds the matching SINGLE/DOUBLE profile.
//
// Placement is measured as a weight *step* (settled weight after placement minus the
// settled baseline before), so it is independent of wherever the last tare left the
// scale. Unknown or ambiguous portafilters prompt the user to label them; the sample
// is only learned once the resulting grind completes successfully, so a cancelled
// misdetection never pollutes the clusters.
class AutoModeController {
public:
    explicit AutoModeController(UIManager* manager);

    void register_events();
    void update();
    void on_tab_changed(int tab);
    void refresh_display();

    PortafilterDetector& detector() { return detector_; }

private:
    enum class Phase {
        WAIT_FOR_PLACEMENT,  // Tracking the empty-scale baseline
        SETTLING,            // Weight stepped up; waiting for it to settle
        STARTING,            // Identified; showing the result before grinding
        ASK_LABEL,           // Unknown portafilter; waiting for SINGLE/DOUBLE
        OCCUPIED             // Portafilter still on the scale after a grind
    };

    struct PendingSample {
        bool valid = false;
        bool grind_started = false;
        float weight_g = 0.0f;
        ShotType shot_type = ShotType::SINGLE;
        int cluster_hint = -1;
        uint32_t created_ms = 0;
    };

    void update_detection(uint32_t now);
    void update_learning(uint32_t now);
    void handle_settled_placement(float step_g, uint32_t now);
    void handle_label(ShotType shot_type);
    void start_grind(ShotType shot_type, int cluster_hint);
    void on_activated();
    void enter_phase(Phase phase);
    bool is_active() const;

    UIManager* ui_manager_;
    PortafilterDetector detector_;

    Phase phase_ = Phase::WAIT_FOR_PLACEMENT;
    bool baseline_valid_ = false;
    float baseline_g_ = 0.0f;       // Settled weight before placement
    float occupied_level_g_ = 0.0f; // Settled weight while the portafilter sits on the scale
    float last_step_g_ = 0.0f;      // Most recent placement step (portafilter weight)
    uint32_t start_deadline_ms_ = 0;
    ShotType detected_shot_ = ShotType::SINGLE;
    int detected_cluster_ = -1;
    const char* ask_prompt_ = nullptr;
    bool was_active_ = false;
    bool returned_from_grind_ = false;  // Next activation follows an AUTO-started grind
    PendingSample pending_;
};
