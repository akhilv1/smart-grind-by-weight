#pragma once
#include <cstdint>

class WeightSensor;

// Detects an object (portafilter setup) being placed on and lifted off the scale,
// and measures its weight as a step: settled weight after placement minus the
// settled empty-scale baseline before it.
//
// All levels are tracked in tare-independent grams (reading + zero offset), so the
// empty-scale baseline survives tares (e.g. the one at grind start). That lets a
// lift re-arm detection immediately: a portafilter put back (or swapped) before the
// scale settles empty is still measured against the known empty level.
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
    // Pick up after a pause (e.g. a grind): resume holding if the object is still on
    // the scale, otherwise go back to waiting for a placement. Returns true if holding.
    bool resume(WeightSensor& sensor);

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
