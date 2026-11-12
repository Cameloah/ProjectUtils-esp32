#pragma once

#include <Arduino.h>
#include <esp_err.h>
#include <mutex>
#include <nvs.h>
#include <vector>

namespace pu {

class RamLog;

/// Typed key-value settings persisted in the ESP32's non-volatile storage (NVS).
///
/// Usage: begin() with a namespace, define() every key with its default, loadAll() once,
/// then get*/set(). Getters return values, never pointers; a missing key or wrong type yields
/// the fallback. Safe to use from several tasks.
class Settings {
  public:
    enum class Type : uint8_t { Int, Float, Double, String, Bool, Blob };

    /// nvsNamespace: at most 15 characters. log is optional.
    esp_err_t begin(const char* nvsNamespace, RamLog* log = nullptr);

    // Declare a key with its type and default value. Keys are at most 15 characters.
    esp_err_t define(const char* key, int value);
    esp_err_t define(const char* key, float value);
    esp_err_t define(const char* key, double value);
    esp_err_t define(const char* key, bool value);
    esp_err_t define(const char* key, const char* value);
    esp_err_t define(const char* key, const String& value);
    esp_err_t defineBlob(const char* key, const uint8_t* data, size_t size);

    bool contains(const char* key) const;

    int32_t getInt(const char* key, int32_t fallback = 0) const;
    float getFloat(const char* key, float fallback = 0.0F) const;
    double getDouble(const char* key, double fallback = 0.0) const;
    bool getBool(const char* key, bool fallback = false) const;
    String getString(const char* key, const String& fallback = String()) const;
    /// Copies at most `capacity` bytes into `out`; returns the stored blob size.
    size_t getBlob(const char* key, uint8_t* out, size_t capacity) const;

    // Change a value. With persist = true (default) it is written to flash right away.
    esp_err_t set(const char* key, int value, bool persist = true);
    esp_err_t set(const char* key, float value, bool persist = true);
    esp_err_t set(const char* key, double value, bool persist = true);
    esp_err_t set(const char* key, bool value, bool persist = true);
    esp_err_t set(const char* key, const char* value, bool persist = true);
    esp_err_t set(const char* key, const String& value, bool persist = true);
    esp_err_t setBlob(const char* key, const uint8_t* data, size_t size, bool persist = true);

    esp_err_t save(const char* key);
    esp_err_t saveAll();
    esp_err_t load(const char* key);
    /// Loads every defined key. Keys not yet stored keep their defaults (not an error).
    esp_err_t loadAll();

  private:
    struct Entry {
        String key;
        Type type = Type::Int;
        union {
            int32_t asInt;
            float asFloat;
            double asDouble;
            bool asBool;
        } number = {0};
        String text;
        std::vector<uint8_t> blob;
    };

    Entry* find(const char* key);
    const Entry* find(const char* key) const;
    const Entry* findTyped(const char* key, Type type) const;
    Entry* addEntry(const char* key, Type type);
    Entry* entryForSet(const char* key, Type type, esp_err_t& error);
    esp_err_t finishSet(const Entry& entry, bool persist);
    esp_err_t saveEntry(nvs_handle_t handle, const Entry& entry) const;
    esp_err_t loadEntry(nvs_handle_t handle, Entry& entry) const;
    esp_err_t persistEntry(const Entry& entry) const;
    void report(const char* action, const char* key, esp_err_t error) const;

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    String namespace_;
    RamLog* log_ = nullptr;
};

} // namespace pu
