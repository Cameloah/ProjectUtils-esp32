#include "Memory.h"

#include <cmath>
#include <cstdio>
#include <esp_idf_version.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <vector>

#include "Numbers.h"
#include "Settings.h"

namespace pu {

namespace {
const char kAreasKey[] = "memAreas"; // areas created on the memory page, comma-separated
const size_t kMaxName = 15;          // NVS limit for keys and namespaces
const size_t kMaxText = 1000;
const size_t kBlobPreview = 48; // bytes of a data entry shown as hex

struct Stored {
    Stored(const char* name, nvs_type_t kind) : key(name), type(kind) {}
    String key;
    nvs_type_t type;
};

/// Everything stored in one NVS namespace. The iterator API changed in ESP-IDF 5.
std::vector<Stored> storedIn(const char* area) {
    std::vector<Stored> found;
    nvs_entry_info_t info;
#if ESP_IDF_VERSION_MAJOR >= 5
    nvs_iterator_t it = nullptr;
    esp_err_t error = nvs_entry_find(NVS_DEFAULT_PART_NAME, area, NVS_TYPE_ANY, &it);
    while (error == ESP_OK) {
        nvs_entry_info(it, &info);
        found.emplace_back(info.key, info.type);
        error = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
#else
    nvs_iterator_t it = nvs_entry_find(NVS_DEFAULT_PART_NAME, area, NVS_TYPE_ANY);
    while (it != nullptr) {
        nvs_entry_info(it, &info);
        found.emplace_back(info.key, info.type);
        it = nvs_entry_next(it); // releases the iterator at the end
    }
#endif
    return found;
}

const Stored* findStored(const std::vector<Stored>& stored, const String& key) {
    for (const Stored& entry : stored) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

/// Letters, digits and - _ . only, 1 to 15 characters.
bool isValidName(const String& name) {
    if (name.isEmpty() || name.length() > kMaxName) {
        return false;
    }
    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name[i];
        const bool ok =
            isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return true;
}

/// A float as the user typed it (1.2), not as its nearest double (1.2000000476837158).
double tidy(float value) {
    char text[24];
    snprintf(text, sizeof(text), "%.7g", static_cast<double>(value));
    return strtod(text, nullptr);
}

const char* typeName(Settings::Type type) {
    switch (type) {
    case Settings::Type::Int:
        return "int";
    case Settings::Type::Float:
        return "float";
    case Settings::Type::Double:
        return "double";
    case Settings::Type::Bool:
        return "bool";
    case Settings::Type::String:
        return "text";
    default:
        return "blob";
    }
}

String hex(const std::vector<uint8_t>& bytes) {
    String text;
    const size_t shown = bytes.size() < kBlobPreview ? bytes.size() : kBlobPreview;
    for (size_t i = 0; i < shown; ++i) {
        char pair[4];
        snprintf(pair, sizeof(pair), i == 0 ? "%02x" : " %02x", bytes[i]);
        text += pair;
    }
    return text;
}

bool parseWhole(const String& text, int32_t& out) {
    double number = 0.0;
    if (!parseNumber(text.c_str(), number) || !isWholeNumber(number) || number < INT32_MIN ||
        number > INT32_MAX) {
        return false;
    }
    out = static_cast<int32_t>(number);
    return true;
}

bool parseBool(const String& text, bool& out) {
    if (text == "true") {
        out = true;
        return true;
    }
    if (text == "false") {
        out = false;
        return true;
    }
    return false;
}

/// Opens `area` for writing, runs `write`, commits.
template <typename Write> esp_err_t writeRaw(const String& area, Write write) {
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(area.c_str(), NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        return error;
    }
    error = write(handle);
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return error;
}

/// Describes an entry that no Settings object defines, read straight from NVS.
void describeRaw(nvs_handle_t handle, const Stored& stored, JsonObject entry) {
    const char* key = stored.key.c_str();
    entry["editable"] = false;
    switch (stored.type) {
    case NVS_TYPE_I32: {
        int32_t value = 0;
        nvs_get_i32(handle, key, &value);
        entry["type"] = "int";
        entry["value"] = value;
        entry["editable"] = true;
        return;
    }
    case NVS_TYPE_U8: {
        uint8_t value = 0;
        nvs_get_u8(handle, key, &value);
        if (value <= 1) {
            entry["type"] = "bool";
            entry["value"] = value == 1;
            entry["editable"] = true;
        } else {
            entry["type"] = "other";
            entry["value"] = value;
        }
        return;
    }
    case NVS_TYPE_STR: {
        size_t size = 0;
        nvs_get_str(handle, key, nullptr, &size);
        std::vector<char> text(size + 1, '\0');
        nvs_get_str(handle, key, text.data(), &size);
        entry["type"] = "text";
        entry["value"] = text.data();
        entry["editable"] = true;
        return;
    }
    case NVS_TYPE_BLOB: {
        size_t size = 0;
        nvs_get_blob(handle, key, nullptr, &size);
        std::vector<uint8_t> bytes(size);
        nvs_get_blob(handle, key, bytes.data(), &size);
        if (size == sizeof(float)) { // how pu::Settings stores a float
            float value = 0.0F;
            memcpy(&value, bytes.data(), sizeof(value));
            entry["type"] = "float";
            entry["value"] = tidy(value);
            entry["editable"] = true;
        } else if (size == sizeof(double)) {
            double value = 0.0;
            memcpy(&value, bytes.data(), sizeof(value));
            entry["type"] = "double";
            entry["value"] = value;
            entry["editable"] = true;
        } else {
            entry["type"] = "blob";
            entry["value"] = hex(bytes);
            entry["size"] = size;
        }
        return;
    }
    default:
        entry["type"] = "other"; // types pu::Settings never writes; shown, deletable
        entry["value"] = nullptr;
        return;
    }
}

void describeDefined(const Settings::EntryView& view, JsonObject entry) {
    entry["type"] = typeName(view.type);
    entry["defined"] = true;
    entry["secret"] = view.secret;
    entry["editable"] = view.type != Settings::Type::Blob;
    switch (view.type) {
    case Settings::Type::Bool:
        entry["value"] = view.value != 0.0;
        entry["default"] = view.defaultValue != 0.0;
        break;
    case Settings::Type::Float:
        entry["value"] = tidy(static_cast<float>(view.value));
        entry["default"] = tidy(static_cast<float>(view.defaultValue));
        break;
    case Settings::Type::Int:
    case Settings::Type::Double:
        entry["value"] = view.value;
        entry["default"] = view.defaultValue;
        break;
    case Settings::Type::String:
        if (view.secret) {
            entry["set"] = view.textSet; // never the value itself
        } else {
            entry["value"] = view.text;
            entry["default"] = view.defaultText;
        }
        break;
    default:
        entry["value"] = hex(view.blob);
        entry["size"] = view.blob.size();
        break;
    }
}
} // namespace

void Memory::defineSettings(Settings& library) {
    library.define(kAreasKey, "");
}

std::vector<String> Memory::ownAreas() const {
    std::vector<String> areas;
    const String list = library_->getString(kAreasKey);
    int start = 0;
    while (start < static_cast<int>(list.length())) {
        int end = list.indexOf(',', start);
        if (end < 0) {
            end = static_cast<int>(list.length());
        }
        const String area = list.substring(start, end);
        if (!area.isEmpty() && Settings::forNamespace(area.c_str()) == nullptr) {
            areas.push_back(area);
        }
        start = end + 1;
    }
    return areas;
}

bool Memory::isListed(const String& area) const {
    if (Settings::forNamespace(area.c_str()) != nullptr) {
        return true;
    }
    for (const String& own : ownAreas()) {
        if (own == area) {
            return true;
        }
    }
    return false;
}

void Memory::describe(JsonDocument& doc) const {
    nvs_stats_t stats = {};
    if (nvs_get_stats(nullptr, &stats) == ESP_OK) {
        doc["used"] = stats.used_entries;
        doc["total"] = stats.total_entries;
    }
    doc["library"] = library_->nvsNamespace();
    JsonArray areas = doc["areas"].to<JsonArray>();

    std::vector<String> names;
    for (const Settings* settings : Settings::all()) {
        names.push_back(settings->nvsNamespace());
    }
    for (const String& own : ownAreas()) {
        names.push_back(own);
    }

    for (const String& name : names) {
        JsonObject area = areas.add<JsonObject>();
        area["name"] = name;
        area["library"] = name == library_->nvsNamespace();
        JsonArray entries = area["entries"].to<JsonArray>();
        const std::vector<Stored> stored = storedIn(name.c_str());

        const Settings* settings = Settings::forNamespace(name.c_str());
        std::vector<String> defined;
        if (settings != nullptr) {
            settings->forEach([&](const Settings::EntryView& view) {
                JsonObject entry = entries.add<JsonObject>();
                entry["key"] = view.key;
                entry["stored"] = findStored(stored, view.key) != nullptr;
                describeDefined(view, entry);
                defined.push_back(view.key);
            });
        }

        nvs_handle_t handle = 0;
        if (nvs_open(name.c_str(), NVS_READONLY, &handle) != ESP_OK) {
            continue; // nothing stored in this area yet
        }
        for (const Stored& item : stored) {
            bool isDefined = false;
            for (const String& key : defined) {
                isDefined = isDefined || key == item.key;
            }
            if (isDefined) {
                continue;
            }
            JsonObject entry = entries.add<JsonObject>();
            entry["key"] = item.key;
            entry["stored"] = true;
            entry["defined"] = false;
            entry["secret"] = false;
            describeRaw(handle, item, entry);
        }
        nvs_close(handle);
    }
}

int Memory::set(const String& area, const String& key, const String& value, String& message) {
    if (!isListed(area)) {
        message = "unknown area";
        return 404;
    }
    if (value.length() > kMaxText) {
        message = "the value is too long";
        return 400;
    }
    Settings* settings = Settings::forNamespace(area.c_str());
    Settings::Type type = Settings::Type::Int;
    if (settings != nullptr && settings->typeOf(key.c_str(), type)) {
        esp_err_t error = ESP_FAIL;
        int32_t whole = 0;
        double number = 0.0;
        bool flag = false;
        switch (type) {
        case Settings::Type::Int:
            if (!parseWhole(value, whole)) {
                message = "a whole number is needed";
                return 400;
            }
            error = settings->set(key.c_str(), static_cast<int>(whole));
            break;
        case Settings::Type::Float:
        case Settings::Type::Double:
            if (!parseNumber(value.c_str(), number)) {
                message = "a number is needed";
                return 400;
            }
            error = type == Settings::Type::Float
                        ? settings->set(key.c_str(), static_cast<float>(number))
                        : settings->set(key.c_str(), number);
            break;
        case Settings::Type::Bool:
            if (!parseBool(value, flag)) {
                message = "on or off is needed";
                return 400;
            }
            error = settings->set(key.c_str(), flag);
            break;
        case Settings::Type::String:
            error = settings->set(key.c_str(), value);
            break;
        default:
            message = "data entries cannot be changed here";
            return 400;
        }
        message = error == ESP_OK ? "saved" : esp_err_to_name(error);
        return error == ESP_OK ? 200 : 500;
    }

    // not defined by the firmware: write with the type it is stored with
    const std::vector<Stored> stored = storedIn(area.c_str());
    const Stored* item = findStored(stored, key);
    if (item == nullptr) {
        message = "no such entry";
        return 404;
    }
    int32_t whole = 0;
    double number = 0.0;
    bool flag = false;
    esp_err_t error = ESP_FAIL;
    const char* name = key.c_str();
    if (item->type == NVS_TYPE_I32 && parseWhole(value, whole)) {
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_i32(h, name, whole); });
    } else if (item->type == NVS_TYPE_U8 && parseBool(value, flag)) {
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_u8(h, name, flag ? 1 : 0); });
    } else if (item->type == NVS_TYPE_STR) {
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_str(h, name, value.c_str()); });
    } else if (item->type == NVS_TYPE_BLOB && parseNumber(value.c_str(), number)) {
        nvs_handle_t handle = 0;
        size_t size = 0;
        if (nvs_open(area.c_str(), NVS_READONLY, &handle) == ESP_OK) {
            nvs_get_blob(handle, name, nullptr, &size);
            nvs_close(handle);
        }
        const float single = static_cast<float>(number);
        if (size == sizeof(float)) {
            error = writeRaw(area, [&](nvs_handle_t h) {
                return nvs_set_blob(h, name, &single, sizeof(single));
            });
        } else if (size == sizeof(double)) {
            error = writeRaw(area, [&](nvs_handle_t h) {
                return nvs_set_blob(h, name, &number, sizeof(number));
            });
        } else {
            message = "data entries cannot be changed here";
            return 400;
        }
    } else {
        message = "the value does not fit the type of this entry";
        return 400;
    }
    message = error == ESP_OK ? "saved" : esp_err_to_name(error);
    return error == ESP_OK ? 200 : 500;
}

int Memory::reset(const String& area, const String& key, String& message) {
    if (!isListed(area)) {
        message = "unknown area";
        return 404;
    }
    Settings* settings = Settings::forNamespace(area.c_str());
    if (settings != nullptr && settings->contains(key.c_str())) {
        const esp_err_t error = settings->reset(key.c_str());
        message = error == ESP_OK ? "back to its default" : esp_err_to_name(error);
        return error == ESP_OK ? 200 : 500;
    }
    if (findStored(storedIn(area.c_str()), key) == nullptr) {
        message = "no such entry";
        return 404;
    }
    const esp_err_t error =
        writeRaw(area, [&](nvs_handle_t h) { return nvs_erase_key(h, key.c_str()); });
    if (error != ESP_OK) {
        message = esp_err_to_name(error);
        return 500;
    }
    if (settings == nullptr && storedIn(area.c_str()).empty()) {
        // the last entry of an area created here: the area goes too
        String list;
        for (const String& own : ownAreas()) {
            if (own != area) {
                list += (list.isEmpty() ? "" : ",") + own;
            }
        }
        library_->set(kAreasKey, list);
    }
    message = "deleted";
    return 200;
}

int Memory::add(const String& area, const String& key, const String& type, const String& value,
                String& message) {
    if (!isValidName(area) || !isValidName(key)) {
        message = "area and name: 1 to 15 letters, digits, - _ or .";
        return 400;
    }
    if (area == library_->nvsNamespace()) {
        message = "the library's own area takes no new entries";
        return 403;
    }
    const std::vector<Stored> stored = storedIn(area.c_str());
    const bool listed = isListed(area);
    if (!listed && !stored.empty()) {
        message = "this area is used by another part of the firmware";
        return 409;
    }
    Settings* settings = Settings::forNamespace(area.c_str());
    if (findStored(stored, key) != nullptr ||
        (settings != nullptr && settings->contains(key.c_str()))) {
        message = "an entry with this name exists already";
        return 409;
    }
    if (value.length() > kMaxText) {
        message = "the value is too long";
        return 400;
    }

    int32_t whole = 0;
    double number = 0.0;
    bool flag = false;
    const char* name = key.c_str();
    esp_err_t error = ESP_FAIL;
    if (type == "int") {
        if (!parseWhole(value, whole)) {
            message = "a whole number is needed";
            return 400;
        }
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_i32(h, name, whole); });
    } else if (type == "float") {
        if (!parseNumber(value.c_str(), number)) {
            message = "a number is needed";
            return 400;
        }
        const float single = static_cast<float>(number);
        error = writeRaw(
            area, [&](nvs_handle_t h) { return nvs_set_blob(h, name, &single, sizeof(single)); });
    } else if (type == "bool") {
        if (!parseBool(value, flag)) {
            message = "on or off is needed";
            return 400;
        }
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_u8(h, name, flag ? 1 : 0); });
    } else if (type == "text") {
        error = writeRaw(area, [&](nvs_handle_t h) { return nvs_set_str(h, name, value.c_str()); });
    } else {
        message = "unknown type";
        return 400;
    }
    if (error != ESP_OK) {
        message = esp_err_to_name(error);
        return 500;
    }
    if (!listed) {
        String list = library_->getString(kAreasKey);
        library_->set(kAreasKey, list.isEmpty() ? area : list + "," + area);
    }
    message = "added";
    return 200;
}

} // namespace pu
