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

// Tare-independent grams: undo the zero offset (weight = (raw - offset) / cal)
static float zero_offset_g(WeightSensor& sensor) {
    const float cal = sensor.get_calibration_factor();
    return (cal > 1e-6f || cal < -1e-6f) ? static_cast<float>(sensor.get_zero_offset()) / cal : 0.0f;
}

bool PlacementTracker::resume(WeightSensor& sensor) {
    const float weight = sensor.get_weight_low_latency() + zero_offset_g(sensor);
    if (baseline_valid_ && (weight - baseline_g_) < kLiftThresholdG) {
        state_ = State::EMPTY;
        return false;
    }
    if (!baseline_valid_) {
        // No known empty level: treat whatever is there as empty and wait for a placement
        state_ = State::EMPTY;
        return false;
    }
    state_ = State::HOLDING;
    hold_level_g_ = weight;
    return true;
}

PlacementTracker::Event PlacementTracker::update(WeightSensor& sensor) {
    if (!sensor.data_ready() || sensor.is_tare_in_progress()) {
        return Event::NONE;
    }

    const float offset_g = zero_offset_g(sensor);
    const float weight = sensor.get_weight_low_latency() + offset_g;
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
                baseline_g_ = sensor.get_weight_high_latency() + offset_g;
                baseline_valid_ = true;
            }
            return Event::NONE;

        case State::SETTLING:
            if ((weight - baseline_g_) < kLiftThresholdG) {
                state_ = State::EMPTY;
                return Event::LIFTED;
            }
            if (settled) {
                hold_level_g_ = sensor.get_weight_high_latency() + offset_g;
                step_g_ = hold_level_g_ - baseline_g_;
                state_ = State::HOLDING;
                return Event::SETTLED;
            }
            return Event::NONE;

        case State::HOLDING:
            if (weight < hold_level_g_ - kLiftThresholdG) {
                // Keep the known empty level so the next placement is caught right
                // away, even before the scale settles; it refreshes once it does
                state_ = State::EMPTY;
                return Event::LIFTED;
            }
            if (settled) {
                hold_level_g_ = sensor.get_weight_high_latency() + offset_g;
            }
            return Event::NONE;
    }
    return Event::NONE;
}
