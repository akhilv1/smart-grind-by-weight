#pragma once
#include <cstdint>
class StatisticsManager {
public:
    uint32_t get_total_grinds() const { return 128; }
    uint32_t get_single_shots() const { return 40; }
    uint32_t get_double_shots() const { return 76; }
    uint32_t get_custom_shots() const { return 12; }
    uint64_t get_motor_runtime_ms() const { return 3600000; }
    uint32_t get_device_uptime_hrs() const { return 72; }
    uint32_t get_device_uptime_min_remainder() const { return 15; }
    float get_total_weight_kg() const { return 1.8f; }
    uint32_t get_weight_mode_grinds() const { return 110; }
    uint32_t get_time_mode_grinds() const { return 18; }
    float get_avg_accuracy_g() const { return 0.08f; }
    uint32_t get_total_pulses() const { return 96; }
    float get_avg_pulses() const { return 0.75f; }
};
inline StatisticsManager statistics_manager;
