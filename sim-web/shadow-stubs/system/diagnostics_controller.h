#pragma once
// Stand-in for src/system/diagnostics_controller.h - only needed here
// because the real file's methods aren't defined inline (implemented in
// diagnostics_controller.cpp, which this plan doesn't compile). The
// DiagnosticCode enum values below match the real header exactly.

// The real diagnostics_controller.h forward-declares WeightSensor (it never
// needs the full type, only WeightSensor* parameters) so that menu_screen.h,
// which #includes this header and then uses `const WeightSensor*` /
// `WeightSensor*` parameter types of its own a few lines later, has a name
// to resolve against without needing the full definition at that point.
class WeightSensor;

enum class DiagnosticCode {
    NONE = 0,
    HX711_NOT_CONNECTED,
    HX711_SAMPLE_RATE_INVALID,
    LOAD_CELL_NOT_CALIBRATED,
    LOAD_CELL_NOISY_SUSTAINED,
    MECHANICAL_INSTABILITY,
    TOUCH_NOT_RESPONDING
};
class DiagnosticsController {
public:
    DiagnosticCode get_highest_priority_warning() const { return DiagnosticCode::NONE; }
    const char* get_diagnostic_message(DiagnosticCode) const { return "No active diagnostics"; }
};
