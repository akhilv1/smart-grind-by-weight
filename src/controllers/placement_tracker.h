#pragma once
#include <cstdint>

class WeightSensor;

// Detects an object (portafilter setup) being placed on and lifted off the scale,
// and measures its weight as a step: settled weight after placement minus the
// settled empty-scale baseline before it. Measuring the step makes the result
// independent of wherever the last tare left the zero point.
//
// Call update() every UI tick; it reports transitions as events.
class PlacementTracker {
public:
    enum class Event {
        NONE,
        PLACED,    // Weight stepped up by at least USER_PF_PLACEMENT_DELTA_G; settling
        SETTLED,   // Placement settled; step_g() holds the measured weight
        LIFTED     // The object was removed (or lifted before settling)
    };

    // Forget everything and wait for a settled empty-scale baseline
    void reset();
    // Something already sits on the scale and currently reads level_g; report its removal
    void assume_occupied(float level_g);

    Event update(WeightSensor& sensor);

    bool is_settling() const { return state_ == State::SETTLING; }
    bool is_holding() const { return state_ == State::HOLDING; }
    bool has_baseline() const { return baseline_valid_; }
    float step_g() const { return step_g_; }

private:
    enum class State {
        EMPTY,     // Tracking the empty-scale baseline
        SETTLING,  // Stepped up, waiting to settle
        HOLDING    // Object resting on the scale
    };

    State state_ = State::EMPTY;
    bool baseline_valid_ = false;
    float baseline_g_ = 0.0f;
    float hold_level_g_ = 0.0f;
    float step_g_ = 0.0f;
};
