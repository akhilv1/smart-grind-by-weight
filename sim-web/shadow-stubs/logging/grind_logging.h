#pragma once
#include <cstdint>
class GrindLogger {
public:
    uint32_t get_total_flash_sessions() const { return 24; }
    uint32_t count_total_events_in_flash() const { return 96; }
    uint32_t count_total_measurements_in_flash() const { return 18400; }
};
inline GrindLogger grind_logger;
