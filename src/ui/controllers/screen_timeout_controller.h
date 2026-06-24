#pragma once

#include <cstdint>
#include "../screens/nyan_cat_screensaver.h"

class UIManager;

// Implements automatic screen dimming and Nyan Cat screensaver

class ScreenTimeoutController {
public:
    explicit ScreenTimeoutController(UIManager* manager);

    void register_events();
    void update();

private:
    UIManager* ui_manager_;
    bool screen_dimmed_;
    NyanCatScreensaver screensaver_;
    bool screensaver_created_ = false;
};
