#pragma once

#include "grind_strategy.h"

// Manual "hold-to-grind" mode: after taring, the motor runs only while the
// user holds the grind button. Releasing stops the motor and completes the
// grind. A safety cap stops the motor after GRIND_MANUAL_MAX_DURATION_MS.
class ManualGrindStrategy : public IGrindStrategy {
public:
    ManualGrindStrategy() = default;

    void on_enter(const GrindSessionDescriptor& session,
                  GrindStrategyContext& context,
                  const GrindLoopData& loop_data) override;

    bool update(const GrindSessionDescriptor& session,
                GrindStrategyContext& context,
                const GrindLoopData& loop_data) override;

    void on_exit(const GrindSessionDescriptor& session,
                 GrindStrategyContext& context) override;

    int progress_percent(const GrindSessionDescriptor& session,
                         const GrindController& controller) const override;

    const char* name() const override { return "Manual"; }
};
