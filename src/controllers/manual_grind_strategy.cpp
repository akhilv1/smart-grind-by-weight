#include "manual_grind_strategy.h"
#include "../config/constants.h"
#include "grind_controller.h"
#include "arduino_compat.h"

void ManualGrindStrategy::on_enter(const GrindSessionDescriptor&,
                                   GrindStrategyContext& context,
                                   const GrindLoopData&) {
    if (context.controller) {
        context.controller->time_grind_start_ms = 0;
    }
}

bool ManualGrindStrategy::update(const GrindSessionDescriptor& session,
                                 GrindStrategyContext& context,
                                 const GrindLoopData& loop_data) {
    auto* controller = context.controller;
    if (!controller || session.mode != GrindMode::MANUAL) return false;
    if (controller->phase != GrindPhase::MANUAL_GRINDING) return false;

    if (controller->time_grind_start_ms == 0) {
        controller->time_grind_start_ms = loop_data.now;
    }

    // Safety cap: stop after the maximum manual duration regardless of button.
    bool exceeded = (loop_data.now - controller->time_grind_start_ms) >= GRIND_MANUAL_MAX_DURATION_MS;

    if (!controller->is_manual_button_held() || exceeded) {
        controller->grinder->stop();
        controller->switch_phase(GrindPhase::FINAL_SETTLING, loop_data);
        return true;
    }

    // Button held: keep the motor running.
    if (!controller->grinder->is_grinding()) {
        controller->grinder->start();
    }
    return true;
}

void ManualGrindStrategy::on_exit(const GrindSessionDescriptor&,
                                  GrindStrategyContext& context) {
    if (context.controller) {
        context.controller->time_grind_start_ms = 0;
    }
}

int ManualGrindStrategy::progress_percent(const GrindSessionDescriptor&,
                                          const GrindController&) const {
    // No target; progress is indeterminate.
    return 0;
}
