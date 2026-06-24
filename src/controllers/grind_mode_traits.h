#pragma once

#include "grind_mode.h"
#include <cstddef>
#include <cstdint>

class ProfileController;

struct GrindModeTraits {
    const char* name;                // Human readable mode name
    const char* ready_unit_suffix;   // Display suffix for ready/edit screens
    const char* arc_prefix;          // Prefix for arc screen target label
    const char* chart_label;         // Label used in chart screen secondary line
    float fine_increment;            // Fine adjustment for jog/edit controls
};

const GrindModeTraits& get_grind_mode_traits(GrindMode mode);

// Theme color (RGB hex) representing each grind mode:
// Weight = red, Time = blue, Hybrid (calibrated time) = green.
uint32_t grind_mode_color(GrindMode mode);

// Fills `out` with the ordered grind modes available for the given feed mode and
// returns the count. Hopper -> {WEIGHT, TIME, CALIBRATED_TIME, MANUAL} (4),
// Single -> {SINGLE_AUTO, MANUAL} (2). `out` must hold at least 5 elements.
int grind_modes_for_feed(FeedMode feed, GrindMode out[5]);

float get_profile_target(const ProfileController& profiles, GrindMode mode, int index);
void set_profile_target(ProfileController& profiles, GrindMode mode, int index, float value);
float get_current_profile_target(const ProfileController& profiles, GrindMode mode);
void update_current_profile_target(ProfileController& profiles, GrindMode mode, float value);
float clamp_profile_target(const ProfileController& profiles, GrindMode mode, float value);

void format_ready_value(char* buffer, std::size_t buffer_len, GrindMode mode, float value);
