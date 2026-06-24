#include "single_auto_grind_strategy.h"
#include "../config/constants.h"
#include "grind_controller.h"
#include "arduino_compat.h"

void SingleAutoGrindStrategy::on_enter(const GrindSessionDescriptor&,
                                       GrindStrategyContext& context,
                                       const GrindLoopData&) {
    flow_armed_ = false;
    low_flow_start_ms_ = 0;
    if (context.controller) {
        context.controller->time_grind_start_ms = 0;
    }
}

bool SingleAutoGrindStrategy::update(const GrindSessionDescriptor& session,
                                     GrindStrategyContext& context,
                                     const GrindLoopData& loop_data) {
    auto* controller = context.controller;
    if (!controller || session.mode != GrindMode::SINGLE_AUTO) return false;
    if (controller->phase != GrindPhase::TIME_GRINDING) return false;

    if (controller->time_grind_start_ms == 0) {
        controller->time_grind_start_ms = loop_data.now;
    }

    const float flow = loop_data.flow_rate;

    // Don't arm stop-detection until the grind has actually started flowing,
    // otherwise the initial zero flow would stop it immediately.
    if (flow > GRIND_SINGLE_DOSE_MIN_FLOW_ARMED_GPS) {
        flow_armed_ = true;
    }

    if (flow_armed_) {
        if (flow <= GRIND_SINGLE_DOSE_FLOW_THRESHOLD_GPS) {
            if (low_flow_start_ms_ == 0) {
                low_flow_start_ms_ = loop_data.now;
            } else if (loop_data.now - low_flow_start_ms_ >= GRIND_SINGLE_DOSE_STOP_DELAY_MS) {
                controller->grinder->stop();
                controller->switch_phase(GrindPhase::FINAL_SETTLING, loop_data);
            }
        } else {
            // Flow recovered; reset the stop timer.
            low_flow_start_ms_ = 0;
        }
    }
    return true;
}

void SingleAutoGrindStrategy::on_exit(const GrindSessionDescriptor&,
                                      GrindStrategyContext& context) {
    flow_armed_ = false;
    low_flow_start_ms_ = 0;
    if (context.controller) {
        context.controller->time_grind_start_ms = 0;
    }
}

int SingleAutoGrindStrategy::progress_percent(const GrindSessionDescriptor&,
                                              const GrindController&) const {
    // No target weight/time; progress is indeterminate.
    return 0;
}
