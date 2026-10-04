#pragma once
#include <cstddef>
#include <cstdint>
#include "littlefs_idf.h"

// Logo upload commands on the data control characteristic (see config/bluetooth.h)
enum BLELogoCommand {
    BLE_LOGO_CMD_START = 0x30,   // [0x30][size:u32 LE][crc32:u32 LE]
    BLE_LOGO_CMD_DATA = 0x31,    // [0x31][bytes...]
    BLE_LOGO_CMD_END = 0x32,     // [0x32]
    BLE_LOGO_CMD_ABORT = 0x33,   // [0x33]
    BLE_LOGO_CMD_DELETE = 0x34   // [0x34]
};

// Status bytes sent as data status notifications
enum BLELogoStatus {
    BLE_LOGO_STATUS_READY = 0x31,    // START accepted, send DATA
    BLE_LOGO_STATUS_SUCCESS = 0x33,  // Installed (END) or deleted (DELETE)
    BLE_LOGO_STATUS_ERROR = 0x34     // Rejected; nothing installed
};

// Receives a custom logo over BLE and installs it on LittleFS.
//
// Ported from quickcoffee's screensaver image upload (ImageUploadHandler),
// adapted for the logo: variable size, CRC32 over the whole file, and the LVGL
// image header is validated before the file replaces the installed logo. Data
// goes to a temp file and is only renamed into place once everything checks out,
// so a failed or interrupted upload never leaves a broken logo behind.
class LogoUploadHandler {
public:
    bool start(uint32_t size, uint32_t crc32);
    bool append(const uint8_t* data, size_t size);
    bool finish();
    void abort();
    bool is_active() const { return active_; }

private:
    void discard_temp();

    File temp_file_;
    uint32_t expected_size_ = 0;
    uint32_t expected_crc_ = 0;
    uint32_t received_ = 0;
    uint32_t crc_ = 0;
    bool active_ = false;
};
