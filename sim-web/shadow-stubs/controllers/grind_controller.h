#pragma once
// Browser stand-in for src/controllers/grind_controller.h. The menu only reads
// the motor latency and the preference keys below; the keys must match the
// real header so Settings reads and writes the same Preferences entries.
class GrindController {
public:
    float get_motor_response_latency() const { return 62.0f; }
    static constexpr const char* PREF_KEY_PRIME_ENABLED = "prime_enabled";
    static constexpr const char* PREF_KEY_GRINDER_MODE = "grinder_mode";
    static constexpr const char* PREF_KEY_GRINDER_AMOUNT_G = "grinder_amount_g";
    static constexpr const char* PREF_KEY_GRIND_FRESHNESS_HOURS = "freshness_hrs";
    static constexpr const char* PREF_KEY_LAST_GRIND_RUNTIME = "last_grind_ms";
    static constexpr const char* PREF_KEY_PULSE_CORRECTIONS = "pulse_corr";
};
