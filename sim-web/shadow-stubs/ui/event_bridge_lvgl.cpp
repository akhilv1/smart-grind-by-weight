// Stand-in for src/ui/event_bridge_lvgl.cpp. The real header (copied
// unmodified into the shadow tree) is authoritative for EventType and the
// class shape; only the .cpp is replaced, because the real one #includes
// ui_manager.h, which pulls in every screen and controller in the project.
//
// All three members below are the real .cpp's implementation, minus its
// LOG_BLE lines: none of them actually uses UIManager. dispatch_event() only
// null-*checks* the pointer as an "is the UI up yet" guard before reading the
// event type out of the widget's user_data, and handle_event()/
// register_handler() are a plain std::array of std::function indexed by
// EventType. Dropping that one guard is what lets the simulator use the
// genuine event path throughout: menu rows wired with dispatch_event reach
// the handlers main.cpp registers, exactly as menu_controller.cpp's do on the
// device, instead of the UI being decorated with sim-only buttons.
//
// Events with no registered handler fall through silently here, which is the
// normal case for every control this simulator deliberately leaves inert.
#include "event_bridge_lvgl.h"

class UIManager; // never defined, never instantiated - ui_manager stays null
UIManager* EventBridgeLVGL::ui_manager = nullptr;
std::array<EventBridgeLVGL::EventHandler, static_cast<size_t>(EventBridgeLVGL::EventType::COUNT)>
    EventBridgeLVGL::custom_handlers{};

void EventBridgeLVGL::dispatch_event(lv_event_t* e) {
    EventType event_type = static_cast<EventType>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    handle_event(event_type, e);
}

void EventBridgeLVGL::profile_long_press_handler(lv_event_t* e) {
    handle_event(EventType::PROFILE_LONG_PRESS, e);
}

void EventBridgeLVGL::handle_event(EventBridgeLVGL::EventType event_type, lv_event_t* e) {
    size_t index = static_cast<size_t>(event_type);
    if (index < custom_handlers.size()) {
        auto& handler = custom_handlers[index];
        if (handler) {
            handler(e);
            return;
        }
    }
    // Unregistered types fall through silently. On the device that's a logged
    // warning; here it is the normal case for every event except MENU_BACK,
    // since this simulator deliberately wires up no controllers.
}

void EventBridgeLVGL::register_handler(EventBridgeLVGL::EventType event_type, EventHandler handler) {
    custom_handlers[static_cast<size_t>(event_type)] = std::move(handler);
}
