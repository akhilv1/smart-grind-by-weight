#include "custom_logo.h"

#include <cstring>
#include <esp_heap_caps.h>
#include "littlefs_idf.h"
#include "../config/constants.h"
#include "../ui/assets/boot_logo.h"

CustomLogo& CustomLogo::instance() {
    static CustomLogo logo;
    return logo;
}

const lv_image_dsc_t* CustomLogo::image() {
    if (changed_.exchange(false)) {
        reload();
    }
    return custom_ ? &dsc_ : &boot_logo;
}

bool CustomLogo::is_custom() {
    image();  // Apply any pending change first
    return custom_;
}

bool CustomLogo::remove() {
    bool ok = true;
    if (LittleFS.exists(LOGO_FILE_PATH)) {
        ok = LittleFS.remove(LOGO_FILE_PATH);
    }
    mark_changed();
    LOG_BLE("[LOGO] Custom logo %s\n", ok ? "removed" : "could not be removed");
    return ok;
}

bool CustomLogo::is_valid_image(const lv_image_header_t& header, size_t file_size) {
    if (header.magic != LV_IMAGE_HEADER_MAGIC || header.cf != LV_COLOR_FORMAT_RGB565A8) {
        return false;
    }
    if (header.w == 0 || header.h == 0 || header.w > HW_DISPLAY_WIDTH_PX || header.h > HW_DISPLAY_HEIGHT_PX) {
        return false;
    }
    // RGB565A8: a color plane (stride x h) followed by an 8-bit alpha plane (w x h)
    const uint32_t stride = header.stride ? header.stride : header.w * 2u;
    const size_t pixel_bytes = static_cast<size_t>(stride) * header.h + static_cast<size_t>(header.w) * header.h;
    return file_size == sizeof(lv_image_header_t) + pixel_bytes;
}

void CustomLogo::release() {
    // Nothing may still be drawing the old image when this runs: the splash and
    // the screensaver overlay request the logo only when they are created. The
    // image cache is disabled (LV_CACHE_DEF_SIZE 0), so no decoded copy outlives it.
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
    dsc_ = {};
    custom_ = false;
}

void CustomLogo::reload() {
    release();

    if (!LittleFS.exists(LOGO_FILE_PATH)) {
        return;
    }
    File file = LittleFS.open(LOGO_FILE_PATH, "r");
    if (!file) {
        LOG_BLE("[LOGO] Could not open %s - using the built-in logo\n", LOGO_FILE_PATH);
        return;
    }

    const size_t size = file.size();
    if (size < sizeof(lv_image_header_t) || size > LOGO_MAX_FILE_BYTES) {
        LOG_BLE("[LOGO] %s has an invalid size (%u bytes) - using the built-in logo\n",
                LOGO_FILE_PATH, static_cast<unsigned>(size));
        file.close();
        return;
    }

    buffer_ = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer_) {
        LOG_BLE("[LOGO] Out of PSRAM for the custom logo - using the built-in logo\n");
        file.close();
        return;
    }
    const size_t read = file.read(buffer_, size);
    file.close();

    lv_image_header_t header;
    std::memcpy(&header, buffer_, sizeof(header));
    if (read != size || !is_valid_image(header, size)) {
        LOG_BLE("[LOGO] %s is not a valid RGB565A8 logo - using the built-in logo\n", LOGO_FILE_PATH);
        release();
        return;
    }

    dsc_.header = header;
    dsc_.data_size = size - sizeof(lv_image_header_t);
    dsc_.data = buffer_ + sizeof(lv_image_header_t);
    custom_ = true;
    LOG_BLE("[LOGO] Loaded custom logo %ux%u\n", static_cast<unsigned>(header.w), static_cast<unsigned>(header.h));
}
