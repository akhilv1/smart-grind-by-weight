#pragma once
#include <cstddef>

// Stand-in for src/controllers/autotune_controller.h. The real header pulls in
// <LittleFS.h>, hardware/grinder.h and grind_controller.h - the whole ESP32
// grind path - but autotune_screen.cpp only ever touches AutoTuneProgress, and
// of that only `has_new_message` and `last_message` (see its update_progress()).
// The struct and enum below are copied field-for-field from the real header so
// the screen compiles against the same layout it would on the device; the
// AutoTuneController class itself is never referenced by the screen and is
// deliberately absent.
enum class AutoTunePhase {
    IDLE,
    PRIMING,
    BINARY_SEARCH,
    VERIFICATION,
    COMPLETE_SUCCESS,
    COMPLETE_FAILURE
};

struct AutoTuneProgress {
    AutoTunePhase phase;
    int iteration;
    float current_pulse_ms;
    float last_pulse_ms;
    float step_size_ms;
    bool last_pulse_success;
    int verification_round;
    int verification_success_count;
    float final_latency_ms;
    float previous_latency_ms;

    // Console message tracking
    char last_message[256];
    bool has_new_message;
};
