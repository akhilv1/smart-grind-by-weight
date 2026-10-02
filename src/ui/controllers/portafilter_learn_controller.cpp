#include "portafilter_learn_controller.h"

#include <cstdio>
#include "arduino_compat.h"
#include "../../config/constants.h"
#include "../event_bridge_lvgl.h"
#include "../ui_manager.h"

PortafilterLearnController::PortafilterLearnController(UIManager* manager)
    : ui_manager_(manager) {}

PortafilterDetector* PortafilterLearnController::detector() const {
    return (ui_manager_ && ui_manager_->auto_mode_controller_)
        ? &ui_manager_->auto_mode_controller_->detector()
        : nullptr;
}

void PortafilterLearnController::register_events() {
    using ET = EventBridgeLVGL::EventType;
    EventBridgeLVGL::register_handler(ET::LEARN_LABEL_SINGLE,
                                      [this](lv_event_t*) { handle_label(ShotType::SINGLE); });
    EventBridgeLVGL::register_handler(ET::LEARN_LABEL_DOUBLE,
                                      [this](lv_event_t*) { handle_label(ShotType::DOUBLE); });
    EventBridgeLVGL::register_handler(ET::LEARN_FORGET, [this](lv_event_t* e) { handle_forget(e); });
}

void PortafilterLearnController::start(bool return_to_auto) {
    if (!ui_manager_) {
        return;
    }
    return_to_auto_ = return_to_auto;
    LOG_BLE("[LEARN PF] Started\n");
    resume();
}

void PortafilterLearnController::resume() {
    tracker_.reset();
    ui_manager_->switch_to_state(UIState::PORTAFILTER_LEARN);
    if (PortafilterDetector* d = detector()) {
        ui_manager_->portafilter_learn_screen.update_setup_list(*d);
    }
    enter_phase(Phase::CLEAR_SCALE);
}

void PortafilterLearnController::update() {
    auto* sensor = (ui_manager_ && ui_manager_->hardware_manager)
        ? ui_manager_->hardware_manager->get_weight_sensor()
        : nullptr;
    if (!sensor) {
        return;
    }

    switch (tracker_.update(*sensor)) {
        case PlacementTracker::Event::PLACED:
            enter_phase(Phase::SETTLING);
            break;
        case PlacementTracker::Event::SETTLED:
            show_measurement(tracker_.step_g());
            break;
        case PlacementTracker::Event::LIFTED:
            enter_phase(Phase::WAIT_FOR_PLACEMENT);
            break;
        case PlacementTracker::Event::NONE:
            break;
    }

    if (phase_ == Phase::CLEAR_SCALE && tracker_.has_baseline()) {
        enter_phase(Phase::WAIT_FOR_PLACEMENT);
    }
}

void PortafilterLearnController::show_measurement(float step_g) {
    step_g_ = step_g;
    PortafilterDetector* d = detector();
    detection_ = d ? d->classify(step_g) : PortafilterDetection{};
    LOG_BLE("[LEARN PF] Measured %.2fg\n", static_cast<double>(step_g));
    enter_phase(Phase::ASK_LABEL);
}

void PortafilterLearnController::handle_label(ShotType shot_type) {
    PortafilterDetector* d = detector();
    if (!d || phase_ != Phase::ASK_LABEL) {
        return;
    }

    // Teaching distinct setups: only reinforce the matched setup when this weight is
    // that same physical setup; otherwise the detector applies the same strict test
    // to its nearest same-label setup or starts a new one
    const int matched = detection_.cluster_index;
    const int hint = (matched >= 0 &&
                      d->cluster(matched).shot_type == static_cast<uint8_t>(shot_type) &&
                      d->is_same_setup(matched, step_g_))
        ? matched
        : -1;
    bool created = false;
    const int index = d->learn(step_g_, shot_type, hint, &created);

    if (index >= 0 && !created) {
        snprintf(saved_text_, sizeof(saved_text_), "Added to %s %.1fg",
                 shot_type == ShotType::DOUBLE ? "Double" : "Single",
                 static_cast<double>(d->cluster(index).mean_g));
    } else {
        snprintf(saved_text_, sizeof(saved_text_), "New %s setup saved",
                 shot_type == ShotType::DOUBLE ? "double" : "single");
    }

    ui_manager_->portafilter_learn_screen.update_setup_list(*d);
    enter_phase(Phase::SAVED);
}

void PortafilterLearnController::handle_forget(lv_event_t* e) {
    PortafilterDetector* d = detector();
    if (!d || !e) {
        return;
    }
    lv_obj_t* button = lv_event_get_current_target_obj(e);
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(button)));
    if (index < 0 || index >= d->cluster_count()) {
        return;
    }

    const PortafilterCluster& cluster = d->cluster(index);
    static char message[96];
    snprintf(message, sizeof(message), "%s setup at %.1fg (%u samples).",
             cluster.shot_type == static_cast<uint8_t>(ShotType::DOUBLE) ? "Double" : "Single",
             static_cast<double>(cluster.mean_g), static_cast<unsigned>(cluster.count));

    // The confirm dialog is its own UI state; both answers come back here
    ui_manager_->show_confirmation(
        "FORGET",
        message,
        "FORGET",
        lv_color_hex(THEME_COLOR_ERROR),
        [this, index]() {
            if (PortafilterDetector* detector_ptr = detector()) {
                detector_ptr->forget(index);
            }
            resume();
        },
        "CANCEL",
        [this]() { resume(); });
}

void PortafilterLearnController::handle_cancel() {
    if (!ui_manager_) {
        return;
    }
    LOG_BLE("[LEARN PF] Closed\n");
    if (return_to_auto_) {
        ui_manager_->set_current_tab(UIManager::kAutoTabIndex);
        ui_manager_->switch_to_state(UIState::READY);
        return;
    }
    ui_manager_->set_current_tab(UIManager::kMenuTabIndex);
    ui_manager_->switch_to_state(UIState::MENU);
}

void PortafilterLearnController::enter_phase(Phase phase) {
    phase_ = phase;
    PortafilterLearnScreen& screen = ui_manager_->portafilter_learn_screen;

    char weight_text[16] = "--";
    char match_text[64] = "";
    bool ask = false;
    int suggested = -1;

    switch (phase) {
        case Phase::CLEAR_SCALE:
            screen.set_status("Clear the scale");
            break;
        case Phase::WAIT_FOR_PLACEMENT:
            screen.set_status("Place a portafilter setup");
            snprintf(match_text, sizeof(match_text), "Handle + basket + funnel");
            break;
        case Phase::SETTLING:
            screen.set_status("Hold still...");
            break;
        case Phase::ASK_LABEL:
        case Phase::SAVED: {
            snprintf(weight_text, sizeof(weight_text), "%.1fg", static_cast<double>(step_g_));
            PortafilterDetector* d = detector();
            const int matched = detection_.cluster_index;
            switch (detection_.status) {
                case PortafilterMatch::MATCH:
                    snprintf(match_text, sizeof(match_text), "Looks like %s (%.1fg)",
                             detection_.shot_type == ShotType::DOUBLE ? "Double" : "Single",
                             static_cast<double>(d ? d->cluster(matched).mean_g : 0.0f));
                    suggested = static_cast<int>(detection_.shot_type);
                    break;
                case PortafilterMatch::AMBIGUOUS:
                    snprintf(match_text, sizeof(match_text), "Too close to tell");
                    break;
                case PortafilterMatch::NO_MATCH:
                case PortafilterMatch::UNTRAINED:
                    snprintf(match_text, sizeof(match_text), "New setup");
                    break;
            }
            if (phase == Phase::ASK_LABEL) {
                screen.set_status("Which basket?");
                ask = true;
            } else {
                screen.set_status("Saved. Lift it off.");
                snprintf(match_text, sizeof(match_text), "%s", saved_text_);
            }
            break;
        }
    }

    screen.set_measurement(weight_text, match_text);
    screen.set_label_buttons(ask, suggested);
}
