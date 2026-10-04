#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <lvgl.h>

// The logo shown on the boot splash and by the screensaver.
//
// A personal logo can be uploaded over BLE (see LOGO_* in config/bluetooth.h);
// it lives on LittleFS, so OTA updates never overwrite it and it never needs to
// be in the repository. Without one, or if the file is damaged, the built-in
// logo compiled into the firmware is used.
//
// image() and the loading run on the UI task. The BLE task only calls
// mark_changed() after installing or deleting the file; the UI picks up the new
// logo the next time it asks for it.
class CustomLogo {
public:
    static CustomLogo& instance();

    // Logo to display: the uploaded one if valid, otherwise the built-in logo.
    const lv_image_dsc_t* image();

    // True when an uploaded logo is installed (file present and valid).
    bool is_custom();

    // Delete the uploaded logo (back to the built-in one).
    bool remove();

    // Called after the file changed (any task); the next image() reloads.
    void mark_changed() { changed_.store(true); }

    // Checks an LVGL v9 binary image header against the file size: RGB565A8, at
    // most the panel size, and exactly header + pixel data long.
    static bool is_valid_image(const lv_image_header_t& header, size_t file_size);

private:
    CustomLogo() = default;
    void reload();
    void release();

    std::atomic<bool> changed_{true};  // Load lazily on first use
    uint8_t* buffer_ = nullptr;        // Whole file in PSRAM (header + pixels)
    lv_image_dsc_t dsc_ = {};
    bool custom_ = false;
};
