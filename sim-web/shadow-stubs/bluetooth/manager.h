#pragma once
// Stand-in for src/bluetooth/manager.h in the shadow tree. The real header
// pulls in the ESP32 BLE stack (BLEDevice.h etc.) which doesn't exist for
// a browser build. menu_screen.cpp only ever calls these 3 methods on a
// BluetoothManager* it treats as opaque - see the "Menu stub surface"
// table in docs/superpowers/plans/2026-08-23-browser-lvgl-simulator.md
// for how that was verified exhaustively, not assumed.
class BluetoothManager {
public:
    bool is_enabled() const { return true; }
    bool is_connected() const { return false; }
    unsigned long get_bluetooth_timeout_remaining_ms() const { return 0; }
};
