#include "auto_mode_controller.h"

#include <cstdio>
#include "arduino_compat.h"
#include "../../config/constants.h"
#include "../../controllers/grind_mode_traits.h"
#include "../event_bridge_lvgl.h"
#include "../ui_manager.h"

// A pending sample whose grind never started (e.g. start rejected) is dropped after this
static constexpr uint32_t kPendingStartTimeoutMs = 3000;

AutoModeController::AutoModeController(UIManager* manager)
    : ui_manager_(manager) {
    detector_.init();
}

void AutoModeController::register_events() {
    using ET = EventBridgeLVGL::EventType;
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_SINGLE,
                                      [this](lv_event_t*) { handle_label(ShotType::SINGLE); });
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_DOUBLE,
                                      [this](lv_event_t*) { handle_label(ShotType::DOUBLE); });
    refresh_display();
}

bool AutoModeController::is_active() const {
    return ui_manager_ && ui_manager_->state_machine &&
           ui_manager_->state_machine->is_state(UIState::READY) &&
           ui_manager_->current_tab == UIManager::kAutoTabIndex;
}

void AutoModeController::update() {
    if (!ui_manager_ || !ui_manager_->state_machine) {
        return;
    }
    const uint32_t now = millis();
    update_learning(now);

    const bool active = is_active();
    if (active != was_active_) {
        was_active_ = active;
        if (active) {
            on_activated();
        } else if (phase_ == Phase::STARTING || phase_ == Phase::ASK_LABEL || phase_ == Phase::SETTLING) {
            // Left the tab (or a dialog opened) mid-detection: start over on return
            enter_phase(Phase::WAIT_FOR_PLACEMENT);
        }
    }

    if (active) {
        update_detection(now);
    }
}

void AutoModeController::on_activated() {
    if (!returned_from_grind_) {
        enter_phase(Phase::WAIT_FOR_PLACEMENT);
        return;
    }
    returned_from_grind_ = false;

    // The grind tared with the portafilter on, so an empty scale now reads about
    // minus the portafilter weight. Tell "still on" from "already removed".
    auto* sensor = ui_manager_->hardware_manager ? ui_manager_->hardware_manager->get_weight_sensor() : nullptr;
    const float weight = sensor ? sensor->get_weight_low_latency() : 0.0f;
    if (weight < -last_step_g_ * 0.5f) {
        enter_phase(Phase::WAIT_FOR_PLACEMENT);
    } else {
        occupied_level_g_ = weight;
        enter_phase(Phase::OCCUPIED);
    }
}

void AutoModeController::on_tab_changed(int tab) {
    if (tab == UIManager::kAutoTabIndex) {
        refresh_display();
    }
}

void AutoModeController::update_detection(uint32_t now) {
    auto* sensor = ui_manager_->hardware_manager ? ui_manager_->hardware_manager->get_weight_sensor() : nullptr;
    if (!sensor || !sensor->data_ready() || sensor->is_tare_in_progress()) {
        return;
    }

    const float weight = sensor->get_weight_low_latency();
    const bool settled = sensor->is_settled();
    const float lift_threshold_g = USER_PF_PLACEMENT_DELTA_G * 0.5f;

    switch (phase_) {
        case Phase::WAIT_FOR_PLACEMENT:
            // Check for a step before refreshing the baseline, so a fast placement
            // isn't absorbed into the baseline
            if (baseline_valid_ && (weight - baseline_g_) >= USER_PF_PLACEMENT_DELTA_G) {
                enter_phase(Phase::SETTLING);
            } else if (settled) {
                baseline_g_ = sensor->get_weight_high_latency();
                baseline_valid_ = true;
            }
            break;

        case Phase::SETTLING:
            if ((weight - baseline_g_) < lift_threshold_g) {
                enter_phase(Phase::WAIT_FOR_PLACEMENT);
            } else if (settled) {
                handle_settled_placement(sensor->get_weight_high_latency() - baseline_g_, now);
            }
            break;

        case Phase::STARTING:
            if ((weight - baseline_g_) < lift_threshold_g) {
                enter_phase(Phase::WAIT_FOR_PLACEMENT);
            } else if (now >= start_deadline_ms_) {
                start_grind(detected_shot_, detected_cluster_);
            }
            break;

        case Phase::ASK_LABEL:
            if ((weight - baseline_g_) < lift_threshold_g) {
                enter_phase(Phase::WAIT_FOR_PLACEMENT);
            }
            break;

        case Phase::OCCUPIED:
            if (weight < occupied_level_g_ - lift_threshold_g) {
                enter_phase(Phase::WAIT_FOR_PLACEMENT);
            } else if (settled) {
                occupied_level_g_ = sensor->get_weight_high_latency();
            }
            break;
    }
}

void AutoModeController::handle_settled_placement(float step_g, uint32_t now) {
    last_step_g_ = step_g;
    const PortafilterDetection detection = detector_.classify(step_g);

    switch (detection.status) {
        case PortafilterMatch::MATCH:
            LOG_BLE("[AUTO MODE] %.2fg -> %s (cluster %d, %.1f sigma)\n", static_cast<double>(step_g),
                    PortafilterDetector::shot_type_name(detection.shot_type), detection.cluster_index,
                    static_cast<double>(detection.distance_sigmas));
            detected_shot_ = detection.shot_type;
            detected_cluster_ = detection.cluster_index;
            start_deadline_ms_ = now + USER_PF_START_DELAY_MS;
            enter_phase(Phase::STARTING);
            break;

        case PortafilterMatch::AMBIGUOUS:
            LOG_BLE("[AUTO MODE] %.2fg is ambiguous between single and double\n", static_cast<double>(step_g));
            ask_prompt_ = "Too close to tell.\nSingle or double?";
            enter_phase(Phase::ASK_LABEL);
            break;

        case PortafilterMatch::NO_MATCH:
            LOG_BLE("[AUTO MODE] %.2fg matches no learned portafilter\n", static_cast<double>(step_g));
            ask_prompt_ = "New portafilter?\nSingle or double?";
            enter_phase(Phase::ASK_LABEL);
            break;

        case PortafilterMatch::UNTRAINED:
            LOG_BLE("[AUTO MODE] %.2fg placed, nothing learned yet\n", static_cast<double>(step_g));
            ask_prompt_ = "Single or double?";
            enter_phase(Phase::ASK_LABEL);
            break;
    }
}

void AutoModeController::handle_label(ShotType shot_type) {
    if (!is_active() || phase_ != Phase::ASK_LABEL) {
        return;
    }
    detected_shot_ = shot_type;
    start_grind(shot_type, -1);
}

void AutoModeController::start_grind(ShotType shot_type, int cluster_hint) {
    if (!ui_manager_->profile_controller || !ui_manager_->grinding_controller_) {
        return;
    }

    // The sample is learned only if this grind completes (see update_learning)
    pending_ = {};
    pending_.valid = true;
    pending_.weight_g = last_step_g_;
    pending_.shot_type = shot_type;
    pending_.cluster_hint = cluster_hint;
    pending_.created_ms = millis();

    returned_from_grind_ = true;
    occupied_level_g_ = baseline_g_ + last_step_g_;
    enter_phase(Phase::OCCUPIED);

    ui_manager_->profile_controller->set_current_profile(static_cast<int>(shot_type));
    LOG_BLE("[AUTO MODE] Starting %s grind\n", PortafilterDetector::shot_type_name(shot_type));
    ui_manager_->grinding_controller_->handle_grind_button();
}

void AutoModeController::update_learning(uint32_t now) {
    if (!pending_.valid) {
        return;
    }

    switch (ui_manager_->state_machine->get_current_state()) {
        case UIState::GRINDING:
        case UIState::PURGE_CONFIRM:
            pending_.grind_started = true;
            break;

        case UIState::GRIND_COMPLETE:
            // Any completed grind confirms the identification (overshoot is a grind
            // accuracy issue, not a wrong portafilter)
            if (pending_.grind_started) {
                detector_.learn(pending_.weight_g, pending_.shot_type, pending_.cluster_hint);
                pending_.valid = false;
            }
            break;

        case UIState::READY:
            // Grind cancelled, or never started
            if (pending_.grind_started || (now - pending_.created_ms) > kPendingStartTimeoutMs) {
                LOG_BLE("[AUTO MODE] Grind did not complete - discarding %.2fg sample\n",
                        static_cast<double>(pending_.weight_g));
                pending_.valid = false;
            }
            break;

        case UIState::GRIND_TIMEOUT:
            pending_.valid = false;
            break;

        default:
            break;
    }
}

void AutoModeController::enter_phase(Phase phase) {
    phase_ = phase;
    if (phase == Phase::WAIT_FOR_PLACEMENT) {
        // Re-establish the empty-scale level before looking for the next placement
        baseline_valid_ = false;
    }
    refresh_display();
}

void AutoModeController::refresh_display() {
    if (!ui_manager_ || !ui_manager_->profile_controller) {
        return;
    }

    char value_text[24] = "--";
    char status_text[48] = "";
    bool ask_label = false;
    const bool show_target = (phase_ == Phase::STARTING || phase_ == Phase::OCCUPIED);

    if (show_target) {
        const float target = get_profile_target(*ui_manager_->profile_controller, ui_manager_->current_mode,
                                                static_cast<int>(detected_shot_));
        format_ready_value(value_text, sizeof(value_text), ui_manager_->current_mode, target);
    }

    switch (phase_) {
        case Phase::WAIT_FOR_PLACEMENT:
            snprintf(status_text, sizeof(status_text), "Place portafilter");
            break;
        case Phase::SETTLING:
            snprintf(status_text, sizeof(status_text), "Hold still...");
            break;
        case Phase::STARTING:
            snprintf(status_text, sizeof(status_text), "%s detected",
                     detected_shot_ == ShotType::DOUBLE ? "Double" : "Single");
            break;
        case Phase::ASK_LABEL:
            snprintf(status_text, sizeof(status_text), "%s", ask_prompt_ ? ask_prompt_ : "Single or double?");
            ask_label = true;
            break;
        case Phase::OCCUPIED:
            snprintf(status_text, sizeof(status_text), "Remove portafilter\nto grind again");
            break;
    }

    ui_manager_->ready_screen.update_auto_page(value_text, status_text, ask_label);
}
