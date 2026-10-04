#pragma once
// Browser stand-in for include/arduino_compat.h. The real header pulls in
// FreeRTOS, esp_timer and GPIO drivers; the screens only use the timing,
// logging and String helpers below. This directory sits ahead of include/ on
// the sim's include path, so a bare #include "arduino_compat.h" lands here.

#include <cstdint>
#include <cstdio>
#include <string>
#include <emscripten.h>

inline uint32_t millis() { return static_cast<uint32_t>(emscripten_get_now()); }
inline uint32_t micros() { return static_cast<uint32_t>(emscripten_get_now() * 1000.0); }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}

struct WebSimSerial {
    void println(const char* message) const { std::puts(message); }
    template <typename... Args>
    void printf(const char* format, Args... args) const { std::printf(format, args...); }
};
inline WebSimSerial Serial;

class _ESP {
public:
    static uint32_t getFreeHeap() { return 168 * 1024; }
    static uint32_t getHeapSize() { return 320 * 1024; }
    static uint32_t getFlashChipSize() { return 16 * 1024 * 1024; }
    static uint32_t getCpuFreqMHz() { return 240; }
    static void restart() {}
};
inline _ESP ESP;

using String = std::string;
