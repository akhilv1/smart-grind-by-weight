#pragma once
// Browser stand-in for include/preferences_idf.h (the NVS-backed Preferences).
// Values live in an in-memory map for the life of the page, so toggles,
// sliders and learned portafilters keep their state while you click around.
// Reloading the page starts from defaults again.

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
public:
    bool begin(const char* name, bool /*read_only*/ = false, const char* /*partition*/ = nullptr) {
        namespace_ = name ? name : "";
        return true;
    }
    void end() {}

    bool isKey(const char* key) { return store().count(full_key(key)) > 0; }
    bool remove(const char* key) { return store().erase(full_key(key)) > 0; }
    void clear() {
        const std::string prefix = namespace_ + "/";
        for (auto it = store().begin(); it != store().end();) {
            it = (it->first.rfind(prefix, 0) == 0) ? store().erase(it) : std::next(it);
        }
    }

    bool getBool(const char* key, bool def = false) { return get_value(key, def); }
    int getInt(const char* key, int def = 0) { return get_value(key, def); }
    uint32_t getUInt(const char* key, uint32_t def = 0) { return get_value(key, def); }
    float getFloat(const char* key, float def = 0.0f) { return get_value(key, def); }
    uint64_t getULong64(const char* key, uint64_t def = 0) { return get_value(key, def); }
    std::string getString(const char* key, const char* def = "") {
        auto it = store().find(full_key(key));
        return it == store().end() ? std::string(def) : std::string(it->second.begin(), it->second.end());
    }

    size_t putBool(const char* key, bool value) { return put_value(key, value); }
    size_t putInt(const char* key, int value) { return put_value(key, value); }
    size_t putUInt(const char* key, uint32_t value) { return put_value(key, value); }
    size_t putFloat(const char* key, float value) { return put_value(key, value); }
    size_t putULong64(const char* key, uint64_t value) { return put_value(key, value); }
    size_t putString(const char* key, const char* value) {
        const std::string text = value ? value : "";
        store()[full_key(key)] = std::vector<uint8_t>(text.begin(), text.end());
        return text.size();
    }

    bool putBytes(const char* key, const void* value, size_t len) {
        const auto* bytes = static_cast<const uint8_t*>(value);
        store()[full_key(key)] = std::vector<uint8_t>(bytes, bytes + len);
        return true;
    }
    size_t getBytes(const char* key, void* buf, size_t max_len) {
        auto it = store().find(full_key(key));
        if (it == store().end()) return 0;
        const size_t n = it->second.size() < max_len ? it->second.size() : max_len;
        std::memcpy(buf, it->second.data(), n);
        return n;
    }
    size_t getBytesLength(const char* key) {
        auto it = store().find(full_key(key));
        return it == store().end() ? 0 : it->second.size();
    }

private:
    static std::map<std::string, std::vector<uint8_t>>& store() {
        static std::map<std::string, std::vector<uint8_t>> values;
        return values;
    }
    std::string full_key(const char* key) const { return namespace_ + "/" + (key ? key : ""); }

    template <typename T>
    T get_value(const char* key, T def) {
        auto it = store().find(full_key(key));
        if (it == store().end() || it->second.size() != sizeof(T)) return def;
        T value;
        std::memcpy(&value, it->second.data(), sizeof(T));
        return value;
    }
    template <typename T>
    size_t put_value(const char* key, T value) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
        store()[full_key(key)] = std::vector<uint8_t>(bytes, bytes + sizeof(T));
        return sizeof(T);
    }

    std::string namespace_;
};
