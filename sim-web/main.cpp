// Browser harness for the grinder UI.
//
// Every screen and the nav bar below are the real, unmodified src/ui files
// compiled to WebAssembly. What this file replaces is the layer above them:
// UIManager's state switching and the controllers' event handlers, kept to the
// same shapes (state -> which screen shows, nav bar title/back per state, which
// event does what). Hardware is faked: grinds follow a deterministic flow curve,
// the scale tab adds weight while GRIND is held, and portafilter placement on the
// AUTO tab comes from the panel beside the canvas (sim_place / sim_lift below).
//
// The portafilter detector, profile controller and Preferences-backed settings
// are real (Preferences is an in-memory map, see platform/preferences_idf.h), so
// learned setups, profile targets and toggles behave like the device until the
// page is reloaded.

#include <lvgl.h>
#include <emscripten.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "config/constants.h"
#include "controllers/grind_mode.h"
#include "controllers/grind_mode_traits.h"
#include "controllers/profile_controller.h"
#include "controllers/portafilter_detector.h"
#include "ui/screens/ready_screen.h"
#include "ui/screens/grinding_screen_arc.h"
#include "ui/screens/grinding_screen_chart.h"
#include "ui/screens/confirm_screen.h"
#include "ui/screens/calibration_screen.h"
#include "ui/screens/portafilter_learn_screen.h"
// Shadowed screens (their ESP32-only sibling includes land on the stand-ins)
#include "screens/autotune_screen.h"
#include "screens/menu_screen.h"
#include "controllers/status_indicator_controller.h"
#include "controllers/auto_mode_controller.h"
#include "ui_manager.h"
#include "hardware/WeightSensor.h"
#include "hardware/hardware_manager.h"
#include "ui/components/blocking_overlay.h"
#include "event_bridge_lvgl.h"
#include "preferences_idf.h"

namespace {

constexpr int kDisplayWidth = HW_DISPLAY_WIDTH_PX;
constexpr int kDisplayHeight = HW_DISPLAY_HEIGHT_PX;

// Home tab order (UIManager::kAutoTabIndex etc.)
constexpr int kAutoTab = 0;
constexpr int kFirstProfileTab = 1;
constexpr int kMenuTab = kFirstProfileTab + USER_PROFILE_COUNT;
constexpr int kScaleTab = kMenuTab + 1;

// Matches GrindingUIController's floating button position
constexpr int kGrindButtonBottomY = -34;
constexpr uint32_t kSettleMs = 900;             // "Hold still..." before AUTO decides
constexpr float kFlowRateGps = 1.7f;            // Fake grinder flow at full speed

enum class SimState { READY, MENU, CONFIRM, CALIBRATION, AUTOTUNING, LEARN, GRINDING, GRIND_COMPLETE, NOTICE };

// --- Real screens and stand-ins --------------------------------------------
ReadyScreen ready_screen;
GrindingScreenArc arc_screen;
GrindingScreenChart chart_screen;
MenuScreen menu_screen;
ConfirmScreen confirm_screen;
CalibrationScreen calibration_screen;
AutoTuneScreen autotune_screen;
PortafilterLearnScreen learn_screen;
BluetoothManager stub_bluetooth;
GrindController stub_grind_controller;
DiagnosticsController stub_diagnostics;
WeightSensor stub_weight_sensor;
HardwareManager stub_hardware;
UIManager nav_owner;
StatusIndicatorController* nav_bar = nullptr;
ProfileController profiles;
PortafilterDetector detector;

SimState state = SimState::READY;
SimState return_state = SimState::MENU;     // Where a dialog's back arrow goes
GrindMode mode = GrindMode::WEIGHT;
int current_tab = kAutoTab;

// --- Grind simulation ---------------------------------------------------------
lv_obj_t* grind_button = nullptr;
lv_obj_t* grind_icon = nullptr;
bool chart_layout = false;
bool grinding = false;
bool settling = false;
float weight_g = 0.0f;
float flow_gps = 0.0f;
float target_g = 0.0f;
float target_s = 0.0f;
uint32_t grind_started_ms = 0;
uint32_t last_update_ms = 0;
bool grind_from_auto = false;

// --- Scale tab ------------------------------------------------------------------
float scale_weight_g = 0.0f;
bool manual_grinding = false;

// --- AUTO tab / portafilter placement ------------------------------------------
enum class AutoPhase { WAIT, SETTLING, GUESSED, ASK, OCCUPIED };
AutoPhase auto_phase = AutoPhase::WAIT;
bool portafilter_on = false;
float placed_g = 0.0f;
uint32_t settle_deadline_ms = 0;
ShotType detected_shot = ShotType::DOUBLE;
int detected_cluster = -1;
bool guess_flipped = false;
const char* ask_prompt = "Single or double?";

// --- Learn Portafilters screen --------------------------------------------------
enum class LearnPhase { PLACE, SETTLING, ASK, SAVED };
LearnPhase learn_phase = LearnPhase::PLACE;

// --- Simulator boundary notice ---------------------------------------------------
lv_obj_t* sim_notice = nullptr;
lv_obj_t* sim_notice_text = nullptr;
const char* pending_notice = nullptr;
void (*pending_confirm_action)() = nullptr;
lv_timer_t* autotune_timer = nullptr;

uint32_t now_ms() { return static_cast<uint32_t>(emscripten_get_now()); }

Preferences& main_prefs() { return *stub_hardware.get_preferences(); }

float profile_target(int profile) { return get_profile_target(profiles, mode, profile); }

void refresh_profiles() {
    float values[USER_PROFILE_COUNT];
    for (int i = 0; i < USER_PROFILE_COUNT; ++i) {
        values[i] = profile_target(i);
    }
    ready_screen.update_profile_values(values, mode);
}

// ===========================================================================
// State switching (mirrors UIManager::switch_to_state + apply_menubar_for_state)
// ===========================================================================
void refresh_auto_page();

void apply_nav_bar() {
    const bool immersive = (state == SimState::GRINDING || state == SimState::GRIND_COMPLETE);
    nav_bar->set_visible(!immersive);
    if (immersive) return;

    const char* title = "";
    bool back = false;
    switch (state) {
        case SimState::MENU:        title = menu_screen.get_current_title(); back = true; break;
        case SimState::CALIBRATION:
        case SimState::CONFIRM:
        case SimState::AUTOTUNING:  back = true; break;
        case SimState::LEARN:       title = "Learn"; back = true; break;
        default: break;
    }
    nav_bar->set_title(title);
    nav_bar->set_back_visible(back);
    nav_bar->bring_to_front();
}

void switch_to(SimState next) {
    state = next;
    ready_screen.hide();
    arc_screen.hide();
    chart_screen.hide();
    menu_screen.hide();
    confirm_screen.hide();
    calibration_screen.hide();
    autotune_screen.hide();
    learn_screen.hide();
    lv_obj_add_flag(sim_notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(grind_button, LV_OBJ_FLAG_HIDDEN);

    switch (next) {
        case SimState::READY:
            ready_screen.show();
            ready_screen.set_active_tab(current_tab);
            refresh_profiles();
            refresh_auto_page();
            break;
        case SimState::MENU:        menu_screen.show(); break;
        case SimState::CONFIRM:     confirm_screen.show(); break;
        case SimState::CALIBRATION: calibration_screen.show(); break;
        case SimState::AUTOTUNING:  autotune_screen.show(); break;
        case SimState::LEARN:       learn_screen.show(); break;
        case SimState::GRINDING:
        case SimState::GRIND_COMPLETE:
            if (chart_layout) chart_screen.show(); else arc_screen.show();
            lv_obj_clear_flag(grind_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(grind_button);
            break;
        case SimState::NOTICE:
            lv_obj_clear_flag(sim_notice, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(sim_notice);
            break;
    }
    apply_nav_bar();
}

// ===========================================================================
// Grinding (fake flow curve; screens are real)
// ===========================================================================
void set_grind_button(const char* symbol, uint32_t color) {
    lv_image_set_src(grind_icon, symbol);
    lv_obj_set_style_bg_color(grind_button, lv_color_hex(color), 0);
}

void start_grind(int profile, bool from_auto) {
    grind_from_auto = from_auto;
    weight_g = 0.0f;
    flow_gps = 0.0f;
    target_g = get_profile_target(profiles, GrindMode::WEIGHT, profile);
    target_s = get_profile_target(profiles, GrindMode::TIME, profile);
    grinding = true;
    settling = false;
    grind_started_ms = now_ms();
    last_update_ms = grind_started_ms;

    const char* name = profiles.get_profile_name(profile);
    for (IGrindingScreen* screen : {static_cast<IGrindingScreen*>(&arc_screen), static_cast<IGrindingScreen*>(&chart_screen)}) {
        screen->update_profile_name(name);
        screen->update_target_weight(target_g);
        screen->update_current_weight(0.0f);
        screen->update_progress(0);
    }
    arc_screen.set_time_mode(mode == GrindMode::TIME);
    chart_screen.set_time_mode(mode == GrindMode::TIME);
    if (mode == GrindMode::TIME) {
        arc_screen.update_target_time(target_s);
        chart_screen.update_target_time(target_s);
    }
    chart_screen.reset_chart_data();

    switch_to(SimState::GRINDING);
    set_grind_button(LV_SYMBOL_STOP, mode == GrindMode::TIME ? THEME_COLOR_ACCENT : THEME_COLOR_PRIMARY);
}

void finish_grind() {
    grinding = false;
    settling = false;
    flow_gps = 0.0f;
    state = SimState::GRIND_COMPLETE;
    set_grind_button(LV_SYMBOL_OK, THEME_COLOR_SUCCESS);
    if (grind_from_auto && auto_phase == AutoPhase::GUESSED) {
        // A completed AUTO grind confirms the identification (AutoModeController::update_learning)
        detector.learn(placed_g, detected_shot, detected_cluster);
    }
}

void update_grind() {
    if (!grinding && !settling) return;

    const uint32_t current_ms = now_ms();
    const float dt_s = std::min(0.1f, static_cast<float>(current_ms - last_update_ms) / 1000.0f);
    last_update_ms = current_ms;
    const float elapsed_s = static_cast<float>(current_ms - grind_started_ms) / 1000.0f;

    if (grinding) {
        flow_gps = std::min(kFlowRateGps, 0.25f + elapsed_s * 0.55f);
        weight_g += flow_gps * dt_s;
        const bool done = (mode == GrindMode::TIME) ? elapsed_s >= target_s : weight_g >= target_g - 0.45f;
        if (done) {
            grinding = false;
            settling = true;
        }
    } else {
        flow_gps *= std::pow(0.12f, dt_s);
        weight_g += flow_gps * dt_s;
        if (flow_gps < 0.015f) {
            finish_grind();
        }
    }

    const float goal = (mode == GrindMode::TIME) ? std::max(target_s, 0.1f) : std::max(target_g, 0.1f);
    const float progress_of = (mode == GrindMode::TIME) ? elapsed_s : weight_g;
    const int progress = std::clamp(static_cast<int>((progress_of / goal) * 100.0f), 0, 100);
    arc_screen.update_current_weight(weight_g);
    chart_screen.update_current_weight(weight_g);
    arc_screen.update_progress(progress);
    chart_screen.add_chart_data_point(weight_g, flow_gps, current_ms);
}

void grind_button_event(lv_event_t*) {
    if (grinding || settling) {
        // Stop mid-grind: back to the home screen, as GrindController::stop_grind() does
        grinding = settling = false;
        grind_from_auto = false;
        switch_to(SimState::READY);
    } else if (state == SimState::GRIND_COMPLETE) {
        switch_to(SimState::READY);
    }
}

void layout_toggle_event(lv_event_t*) {
    if (state != SimState::GRINDING && state != SimState::GRIND_COMPLETE) return;
    chart_layout = !chart_layout;
    if (chart_layout) { arc_screen.hide(); chart_screen.show(); } else { chart_screen.hide(); arc_screen.show(); }
    lv_obj_move_foreground(grind_button);
}

// ===========================================================================
// AUTO tab (display logic copied from AutoModeController::refresh_display)
// ===========================================================================
void refresh_auto_page() {
    const char* name_text = "AUTO";
    char value_text[24] = "--";
    const char* status_text = "";
    char swap_text[16] = "";
    AutoPageAction action = AutoPageAction::LOGO;
    const bool is_double = (detected_shot == ShotType::DOUBLE);

    if (auto_phase == AutoPhase::GUESSED || auto_phase == AutoPhase::OCCUPIED) {
        name_text = is_double ? "DOUBLE" : "SINGLE";
        format_ready_value(value_text, sizeof(value_text), mode, profile_target(static_cast<int>(detected_shot)));
    }

    switch (auto_phase) {
        case AutoPhase::WAIT:     status_text = "Place portafilter"; break;
        case AutoPhase::SETTLING: status_text = "Hold still..."; action = AutoPageAction::DETECTING; break;
        case AutoPhase::GUESSED:
            status_text = guess_flipped ? "Switched. Tap to start" : "Tap to start";
            snprintf(swap_text, sizeof(swap_text), "%s?", is_double ? "Single" : "Double");
            action = AutoPageAction::START;
            break;
        case AutoPhase::ASK:      status_text = ask_prompt; action = AutoPageAction::ASK_LABEL; break;
        case AutoPhase::OCCUPIED: status_text = "Remove portafilter to grind again"; action = AutoPageAction::LOCKED; break;
    }
    ready_screen.update_auto_page(name_text, value_text, status_text, action, is_double ? 2 : 1, swap_text);
}

void auto_classify() {
    const PortafilterDetection detection = detector.classify(placed_g);
    switch (detection.status) {
        case PortafilterMatch::MATCH:
            detected_shot = detection.shot_type;
            detected_cluster = detection.cluster_index;
            guess_flipped = false;
            auto_phase = AutoPhase::GUESSED;
            break;
        case PortafilterMatch::AMBIGUOUS: ask_prompt = "Too close to tell.\nSingle or double?"; auto_phase = AutoPhase::ASK; break;
        case PortafilterMatch::NO_MATCH:  ask_prompt = "New portafilter?\nSingle or double?";  auto_phase = AutoPhase::ASK; break;
        case PortafilterMatch::UNTRAINED: ask_prompt = "Single or double?";                    auto_phase = AutoPhase::ASK; break;
    }
    refresh_auto_page();
}

void auto_start(ShotType shot, int cluster_hint) {
    detected_shot = shot;
    detected_cluster = cluster_hint;
    auto_phase = AutoPhase::GUESSED;   // Learned when the grind completes
    start_grind(static_cast<int>(shot), true);
}

// ===========================================================================
// Learn Portafilters (copy from PortafilterLearnController)
// ===========================================================================
void learn_refresh() {
    char weight_text[24] = "";
    char match_text[64] = "";
    switch (learn_phase) {
        case LearnPhase::PLACE:
            learn_screen.set_status(portafilter_on ? "Clear the scale" : "Place a portafilter setup");
            snprintf(match_text, sizeof(match_text), "Handle + basket + funnel");
            learn_screen.set_measurement("", match_text);
            learn_screen.set_label_buttons(false, -1);
            break;
        case LearnPhase::SETTLING:
            learn_screen.set_status("Hold still...");
            learn_screen.set_measurement("", "");
            learn_screen.set_label_buttons(false, -1);
            break;
        case LearnPhase::ASK: {
            snprintf(weight_text, sizeof(weight_text), "%.1fg", static_cast<double>(placed_g));
            const PortafilterDetection d = detector.classify(placed_g);
            int suggested = -1;
            if (d.status == PortafilterMatch::MATCH) {
                const float mean = detector.cluster(d.cluster_index).mean_g;
                snprintf(match_text, sizeof(match_text), "Looks like %s (%.1fg)",
                         d.shot_type == ShotType::DOUBLE ? "Double" : "Single", static_cast<double>(mean));
                suggested = static_cast<int>(d.shot_type);
            } else if (d.status == PortafilterMatch::AMBIGUOUS) {
                snprintf(match_text, sizeof(match_text), "Too close to tell");
            } else {
                snprintf(match_text, sizeof(match_text), "New setup");
            }
            learn_screen.set_status("Which basket?");
            learn_screen.set_measurement(weight_text, match_text);
            learn_screen.set_label_buttons(true, suggested);
            break;
        }
        case LearnPhase::SAVED:
            learn_screen.set_status("Saved. Lift it off.");
            learn_screen.set_label_buttons(false, -1);
            break;
    }
    learn_screen.update_setup_list(detector);
}

void learn_open() {
    learn_phase = LearnPhase::PLACE;
    return_state = (state == SimState::READY) ? SimState::READY : SimState::MENU;
    switch_to(SimState::LEARN);
    learn_refresh();
}

void learn_label(ShotType shot) {
    if (learn_phase != LearnPhase::ASK) return;
    detector.learn(placed_g, shot);
    learn_phase = LearnPhase::SAVED;
    learn_refresh();
}

// ===========================================================================
// Settings tools: follow each to the last pure-UI point, then say so
// ===========================================================================
void show_sim_notice(const char* message) {
    lv_label_set_text(sim_notice_text, message);
    switch_to(SimState::NOTICE);
}

void show_device_confirm(const char* title, const char* message, const char* confirm_text,
                         uint32_t confirm_color, const char* notice, void (*action)() = nullptr) {
    pending_notice = notice;
    pending_confirm_action = action;
    return_state = SimState::MENU;
    confirm_screen.show(title, message, confirm_text, lv_color_hex(confirm_color));
    switch_to(SimState::CONFIRM);
}

void calibration_apply_step(CalibrationStep step) {
    calibration_screen.set_step(step);
    switch (step) {
        case CAL_STEP_EMPTY:
            calibration_screen.update_current_weight(0.0f);
            calibration_screen.set_ok_button_enabled(true);
            break;
        case CAL_STEP_WEIGHT:
            calibration_screen.update_current_weight(100.2f);
            calibration_screen.update_calibration_weight(100.0f);
            calibration_screen.set_ok_button_enabled(true);
            break;
        case CAL_STEP_NOISE_CHECK:
            calibration_screen.update_noise_status("Status: OK", lv_color_hex(THEME_COLOR_SUCCESS));
            calibration_screen.update_noise_metric(0.02f);
            calibration_screen.set_ok_button_enabled(true);
            break;
        case CAL_STEP_COMPLETE:
            break;
    }
}

void calibration_ok_event(lv_event_t*) {
    switch (calibration_screen.get_step()) {
        case CAL_STEP_EMPTY:       calibration_apply_step(CAL_STEP_WEIGHT); break;
        case CAL_STEP_WEIGHT:      calibration_apply_step(CAL_STEP_NOISE_CHECK); break;
        case CAL_STEP_NOISE_CHECK: calibration_apply_step(CAL_STEP_COMPLETE); break;
        case CAL_STEP_COMPLETE:
            show_sim_notice("Nothing was calibrated. These are the real screens, but there is no load cell; "
                            "the weights were fixed demo values.");
            break;
    }
}

void autotune_finish_cb(lv_timer_t*) {
    autotune_timer = nullptr;  // repeat count 1: LVGL deletes it after this call
    autotune_screen.show_success_screen(62.0f, 68.0f);
}

void autotune_open() {
    return_state = SimState::MENU;
    switch_to(SimState::AUTOTUNING);
    autotune_screen.show_console_screen();
    autotune_screen.append_console_message("Priming chute...");
    autotune_screen.append_console_message("Binary search: 90ms -> miss");
    autotune_screen.append_console_message("Binary search: 70ms -> hit");
    autotune_screen.append_console_message("Binary search: 60ms -> miss");
    autotune_screen.append_console_message("Verification 3/3 ok");
    autotune_timer = lv_timer_create(autotune_finish_cb, 2500, nullptr);
    lv_timer_set_repeat_count(autotune_timer, 1);
}

void autotune_close(lv_event_t*) {
    if (autotune_timer) {
        lv_timer_delete(autotune_timer);
        autotune_timer = nullptr;
    }
    show_sim_notice("Nothing was tuned. The console is a fixed transcript; auto-tune needs the real motor and load cell.");
}

// Nav bar back arrow (UIManager::handle_menubar_back)
void handle_back() {
    switch (state) {
        case SimState::MENU:
            menu_screen.go_back();  // Pops a sub-page, or fires MENU_BACK at the root
            break;
        case SimState::CONFIRM:
        case SimState::CALIBRATION:
        case SimState::NOTICE:
            switch_to(SimState::MENU);
            break;
        case SimState::AUTOTUNING:
            autotune_close(nullptr);
            break;
        case SimState::LEARN:
            switch_to(return_state);
            break;
        default:
            switch_to(SimState::READY);
            break;
    }
}

// ===========================================================================
// Settings persistence for toggles and radios (the menu re-reads these on show)
// ===========================================================================
void save_toggle(lv_event_t* e, const char* ns, const char* key) {
    lv_obj_t* toggle = static_cast<lv_obj_t*>(lv_event_get_target(e));
    Preferences prefs;
    prefs.begin(ns, false);
    prefs.putBool(key, lv_obj_has_state(toggle, LV_STATE_CHECKED));
    prefs.end();
}

void register_settings_handlers() {
    using ET = EventBridgeLVGL::EventType;
    EventBridgeLVGL::register_handler(ET::BLE_STARTUP_TOGGLE, [](lv_event_t* e) { save_toggle(e, "bluetooth", "startup"); });
    EventBridgeLVGL::register_handler(ET::LOGGING_TOGGLE, [](lv_event_t* e) { save_toggle(e, "logging", "enabled"); });
    EventBridgeLVGL::register_handler(ET::GRIND_MODE_SWIPE_TOGGLE, [](lv_event_t* e) { save_toggle(e, "swipe", "enabled"); });
    EventBridgeLVGL::register_handler(ET::AUTO_START_TOGGLE, [](lv_event_t* e) { save_toggle(e, "autogrind", "auto_start"); });
    EventBridgeLVGL::register_handler(ET::AUTO_RETURN_TOGGLE, [](lv_event_t* e) { save_toggle(e, "autogrind", "auto_return"); });
    EventBridgeLVGL::register_handler(ET::AUTO_MODE_AUTO_START_TOGGLE, [](lv_event_t* e) {
        save_toggle(e, "autogrind", AutoModeController::kPrefKeyAutoStart);
    });
    EventBridgeLVGL::register_handler(ET::PULSE_CORRECTIONS_TOGGLE, [](lv_event_t* e) {
        main_prefs().putBool(GrindController::PREF_KEY_PULSE_CORRECTIONS,
                             lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED));
    });
    EventBridgeLVGL::register_handler(ET::GRIND_MODE_RADIO_BUTTON, [](lv_event_t*) {
        const int selected = radio_button_group_get_selection(menu_screen.get_grind_mode_radio_group());
        mode = (selected == 1) ? GrindMode::TIME : GrindMode::WEIGHT;
        main_prefs().putInt("grind_mode", static_cast<int>(mode));
        profiles.set_grind_mode(mode);
    });
    EventBridgeLVGL::register_handler(ET::GRINDER_PURGE_MODE_RADIO_BUTTON, [](lv_event_t*) {
        main_prefs().putInt(GrindController::PREF_KEY_GRINDER_MODE,
                            radio_button_group_get_selection(menu_screen.get_grinder_purge_mode_radio_group()));
    });
    EventBridgeLVGL::register_handler(ET::PORTAFILTER_FORGET, [](lv_event_t* e) {
        lv_obj_t* row = lv_event_get_current_target_obj(e);
        detector.forget(static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))));
        menu_screen.update_portafilter_list();
    });
}

void register_navigation_handlers() {
    using ET = EventBridgeLVGL::EventType;
    EventBridgeLVGL::register_handler(ET::MENUBAR_BACK, [](lv_event_t*) { handle_back(); });
    EventBridgeLVGL::register_handler(ET::MENU_BACK, [](lv_event_t*) {
        current_tab = kMenuTab;
        switch_to(SimState::READY);
    });

    // In-tab action buttons: play on profile tabs, gear on the MENU tab
    EventBridgeLVGL::register_handler(ET::GRIND_BUTTON, [](lv_event_t*) {
        if (state != SimState::READY) return;
        if (current_tab == kMenuTab) {
            switch_to(SimState::MENU);
        } else if (current_tab >= kFirstProfileTab && current_tab < kMenuTab) {
            start_grind(current_tab - kFirstProfileTab, false);
        }
    });

    // Scale tab
    EventBridgeLVGL::register_handler(ET::SCALE_TARE, [](lv_event_t*) { scale_weight_g = 0.0f; });
    EventBridgeLVGL::register_handler(ET::SCALE_GRIND, [](lv_event_t* e) {
        const lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_PRESSED) manual_grinding = true;
        else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) manual_grinding = false;
    });

    // AUTO tab
    EventBridgeLVGL::register_handler(ET::AUTO_START, [](lv_event_t*) {
        if (auto_phase == AutoPhase::GUESSED) auto_start(detected_shot, detected_cluster);
    });
    EventBridgeLVGL::register_handler(ET::AUTO_SWAP_GUESS, [](lv_event_t*) {
        if (auto_phase != AutoPhase::GUESSED) return;
        detected_shot = (detected_shot == ShotType::DOUBLE) ? ShotType::SINGLE : ShotType::DOUBLE;
        detected_cluster = -1;
        guess_flipped = !guess_flipped;
        refresh_auto_page();
    });
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_SINGLE, [](lv_event_t*) {
        if (auto_phase == AutoPhase::ASK) auto_start(ShotType::SINGLE, -1);
    });
    EventBridgeLVGL::register_handler(ET::AUTO_LABEL_DOUBLE, [](lv_event_t*) {
        if (auto_phase == AutoPhase::ASK) auto_start(ShotType::DOUBLE, -1);
    });
    EventBridgeLVGL::register_handler(ET::AUTO_LONG_PRESS, [](lv_event_t*) {
        if (state == SimState::READY && current_tab == kAutoTab) learn_open();
    });

    // Learn Portafilters
    EventBridgeLVGL::register_handler(ET::MENU_LEARN_PORTAFILTERS, [](lv_event_t*) { learn_open(); });
    EventBridgeLVGL::register_handler(ET::LEARN_LABEL_SINGLE, [](lv_event_t*) { learn_label(ShotType::SINGLE); });
    EventBridgeLVGL::register_handler(ET::LEARN_LABEL_DOUBLE, [](lv_event_t*) { learn_label(ShotType::DOUBLE); });
    EventBridgeLVGL::register_handler(ET::LEARN_FORGET, [](lv_event_t* e) {
        lv_obj_t* row = lv_event_get_current_target_obj(e);
        detector.forget(static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row))));
        learn_refresh();
    });

    // Tools (dialog wording copied from MenuUIController)
    EventBridgeLVGL::register_handler(ET::MENU_CALIBRATE, [](lv_event_t*) {
        return_state = SimState::MENU;
        switch_to(SimState::CALIBRATION);
        calibration_apply_step(CAL_STEP_EMPTY);
    });
    EventBridgeLVGL::register_handler(ET::MENU_AUTOTUNE, [](lv_event_t*) {
        show_device_confirm("Auto-Tune Setup",
                            "Before starting:\n\n- Beans loaded\n- Cup on scale\n\nProcess takes ~1 min.",
                            "START", THEME_COLOR_ACCENT, nullptr, autotune_open);
    });
    EventBridgeLVGL::register_handler(ET::MENU_MOTOR_TEST, [](lv_event_t*) {
        show_device_confirm("MOTOR TEST", "Motor will be engaged for 1 second.\n\nMake sure grinder is safe to run.",
                            "RUN", THEME_COLOR_SUCCESS, "The motor test drives the real grinder motor, so it stops here.");
    });
    EventBridgeLVGL::register_handler(ET::MENU_RESET, [](lv_event_t*) {
        show_device_confirm("FACTORY RESET",
                            "This will reset all settings to factory defaults:\n\n"
                            "\xE2\x80\xA2 Profile weights\n\xE2\x80\xA2 Calibration data\n"
                            "\xE2\x80\xA2 Grind history\n\xE2\x80\xA2 Lifetime statistics\n\n"
                            "This action cannot be undone.",
                            "RESET", THEME_COLOR_ERROR, "Factory reset stops here. Reload the page to reset the simulator.");
    });
    EventBridgeLVGL::register_handler(ET::MENU_PURGE, [](lv_event_t*) {
        show_device_confirm("PURGE LOGS",
                            "This will remove all saved grind log files from flash.\n"
                            "Lifetime statistics will be preserved.\n\nThis action cannot be undone.",
                            "PURGE LOGS", THEME_COLOR_ERROR, "There are no logs in the simulator; the counts are demo data.");
    });
}

// ===========================================================================
// Widgets owned by the harness
// ===========================================================================
void create_grind_button() {
    // Same geometry as GrindingUIController::build_controls()
    grind_button = lv_button_create(lv_screen_active());
    lv_obj_set_size(grind_button, 100, 100);
    lv_obj_align(grind_button, LV_ALIGN_BOTTOM_MID, 0, kGrindButtonBottomY);
    lv_obj_set_style_radius(grind_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(grind_button, 0, 0);
    lv_obj_set_style_shadow_width(grind_button, 0, 0);
    lv_obj_add_event_cb(grind_button, grind_button_event, LV_EVENT_CLICKED, nullptr);
    grind_icon = lv_image_create(grind_button);
    lv_obj_set_style_text_font(grind_icon, THEME_FONT_SYMBOL, 0);
    lv_obj_center(grind_icon);
    lv_obj_add_flag(grind_button, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(arc_screen.get_screen(), layout_toggle_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(chart_screen.get_screen(), layout_toggle_event, LV_EVENT_CLICKED, nullptr);
}

// The one deliberately non-device element: marks where the simulator stops
void create_sim_notice() {
    sim_notice = lv_obj_create(lv_screen_active());
    lv_obj_set_size(sim_notice, THEME_CONTENT_WIDTH_PX, LV_SIZE_CONTENT);
    lv_obj_center(sim_notice);
    lv_obj_set_style_bg_color(sim_notice, lv_color_hex(THEME_COLOR_BACKGROUND), 0);
    lv_obj_set_style_border_color(sim_notice, lv_color_hex(THEME_COLOR_DETECTING), 0);
    lv_obj_set_style_border_width(sim_notice, 2, 0);
    lv_obj_set_style_radius(sim_notice, THEME_CORNER_RADIUS_PX, 0);
    lv_obj_set_style_pad_all(sim_notice, 16, 0);
    lv_obj_set_layout(sim_notice, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sim_notice, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sim_notice, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sim_notice, 12, 0);
    lv_obj_clear_flag(sim_notice, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* heading = lv_label_create(sim_notice);
    lv_label_set_text(heading, "SIMULATOR");
    lv_obj_set_style_text_font(heading, THEME_FONT_SECTION, 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(THEME_COLOR_DETECTING), 0);

    sim_notice_text = lv_label_create(sim_notice);
    lv_obj_set_width(sim_notice_text, LV_PCT(100));
    lv_label_set_long_mode(sim_notice_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(sim_notice_text, THEME_FONT_BODY, 0);
    lv_obj_set_style_text_align(sim_notice_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sim_notice_text, lv_color_hex(THEME_COLOR_TEXT_PRIMARY), 0);
    lv_obj_add_flag(sim_notice, LV_OBJ_FLAG_HIDDEN);
}

// Some screens dropped their on-screen Cancel in favor of the nav bar's back
// arrow, so their getters can return null; only wire buttons that exist.
void add_click(lv_obj_t* button, lv_event_cb_t callback) {
    if (button) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    }
}

void seed_demo_setups() {
    if (detector.cluster_count() > 0) return;
    const float singles[] = {512.3f, 512.6f, 512.1f, 512.4f};
    const float doubles[] = {531.0f, 530.8f, 531.3f, 531.1f};
    for (float g : singles) detector.learn(g, ShotType::SINGLE);
    for (float g : doubles) detector.learn(g, ShotType::DOUBLE);
}

void frame() {
    const uint32_t now = now_ms();
    update_grind();

    if (manual_grinding) {
        scale_weight_g += kFlowRateGps * 0.016f;
    }
    if (state == SimState::READY && current_tab == kScaleTab) {
        ready_screen.update_scale_weight(scale_weight_g);
    }

    if (auto_phase == AutoPhase::SETTLING && static_cast<int32_t>(now - settle_deadline_ms) >= 0) {
        auto_classify();
    }
    if (state == SimState::LEARN && learn_phase == LearnPhase::SETTLING &&
        static_cast<int32_t>(now - settle_deadline_ms) >= 0) {
        learn_phase = LearnPhase::ASK;
        learn_refresh();
    }

    if (state == SimState::MENU) {
        // Sub-page navigation is internal to lv_menu; mirror its title every tick
        nav_bar->set_title(menu_screen.get_current_title());
        menu_screen.update_diagnostics(&stub_weight_sensor);
    }
    lv_timer_handler();
}

}  // namespace

// ===========================================================================
// Called from the panel beside the canvas (shell.html)
// ===========================================================================
extern "C" {

// LVGL assert hook (lv_conf.h): log where it fired and stop, instead of the
// firmware's spin loop, which would freeze the browser tab
void sim_lvgl_assert_failed(void) {
    emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "LVGL assert failed");
    abort();
}

EMSCRIPTEN_KEEPALIVE void sim_place(float grams) {
    if (portafilter_on) return;
    portafilter_on = true;
    placed_g = grams;
    settle_deadline_ms = now_ms() + kSettleMs;
    if (state == SimState::LEARN) {
        learn_phase = LearnPhase::SETTLING;
        learn_refresh();
        return;
    }
    auto_phase = AutoPhase::SETTLING;
    refresh_auto_page();
}

EMSCRIPTEN_KEEPALIVE void sim_lift() {
    if (!portafilter_on) return;
    portafilter_on = false;
    if (state == SimState::LEARN) {
        learn_phase = LearnPhase::PLACE;
        learn_refresh();
    }
    auto_phase = AutoPhase::WAIT;  // Lifting re-arms detection
    refresh_auto_page();
}

EMSCRIPTEN_KEEPALIVE int sim_portafilter_on() { return portafilter_on ? 1 : 0; }

}  // extern "C"

// After an AUTO grind the home screen shows the locked state while the portafilter is on
static void on_return_to_ready() {
    if (portafilter_on && grind_from_auto) {
        auto_phase = AutoPhase::OCCUPIED;
    }
    grind_from_auto = false;
}

int main() {
    lv_init();
    lv_tick_set_cb(now_ms);

    lv_display_t* display = lv_sdl_window_create(kDisplayWidth, kDisplayHeight);
    if (!display) {
        std::fprintf(stderr, "lv_sdl_window_create failed\n");
        return 1;
    }
    lv_sdl_window_set_title(display, "Smart Grind - Browser Simulator");
    lv_sdl_mouse_create();
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(THEME_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    BlockingOperationOverlay::getInstance().init();

    profiles.init(&main_prefs());
    mode = profiles.get_grind_mode();
    detector.init();
    seed_demo_setups();

    ready_screen.create();
    arc_screen.create();
    chart_screen.create();

    menu_screen.set_portafilter_detector(&detector);
    menu_screen.create(&stub_bluetooth, &stub_grind_controller, /*grinding_screen=*/nullptr, &stub_hardware,
                       &stub_diagnostics);
    // Populate the read-only pages once (they no-op unless the menu is visible)
    menu_screen.show();
    menu_screen.update_info(&stub_weight_sensor, /*uptime_ms=*/259200000UL, /*free_heap=*/168 * 1024);
    menu_screen.update_diagnostics(&stub_weight_sensor);
    menu_screen.refresh_statistics(/*show_overlay=*/false);
    menu_screen.hide();

    confirm_screen.create();
    add_click(confirm_screen.get_confirm_button(), [](lv_event_t*) {
        if (pending_confirm_action) pending_confirm_action(); else show_sim_notice(pending_notice);
    });
    add_click(confirm_screen.get_cancel_button(), [](lv_event_t*) { switch_to(SimState::MENU); });

    calibration_screen.create();
    add_click(calibration_screen.get_ok_button(), calibration_ok_event);
    add_click(calibration_screen.get_cancel_button(), [](lv_event_t*) { switch_to(SimState::MENU); });

    autotune_screen.create();
    add_click(autotune_screen.get_ok_button(), autotune_close);
    add_click(autotune_screen.get_cancel_button(), autotune_close);

    learn_screen.create();

    create_grind_button();
    create_sim_notice();

    // The real nav bar, created last so it draws over every screen
    nav_owner.bluetooth_manager = &stub_bluetooth;
    static StatusIndicatorController status_indicator(&nav_owner);
    nav_bar = &status_indicator;
    nav_bar->build();

    register_navigation_handlers();
    register_settings_handlers();

    lv_obj_add_event_cb(ready_screen.get_tabview(), [](lv_event_t* e) {
        current_tab = static_cast<int>(lv_tabview_get_tab_act(static_cast<lv_obj_t*>(lv_event_get_target(e))));
        ready_screen.update_page_dots(current_tab);
        manual_grinding = false;
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Returning home after a grind (grind button OK/STOP) goes through switch_to(READY)
    lv_obj_add_event_cb(grind_button, [](lv_event_t*) { on_return_to_ready(); refresh_auto_page(); },
                        LV_EVENT_CLICKED, nullptr);

    switch_to(SimState::READY);
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
