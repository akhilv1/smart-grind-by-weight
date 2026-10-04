#pragma once
// Stand-in for src/hardware/WeightSensor.h (HX711 driver). Only reached via
// MenuScreen::update_info/update_diagnostics's WeightSensor* parameter -
// Task 6 calls these once at startup with static values so the Diagnostics
// page isn't left blank.
class WeightSensor {
public:
    float get_instant_weight() const { return 0.0f; }
    unsigned get_sample_count() const { return 1250; }
    long get_raw_adc_instant() const { return 8192; }
    float get_standard_deviation_g(unsigned) const { return 0.02f; }
    long get_standard_deviation_adc(unsigned) const { return 6; }
    bool noise_level_diagnostic() const { return true; }
    float get_calibration_factor() const { return 1023.4f; }
};
