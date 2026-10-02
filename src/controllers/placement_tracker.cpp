#include "placement_tracker.h"

#include "../config/constants.h"
#include "../hardware/WeightSensor.h"

// Falling this far below the resting level counts as lifting the object off
static constexpr float kLiftThresholdG = USER_PF_PLACEMENT_DELTA_G * 0.5f;

void PlacementTracker::reset() {
    state_ = State::EMPTY;
    baseline_valid_ = false;
    step_g_ = 0.0f;
}

void PlacementTracker::assume_occupied(float level_g) {
    state_ = State::HOLDING;
    hold_level_g_ = level_g;
}

PlacementTracker::Event PlacementTracker::update(WeightSensor& sensor) {
    if (!sensor.data_ready() || sensor.is_tare_in_progress()) {
        return Event::NONE;
    }

    const float weight = sensor.get_weight_low_latency();
    const bool settled = sensor.is_settled();

    switch (state_) {
        case State::EMPTY:
            // Check for a step before refreshing the baseline, so a fast placement
            // isn't absorbed into the baseline
            if (baseline_valid_ && (weight - baseline_g_) >= USER_PF_PLACEMENT_DELTA_G) {
                state_ = State::SETTLING;
                return Event::PLACED;
            }
            if (settled) {
                baseline_g_ = sensor.get_weight_high_latency();
                baseline_valid_ = true;
            }
            return Event::NONE;

        case State::SETTLING:
            if ((weight - baseline_g_) < kLiftThresholdG) {
                state_ = State::EMPTY;
                return Event::LIFTED;
            }
            if (settled) {
                hold_level_g_ = sensor.get_weight_high_latency();
                step_g_ = hold_level_g_ - baseline_g_;
                state_ = State::HOLDING;
                return Event::SETTLED;
            }
            return Event::NONE;

        case State::HOLDING:
            if (weight < hold_level_g_ - kLiftThresholdG) {
                // Re-establish the empty level before looking for the next placement
                state_ = State::EMPTY;
                baseline_valid_ = false;
                return Event::LIFTED;
            }
            if (settled) {
                hold_level_g_ = sensor.get_weight_high_latency();
            }
            return Event::NONE;
    }
    return Event::NONE;
}
