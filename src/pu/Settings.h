#pragma once

#include <Arduino.h>
#include <esp_err.h>
#include <functional>
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

    /// A copy of one defined entry, for listing (the memory page).
    struct EntryView {
        String key;
        Type type = Type::Int;
        bool secret = false;
        double value = 0.0; ///< Int, Float, Double, Bool (0/1)
        double defaultValue = 0.0;
        String text; ///< String entries; empty for secrets
        String defaultText;
        bool textSet = false; ///< String entries: value not empty (also for secrets)
        std::vector<uint8_t> blob;
    };

    Settings() = default;
    ~Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    /// Every Settings object that was started with begin(), in that order.
    static std::vector<Settings*> all();
    /// The started Settings object using `nvsNamespace`, or nullptr.
    static Settings* forNamespace(const char* nvsNamespace);

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
    /// Stores the type of `key` in `type`; returns false if the key is not defined.
    bool typeOf(const char* key, Type& type) const;

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

    /// Removes the stored value of `key` from flash; the default from define() applies again.
    esp_err_t reset(const char* key);
    /// Marks `key` as secret: the memory page never shows its value.
    void markSecret(const char* key);
    bool isSecret(const char* key) const;
    /// Calls `visit` with a copy of every defined entry, in the order they were defined.
    void forEach(const std::function<void(const EntryView&)>& visit) const;
    const String& nvsNamespace() const { return namespace_; }

    esp_err_t save(const char* key);
    esp_err_t saveAll();
    esp_err_t load(const char* key);
    /// Loads every defined key. Keys not yet stored keep their defaults (not an error).
    esp_err_t loadAll();

  private:
    union Number {
        int32_t asInt;
        float asFloat;
        double asDouble;
        bool asBool;
    };

    struct Entry {
        String key;
        Type type = Type::Int;
        bool secret = false;
        Number number = {0};
        String text;
        std::vector<uint8_t> blob;
        Number defaultNumber = {0};
        String defaultText;
        std::vector<uint8_t> defaultBlob;
    };

    static void keepDefault(Entry& entry);
    static double numberOf(Type type, const Number& number);

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
