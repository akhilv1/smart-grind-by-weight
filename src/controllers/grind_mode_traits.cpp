#include "grind_mode_traits.h"

#include "profile_controller.h"
#include "../config/constants.h"
#include <cstdio>

namespace {

constexpr GrindModeTraits kModeTraits[] = {
    {
        "Weight",
        "g",
        "Target: ",
        " / ",
        USER_FINE_WEIGHT_ADJUSTMENT_G
    },
    {
        "Time",
        "s",
        "Time: ",
        "Time: ",
        USER_FINE_TIME_ADJUSTMENT_S
    },
    {
        "Hybrid",
        "g",            // Primary target is weight
        "Target: ",
        " / ",
        USER_FINE_WEIGHT_ADJUSTMENT_G   // Edit in weight increments
    },
    {
        "Manual",       // Hold-to-grind; no target
        "g",
        "Target: ",
        " / ",
        USER_FINE_WEIGHT_ADJUSTMENT_G
    },
    {
        "Auto",         // Single-dose auto-stop; no target
        "g",
        "Target: ",
        " / ",
        USER_FINE_WEIGHT_ADJUSTMENT_G
    }
};

constexpr int to_index(GrindMode mode) {
    return static_cast<int>(mode);
}

} // namespace

const GrindModeTraits& get_grind_mode_traits(GrindMode mode) {
    int index = to_index(mode);
    if (index < 0 || index >= static_cast<int>(sizeof(kModeTraits) / sizeof(kModeTraits[0]))) {
        return kModeTraits[0];
    }
    return kModeTraits[index];
}

int grind_modes_for_feed(FeedMode feed, GrindMode out[5]) {
    if (feed == FeedMode::SINGLE) {
        out[0] = GrindMode::SINGLE_AUTO;
        out[1] = GrindMode::MANUAL;
        return 2;
    }
    // HOPPER (default)
    out[0] = GrindMode::WEIGHT;
    out[1] = GrindMode::TIME;
    out[2] = GrindMode::CALIBRATED_TIME;
    out[3] = GrindMode::MANUAL;
    return 4;
}

uint32_t grind_mode_color(GrindMode mode) {
    switch (mode) {
        case GrindMode::TIME:            return THEME_COLOR_ACCENT;   // blue
        case GrindMode::CALIBRATED_TIME: return THEME_COLOR_SUCCESS;  // green (Hybrid)
        case GrindMode::MANUAL:          return THEME_COLOR_WARNING;  // orange
        case GrindMode::SINGLE_AUTO:     return THEME_COLOR_ACCENT;   // blue (Single/Auto)
        case GrindMode::WEIGHT:
        default:                         return THEME_COLOR_PRIMARY;  // red
    }
}

float get_profile_target(const ProfileController& profiles, GrindMode mode, int index) {
    if (mode == GrindMode::TIME) {
        return profiles.get_profile_time(index);
    }
    // WEIGHT and CALIBRATED_TIME both use the weight field
    return profiles.get_profile_weight(index);
}

void set_profile_target(ProfileController& profiles, GrindMode mode, int index, float value) {
    if (mode == GrindMode::TIME) {
        profiles.set_profile_time(index, value);
        return;
    }
    // WEIGHT and CALIBRATED_TIME both set the weight field
    profiles.set_profile_weight(index, value);
}

float get_current_profile_target(const ProfileController& profiles, GrindMode mode) {
    if (mode == GrindMode::TIME) {
        return profiles.get_current_time();
    }
    return profiles.get_current_weight();
}

void update_current_profile_target(ProfileController& profiles, GrindMode mode, float value) {
    if (mode == GrindMode::TIME) {
        profiles.update_current_time(value);
        return;
    }
    profiles.update_current_weight(value);
}

float clamp_profile_target(const ProfileController& profiles, GrindMode mode, float value) {
    if (mode == GrindMode::TIME) {
        return profiles.clamp_time(value);
    }
    return profiles.clamp_weight(value);
}

void format_ready_value(char* buffer, std::size_t buffer_len, GrindMode mode, float value) {
    if (!buffer || buffer_len == 0) {
        return;
    }
    if (mode == GrindMode::TIME) {
        std::snprintf(buffer, buffer_len, "%.1fs", value);
        return;
    }
    // WEIGHT and CALIBRATED_TIME show weight format
    std::snprintf(buffer, buffer_len, SYS_WEIGHT_DISPLAY_FORMAT, value);
}
