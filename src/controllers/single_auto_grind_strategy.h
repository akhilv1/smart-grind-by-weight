#pragma once

#include "grind_strategy.h"

// Single-dose "Auto" mode: the grinder runs continuously and stops a few
// seconds after the flow rate drops to ~0 (i.e. the dosed beans run out).
class SingleAutoGrindStrategy : public IGrindStrategy {
public:
    SingleAutoGrindStrategy() = default;

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

    const char* name() const override { return "Auto"; }

private:
    bool flow_armed_ = false;             // Flow ramped above the arm threshold at least once
    unsigned long low_flow_start_ms_ = 0; // When flow first dropped below the stop threshold
};
