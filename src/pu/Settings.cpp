#include "Settings.h"

#include <cstring>
#include <nvs_flash.h>

#include "RamLog.h"

namespace pu {

namespace {
const size_t kMaxKeyLength = 15; // NVS limit for keys and namespace names
using Lock = std::lock_guard<std::mutex>;

std::mutex& registryMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<Settings*>& registry() {
    static std::vector<Settings*> started;
    return started;
}
} // namespace

Settings::~Settings() {
    std::lock_guard<std::mutex> lock(registryMutex());
    std::vector<Settings*>& started = registry();
    for (auto it = started.begin(); it != started.end(); ++it) {
        if (*it == this) {
            started.erase(it);
            break;
        }
    }
}

std::vector<Settings*> Settings::all() {
    std::lock_guard<std::mutex> lock(registryMutex());
    return registry();
}

Settings* Settings::forNamespace(const char* nvsNamespace) {
    std::lock_guard<std::mutex> lock(registryMutex());
    for (Settings* settings : registry()) {
        if (settings->namespace_ == nvsNamespace) {
            return settings;
        }
    }
    return nullptr;
}

esp_err_t Settings::begin(const char* nvsNamespace, RamLog* log) {
    Lock lock(mutex_);
    log_ = log;
    if (nvsNamespace == nullptr || strlen(nvsNamespace) == 0 ||
        strlen(nvsNamespace) > kMaxKeyLength) {
        report("begin", nvsNamespace != nullptr ? nvsNamespace : "", ESP_ERR_INVALID_ARG);
        return ESP_ERR_INVALID_ARG;
    }
    namespace_ = nvsNamespace;
    {
        std::lock_guard<std::mutex> registryLock(registryMutex());
        bool known = false;
        for (const Settings* settings : registry()) {
            known = known || settings == this;
        }
        if (!known) {
            registry().push_back(this);
        }
    }
    // Safe to call repeatedly; the Arduino core has usually initialised NVS already.
    const esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        report("init", nvsNamespace, error);
    }
    return error;
}

// ---------- lookup helpers (caller holds the mutex) ----------

Settings::Entry* Settings::find(const char* key) {
    for (Entry& entry : entries_) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

const Settings::Entry* Settings::find(const char* key) const {
    for (const Entry& entry : entries_) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

const Settings::Entry* Settings::findTyped(const char* key, Type type) const {
    const Entry* entry = find(key);
    if (entry == nullptr) {
        report("get", key, ESP_ERR_NOT_FOUND);
        return nullptr;
    }
    if (entry->type != type) {
        report("get", key, ESP_ERR_INVALID_ARG);
        return nullptr;
    }
    return entry;
}

Settings::Entry* Settings::addEntry(const char* key, Type type) {
    if (key == nullptr || strlen(key) == 0 || strlen(key) > kMaxKeyLength || find(key) != nullptr) {
        report("define", key != nullptr ? key : "", ESP_ERR_INVALID_ARG);
        return nullptr;
    }
    entries_.emplace_back();
    Entry& entry = entries_.back();
    entry.key = key;
    entry.type = type;
    return &entry;
}

Settings::Entry* Settings::entryForSet(const char* key, Type type, esp_err_t& error) {
    Entry* entry = find(key);
    if (entry == nullptr) {
        error = ESP_ERR_NOT_FOUND;
    } else if (entry->type != type) {
        error = ESP_ERR_INVALID_ARG;
        entry = nullptr;
    } else {
        error = ESP_OK;
    }
    if (error != ESP_OK) {
        report("set", key, error);
    }
    return entry;
}

esp_err_t Settings::finishSet(const Entry& entry, bool persist) {
    return persist ? persistEntry(entry) : ESP_OK;
}

void Settings::report(const char* action, const char* key, esp_err_t error) const {
    if (log_ != nullptr) {
        log_->error("Settings %s '%s': %s", action, key, esp_err_to_name(error));
    }
}

void Settings::keepDefault(Entry& entry) {
    entry.defaultNumber = entry.number;
    entry.defaultText = entry.text;
    entry.defaultBlob = entry.blob;
}

// ---------- define ----------

esp_err_t Settings::define(const char* key, int value) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::Int);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->number.asInt = value;
    keepDefault(*entry);
    return ESP_OK;
}

esp_err_t Settings::define(const char* key, float value) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::Float);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->number.asFloat = value;
    keepDefault(*entry);
    return ESP_OK;
}

esp_err_t Settings::define(const char* key, double value) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::Double);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->number.asDouble = value;
    keepDefault(*entry);
    return ESP_OK;
}

esp_err_t Settings::define(const char* key, bool value) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::Bool);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->number.asBool = value;
    keepDefault(*entry);
    return ESP_OK;
}

esp_err_t Settings::define(const char* key, const char* value) {
    return define(key, String(value != nullptr ? value : ""));
}

esp_err_t Settings::define(const char* key, const String& value) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::String);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->text = value;
    keepDefault(*entry);
    return ESP_OK;
}

esp_err_t Settings::defineBlob(const char* key, const uint8_t* data, size_t size) {
    Lock lock(mutex_);
    Entry* entry = addEntry(key, Type::Blob);
    if (entry == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data != nullptr) {
        entry->blob.assign(data, data + size);
    }
    keepDefault(*entry);
    return ESP_OK;
}

// ---------- get ----------

bool Settings::contains(const char* key) const {
    Lock lock(mutex_);
    return find(key) != nullptr;
}

bool Settings::typeOf(const char* key, Type& type) const {
    Lock lock(mutex_);
    const Entry* entry = find(key);
    if (entry == nullptr) {
        return false;
    }
    type = entry->type;
    return true;
}

int32_t Settings::getInt(const char* key, int32_t fallback) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::Int);
    return entry != nullptr ? entry->number.asInt : fallback;
}

float Settings::getFloat(const char* key, float fallback) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::Float);
    return entry != nullptr ? entry->number.asFloat : fallback;
}

double Settings::getDouble(const char* key, double fallback) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::Double);
    return entry != nullptr ? entry->number.asDouble : fallback;
}

bool Settings::getBool(const char* key, bool fallback) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::Bool);
    return entry != nullptr ? entry->number.asBool : fallback;
}

String Settings::getString(const char* key, const String& fallback) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::String);
    return entry != nullptr ? entry->text : fallback;
}

size_t Settings::getBlob(const char* key, uint8_t* out, size_t capacity) const {
    Lock lock(mutex_);
    const Entry* entry = findTyped(key, Type::Blob);
    if (entry == nullptr) {
        return 0;
    }
    const size_t count = entry->blob.size() < capacity ? entry->blob.size() : capacity;
    if (out != nullptr && count > 0) {
        memcpy(out, entry->blob.data(), count);
    }
    return entry->blob.size();
}

// ---------- set ----------

esp_err_t Settings::set(const char* key, int value, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::Int, error);
    if (entry == nullptr) {
        return error;
    }
    entry->number.asInt = value;
    return finishSet(*entry, persist);
}

esp_err_t Settings::set(const char* key, float value, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::Float, error);
    if (entry == nullptr) {
        return error;
    }
    entry->number.asFloat = value;
    return finishSet(*entry, persist);
}

esp_err_t Settings::set(const char* key, double value, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::Double, error);
    if (entry == nullptr) {
        return error;
    }
    entry->number.asDouble = value;
    return finishSet(*entry, persist);
}

esp_err_t Settings::set(const char* key, bool value, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::Bool, error);
    if (entry == nullptr) {
        return error;
    }
    entry->number.asBool = value;
    return finishSet(*entry, persist);
}

esp_err_t Settings::set(const char* key, const char* value, bool persist) {
    return set(key, String(value != nullptr ? value : ""), persist);
}

esp_err_t Settings::set(const char* key, const String& value, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::String, error);
    if (entry == nullptr) {
        return error;
    }
    entry->text = value;
    return finishSet(*entry, persist);
}

esp_err_t Settings::setBlob(const char* key, const uint8_t* data, size_t size, bool persist) {
    Lock lock(mutex_);
    esp_err_t error = ESP_OK;
    Entry* entry = entryForSet(key, Type::Blob, error);
    if (entry == nullptr) {
        return error;
    }
    if (data == nullptr) {
        entry->blob.clear();
    } else {
        entry->blob.assign(data, data + size);
    }
    return finishSet(*entry, persist);
}

// ---------- reset, secrets, listing ----------

esp_err_t Settings::reset(const char* key) {
    Lock lock(mutex_);
    Entry* entry = find(key);
    if (entry == nullptr) {
        report("reset", key, ESP_ERR_NOT_FOUND);
        return ESP_ERR_NOT_FOUND;
    }
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = nvs_erase_key(handle, key);
        if (error == ESP_ERR_NVS_NOT_FOUND) {
            error = ESP_OK; // nothing stored: already at the default
        }
        if (error == ESP_OK) {
            error = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (error != ESP_OK) {
        report("reset", key, error);
        return error;
    }
    entry->number = entry->defaultNumber;
    entry->text = entry->defaultText;
    entry->blob = entry->defaultBlob;
    return ESP_OK;
}

void Settings::markSecret(const char* key) {
    Lock lock(mutex_);
    Entry* entry = find(key);
    if (entry != nullptr) {
        entry->secret = true;
    }
}

bool Settings::isSecret(const char* key) const {
    Lock lock(mutex_);
    const Entry* entry = find(key);
    return entry != nullptr && entry->secret;
}

void Settings::forEach(const std::function<void(const EntryView&)>& visit) const {
    std::vector<EntryView> views;
    {
        Lock lock(mutex_);
        for (const Entry& entry : entries_) {
            EntryView view;
            view.key = entry.key;
            view.type = entry.type;
            view.secret = entry.secret;
            view.value = numberOf(entry.type, entry.number);
            view.defaultValue = numberOf(entry.type, entry.defaultNumber);
            view.text = entry.secret ? String() : entry.text;
            view.defaultText = entry.secret ? String() : entry.defaultText;
            view.textSet = !entry.text.isEmpty();
            view.blob = entry.blob;
            views.push_back(view);
        }
    }
    for (const EntryView& view : views) { // outside the lock: visit may call back into us
        visit(view);
    }
}

double Settings::numberOf(Type type, const Number& number) {
    switch (type) {
    case Type::Int:
        return number.asInt;
    case Type::Float:
        return number.asFloat;
    case Type::Double:
        return number.asDouble;
    case Type::Bool:
        return number.asBool ? 1.0 : 0.0;
    default:
        return 0.0;
    }
}

// ---------- NVS access (caller holds the mutex) ----------

esp_err_t Settings::saveEntry(nvs_handle_t handle, const Entry& entry) const {
    const char* key = entry.key.c_str();
    switch (entry.type) {
    case Type::Int:
        return nvs_set_i32(handle, key, entry.number.asInt);
    case Type::Float:
        return nvs_set_blob(handle, key, &entry.number.asFloat, sizeof(float));
    case Type::Double:
        return nvs_set_blob(handle, key, &entry.number.asDouble, sizeof(double));
    case Type::Bool:
        return nvs_set_u8(handle, key, entry.number.asBool ? 1 : 0);
    case Type::String:
        return nvs_set_str(handle, key, entry.text.c_str());
    case Type::Blob:
        return nvs_set_blob(handle, key, entry.blob.data(), entry.blob.size());
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t Settings::loadEntry(nvs_handle_t handle, Entry& entry) const {
    const char* key = entry.key.c_str();
    switch (entry.type) {
    case Type::Int:
        return nvs_get_i32(handle, key, &entry.number.asInt);
    case Type::Float: {
        float value = 0.0F;
        size_t size = sizeof(value);
        const esp_err_t error = nvs_get_blob(handle, key, &value, &size);
        if (error == ESP_OK) {
            entry.number.asFloat = value;
        }
        return error;
    }
    case Type::Double: {
        double value = 0.0;
        size_t size = sizeof(value);
        const esp_err_t error = nvs_get_blob(handle, key, &value, &size);
        if (error == ESP_OK) {
            entry.number.asDouble = value;
        }
        return error;
    }
    case Type::Bool: {
        uint8_t value = 0;
        const esp_err_t error = nvs_get_u8(handle, key, &value);
        if (error == ESP_OK) {
            entry.number.asBool = value != 0;
        }
        return error;
    }
    case Type::String: {
        size_t size = 0;
        esp_err_t error = nvs_get_str(handle, key, nullptr, &size);
        if (error != ESP_OK) {
            return error;
        }
        std::vector<char> buffer(size + 1, '\0');
        error = nvs_get_str(handle, key, buffer.data(), &size);
        if (error == ESP_OK) {
            entry.text = buffer.data();
        }
        return error;
    }
    case Type::Blob: {
        size_t size = 0;
        esp_err_t error = nvs_get_blob(handle, key, nullptr, &size);
        if (error != ESP_OK) {
            return error;
        }
        std::vector<uint8_t> buffer(size);
        error = nvs_get_blob(handle, key, buffer.data(), &size);
        if (error == ESP_OK) {
            entry.blob.swap(buffer);
        }
        return error;
    }
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t Settings::persistEntry(const Entry& entry) const {
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = saveEntry(handle, entry);
        if (error == ESP_OK) {
            error = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (error != ESP_OK) {
        report("save", entry.key.c_str(), error);
    }
    return error;
}

// ---------- save / load ----------

esp_err_t Settings::save(const char* key) {
    Lock lock(mutex_);
    const Entry* entry = find(key);
    if (entry == nullptr) {
        report("save", key, ESP_ERR_NOT_FOUND);
        return ESP_ERR_NOT_FOUND;
    }
    return persistEntry(*entry);
}

esp_err_t Settings::saveAll() {
    Lock lock(mutex_);
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        report("open", namespace_.c_str(), error);
        return error;
    }
    for (const Entry& entry : entries_) {
        error = saveEntry(handle, entry);
        if (error != ESP_OK) {
            report("save", entry.key.c_str(), error);
            break;
        }
    }
    if (error == ESP_OK) {
        error = nvs_commit(handle);
        if (error != ESP_OK) {
            report("commit", namespace_.c_str(), error);
        }
    }
    nvs_close(handle);
    return error;
}

esp_err_t Settings::load(const char* key) {
    Lock lock(mutex_);
    Entry* entry = find(key);
    if (entry == nullptr) {
        report("load", key, ESP_ERR_NOT_FOUND);
        return ESP_ERR_NOT_FOUND;
    }
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        report("open", namespace_.c_str(), error);
        return error;
    }
    error = loadEntry(handle, *entry);
    nvs_close(handle);
    return error;
}

esp_err_t Settings::loadAll() {
    Lock lock(mutex_);
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        report("open", namespace_.c_str(), error);
        return error;
    }
    esp_err_t result = ESP_OK;
    for (Entry& entry : entries_) {
        error = loadEntry(handle, entry);
        if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) {
            report("load", entry.key.c_str(), error);
            result = error;
        }
    }
    nvs_close(handle);
    return result;
}

} // namespace pu
