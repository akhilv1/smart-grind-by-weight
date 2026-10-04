#include "logo_upload_handler.h"

#include <cstring>
#include <esp_rom_crc.h>
#include "../config/constants.h"
#include "../system/custom_logo.h"

bool LogoUploadHandler::start(uint32_t size, uint32_t crc32) {
    abort();  // A new START always replaces a stale partial upload

    if (size <= sizeof(lv_image_header_t) || size > LOGO_MAX_FILE_BYTES) {
        LOG_BLE("[LOGO] Upload rejected: size %lu out of range\n", static_cast<unsigned long>(size));
        return false;
    }

    temp_file_ = LittleFS.open(LOGO_TEMP_FILE_PATH, "w");
    if (!temp_file_) {
        LOG_BLE("[LOGO] Upload rejected: cannot create %s\n", LOGO_TEMP_FILE_PATH);
        return false;
    }

    expected_size_ = size;
    expected_crc_ = crc32;
    received_ = 0;
    crc_ = 0;
    active_ = true;
    LOG_BLE("[LOGO] Upload started (%lu bytes)\n", static_cast<unsigned long>(size));
    return true;
}

bool LogoUploadHandler::append(const uint8_t* data, size_t size) {
    if (!active_) {
        return false;
    }
    if (received_ + size > expected_size_) {
        LOG_BLE("[LOGO] Upload failed: more data than announced\n");
        abort();
        return false;
    }
    if (temp_file_.write(data, size) != size) {
        LOG_BLE("[LOGO] Upload failed: write error (filesystem full?)\n");
        abort();
        return false;
    }
    crc_ = esp_rom_crc32_le(crc_, data, size);
    received_ += size;
    return true;
}

bool LogoUploadHandler::finish() {
    if (!active_) {
        return false;
    }
    temp_file_.close();
    active_ = false;

    if (received_ != expected_size_) {
        LOG_BLE("[LOGO] Upload failed: got %lu of %lu bytes\n", static_cast<unsigned long>(received_),
                static_cast<unsigned long>(expected_size_));
        discard_temp();
        return false;
    }
    if (crc_ != expected_crc_) {
        LOG_BLE("[LOGO] Upload failed: CRC mismatch (0x%08lx vs 0x%08lx)\n", static_cast<unsigned long>(crc_),
                static_cast<unsigned long>(expected_crc_));
        discard_temp();
        return false;
    }

    // Check the image header before it can replace the installed logo
    lv_image_header_t header;
    File check = LittleFS.open(LOGO_TEMP_FILE_PATH, "r");
    const bool header_read = check && check.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header);
    check.close();
    if (!header_read || !CustomLogo::is_valid_image(header, received_)) {
        LOG_BLE("[LOGO] Upload failed: not an RGB565A8 LVGL image within %dx%d\n", HW_DISPLAY_WIDTH_PX,
                HW_DISPLAY_HEIGHT_PX);
        discard_temp();
        return false;
    }

    if (LittleFS.exists(LOGO_FILE_PATH)) {
        LittleFS.remove(LOGO_FILE_PATH);
    }
    if (!LittleFS.rename(LOGO_TEMP_FILE_PATH, LOGO_FILE_PATH)) {
        LOG_BLE("[LOGO] Upload failed: could not install %s\n", LOGO_FILE_PATH);
        discard_temp();
        return false;
    }

    CustomLogo::instance().mark_changed();
    LOG_BLE("[LOGO] Installed custom logo %ux%u (%lu bytes)\n", static_cast<unsigned>(header.w),
            static_cast<unsigned>(header.h), static_cast<unsigned long>(received_));
    return true;
}

void LogoUploadHandler::abort() {
    if (active_) {
        temp_file_.close();
        active_ = false;
        LOG_BLE("[LOGO] Upload aborted\n");
    }
    discard_temp();
}

void LogoUploadHandler::discard_temp() {
    if (LittleFS.exists(LOGO_TEMP_FILE_PATH)) {
        LittleFS.remove(LOGO_TEMP_FILE_PATH);
    }
}
