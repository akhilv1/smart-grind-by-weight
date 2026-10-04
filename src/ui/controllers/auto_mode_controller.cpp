#include "auto_mode_controller.h"

#include <cstdio>
#include "arduino_compat.h"
#include "preferences_idf.h"
#include "../../config/constants.h"
#include "../../controllers/grind_mode_traits.h"
#include "../event_bridge_lvgl.h"
#include "../ui_manager.h"

// A pending sample whose grind never started (e.g. start rejected) is dropped after this
static constexpr uint32_t kPendingStartTimeoutMs = 3000;

AutoModeController::AutoModeController(UIManager* manager)
    : ui_manager_(manager) {
    detector_.init();
    refresh_settings();
}

void AutoModeController::refresh_settings() {
    Preferences prefs;
    prefs.begin("autogrind", true);
    auto_start_enabled_ = prefs.getBool(kPrefKeyAutoStart, false);
    prefs.end();
    if (!auto_start_enabled_) {
        auto_start_pending_ = false;
    }
}

void AutoModeController::register_events() {
    using ET = EventBridgeLVGL::EventType;
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_SINGLE,
                                      [this](lv_event_t*) { handle_label(ShotType::SINGLE); });
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_DOUBLE,
                                      [this](lv_event_t*) { handle_label(ShotType::DOUBLE); });
    EventBridgeLVGL::register_handler(ET::AUTO_START, [this](lv_event_t*) { handle_start(); });
    EventBridgeLVGL::register_handler(ET::AUTO_LONG_PRESS, [this](lv_event_t*) {
        if (is_active() && ui_manager_->portafilter_learn_controller_) {
            ui_manager_->portafilter_learn_controller_->start(true);
        }
    });
    EventBridgeLVGL::register_handler(ET::AUTO_SWAP_GUESS, [this](lv_event_t*) { handle_swap_guess(); });
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
        } else if (phase_ == Phase::GUESSED || phase_ == Phase::ASK_LABEL || phase_ == Phase::SETTLING) {
            // Left the tab (or a dialog opened) mid-detection: start over on return
            enter_phase(Phase::WAIT_FOR_PLACEMENT);
        }
    }

    if (active) {
        update_detection();
        update_auto_start(now);
    }
}

void AutoModeController::update_auto_start(uint32_t now) {
    if (phase_ != Phase::GUESSED || !auto_start_pending_ ||
        static_cast<int32_t>(now - auto_start_deadline_ms_) < 0) {
        return;
    }
    auto_start_pending_ = false;
    start_grind(detected_shot_, detected_cluster_);
}

void AutoModeController::on_activated() {
    if (!returned_from_grind_) {
        tracker_.reset();
        enter_phase(Phase::WAIT_FOR_PLACEMENT);
        return;
    }
    returned_from_grind_ = false;

    // The tracker's empty level survives the grind's tare, so it can tell whether the
    // portafilter is still on; lifting it then re-arms detection right away
    auto* sensor = ui_manager_->hardware_manager ? ui_manager_->hardware_manager->get_weight_sensor() : nullptr;
    if (sensor && tracker_.resume(*sensor)) {
        enter_phase(Phase::OCCUPIED);
    } else {
        enter_phase(Phase::WAIT_FOR_PLACEMENT);
    }
}

void AutoModeController::on_tab_changed(int tab) {
    if (tab == UIManager::kAutoTabIndex) {
        refresh_display();
    }
}

void AutoModeController::update_detection() {
    auto* sensor = ui_manager_->hardware_manager ? ui_manager_->hardware_manager->get_weight_sensor() : nullptr;
    if (!sensor) {
        return;
    }

    switch (tracker_.update(*sensor)) {
        case PlacementTracker::Event::PLACED:
            enter_phase(Phase::SETTLING);
            break;
        case PlacementTracker::Event::SETTLED:
            handle_settled_placement(tracker_.step_g());
            break;
        case PlacementTracker::Event::LIFTED:
            // Lifting dismisses the guess / label prompt and re-arms detection
            enter_phase(Phase::WAIT_FOR_PLACEMENT);
            break;
        case PlacementTracker::Event::NONE:
            break;
    }
}

void AutoModeController::handle_settled_placement(float step_g) {
    last_step_g_ = step_g;
    const PortafilterDetection detection = detector_.classify(step_g);

    switch (detection.status) {
        case PortafilterMatch::MATCH:
            LOG_BLE("[AUTO MODE] %.2fg -> %s (cluster %d, %.1f sigma)\n", static_cast<double>(step_g),
                    PortafilterDetector::shot_type_name(detection.shot_type), detection.cluster_index,
                    static_cast<double>(detection.distance_sigmas));
            detected_shot_ = detection.shot_type;
            detected_cluster_ = detection.cluster_index;
            guess_flipped_ = false;
            // Auto Start only fires on a confident match; anything else waits for the user
            auto_start_pending_ = auto_start_enabled_;
            auto_start_deadline_ms_ = millis() + USER_PF_AUTO_START_DELAY_MS;
            enter_phase(Phase::GUESSED);
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

void AutoModeController::handle_start() {
    if (!is_active() || phase_ != Phase::GUESSED) {
        return;
    }
    start_grind(detected_shot_, detected_cluster_);
}

void AutoModeController::handle_swap_guess() {
    if (!is_active() || phase_ != Phase::GUESSED) {
        return;
    }
    // A flipped guess means the matched setup was wrong for this label: learn it fresh
    detected_shot_ = (detected_shot_ == ShotType::DOUBLE) ? ShotType::SINGLE : ShotType::DOUBLE;
    detected_cluster_ = -1;
    guess_flipped_ = !guess_flipped_;
    auto_start_pending_ = false;  // A correction always waits for START
    LOG_BLE("[AUTO MODE] Guess flipped to %s\n", PortafilterDetector::shot_type_name(detected_shot_));
    refresh_display();
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
    if (phase != Phase::GUESSED) {
        auto_start_pending_ = false;
    }
    refresh_display();
}

void AutoModeController::refresh_display() {
    if (!ui_manager_ || !ui_manager_->profile_controller) {
        return;
    }

    const char* name_text = "AUTO";
    char value_text[24] = "--";
    const char* status_text = "";
    char swap_text[16] = "";
    AutoPageAction action = AutoPageAction::LOGO;
    const bool show_target = (phase_ == Phase::GUESSED || phase_ == Phase::OCCUPIED);
    const bool is_double = (detected_shot_ == ShotType::DOUBLE);

    // Once matched, the tab reads like the matching profile tab: shot name over its dose
    if (show_target) {
        const float dose = get_profile_target(*ui_manager_->profile_controller, ui_manager_->current_mode,
                                              static_cast<int>(detected_shot_));
        name_text = is_double ? "DOUBLE" : "SINGLE";
        format_ready_value(value_text, sizeof(value_text), ui_manager_->current_mode, dose);
    }

    switch (phase_) {
        case Phase::WAIT_FOR_PLACEMENT:
            status_text = "Place portafilter";
            break;
        case Phase::SETTLING:
            status_text = "Hold still...";
            action = AutoPageAction::DETECTING;
            break;
        case Phase::GUESSED:
            if (auto_start_pending_) {
                status_text = "Starting...";
            } else {
                // The matched mark is the start control, so say so
                status_text = guess_flipped_ ? "Switched. Tap to start" : "Tap to start";
            }
            snprintf(swap_text, sizeof(swap_text), "%s?", is_double ? "Single" : "Double");
            action = AutoPageAction::START;
            break;
        case Phase::ASK_LABEL:
            status_text = ask_prompt_ ? ask_prompt_ : "Single or double?";
            action = AutoPageAction::ASK_LABEL;
            break;
        case Phase::OCCUPIED:
            status_text = "Remove portafilter to grind again";
            action = AutoPageAction::LOCKED;
            break;
    }

    const uint8_t shots = is_double ? 2 : 1;
    ui_manager_->ready_screen.update_auto_page(name_text, value_text, status_text, action, shots, swap_text);
}
