#pragma once
#include <Arduino.h> // for the Preferences shim (angle-bracket: resolves via
                      // the "platform" -I dir regardless of which directory
                      // this stub is compiled from - see CMakeLists.txt's
                      // shadow-stub-mirror copy step)
#include "WeightSensor.h" // real hardware_manager.h also pulls this in; menu_screen.h
                          // only forward-declares WeightSensor, but menu_screen.cpp
                          // calls methods on a WeightSensor* and needs the full type

// Stand-in for src/hardware/hardware_manager.h - the real header owns real
// DisplayManager/WeightSensor/Grinder value members (SPI/I2C/PWM drivers).
// menu_screen.cpp only calls get_preferences() on the pointer.
class HardwareManager {
public:
    Preferences* get_preferences() { return &preferences_; }
private:
    Preferences preferences_;
};
