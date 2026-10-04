#pragma once
// Browser stand-in for src/ui/ui_manager.h, used only by the real
// status_indicator_controller.cpp (the global nav bar). That file reads exactly
// these two members; the real UIManager would drag in every controller and the
// whole grind path. Mirrored into the shadow tree so "../ui_manager.h" lands here.
#include <memory>
#include "../bluetooth/manager.h"
#include "../system/diagnostics_controller.h"

class UIManager {
public:
    BluetoothManager* bluetooth_manager = nullptr;
    std::unique_ptr<DiagnosticsController> diagnostics_controller_;
};
