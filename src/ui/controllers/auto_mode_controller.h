#pragma once
#include <lvgl.h>
#include <cstdint>
#include "../../controllers/placement_tracker.h"
#include "../../controllers/portafilter_detector.h"

class UIManager;

// Drives the AUTO home tab: detects a portafilter being placed, identifies it with
// the PortafilterDetector, shows its guess, and grinds the matching SINGLE/DOUBLE
// profile when the user presses START.
//
// Placement detection and the tare-independent weight-step measurement come from
// PlacementTracker (shared with the Learn Portafilters tool). Unknown or ambiguous portafilters prompt the user to label them; the sample
// is only learned once the resulting grind completes successfully, so a cancelled
// misdetection never pollutes the clusters.
class AutoModeController {
public:
    explicit AutoModeController(UIManager* manager);

    // NVS keys (namespace "autogrind")
    static constexpr const char* kPrefKeyAutoStart = "pf_auto_start";

    void register_events();
    // Reload the Auto Start preference (call after the setting changes)
    void refresh_settings();
    void update();
    void on_tab_changed(int tab);
    void refresh_display();

    PortafilterDetector& detector() { return detector_; }

private:
    enum class Phase {
        WAIT_FOR_PLACEMENT,  // Tracking the empty-scale baseline
        SETTLING,            // Weight stepped up; waiting for it to settle
        GUESSED,             // Identified; waiting for START (guess can be flipped)
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

    void update_detection();
    void update_auto_start(uint32_t now);
    void update_learning(uint32_t now);
    void handle_settled_placement(float step_g);
    void handle_label(ShotType shot_type);
    void handle_start();
    void handle_swap_guess();
    void start_grind(ShotType shot_type, int cluster_hint);
    void on_activated();
    void enter_phase(Phase phase);
    bool is_active() const;

    UIManager* ui_manager_;
    PortafilterDetector detector_;

    PlacementTracker tracker_;
    Phase phase_ = Phase::WAIT_FOR_PLACEMENT;
    float last_step_g_ = 0.0f;      // Most recent placement step (portafilter weight)
    ShotType detected_shot_ = ShotType::SINGLE;
    int detected_cluster_ = -1;     // Matched setup, or -1 once the user flips the guess
    bool guess_flipped_ = false;
    bool auto_start_enabled_ = false;
    bool auto_start_pending_ = false;       // Auto Start countdown running
    uint32_t auto_start_deadline_ms_ = 0;
    const char* ask_prompt_ = nullptr;
    bool was_active_ = false;
    bool returned_from_grind_ = false;  // Next activation follows an AUTO-started grind
    PendingSample pending_;
};
