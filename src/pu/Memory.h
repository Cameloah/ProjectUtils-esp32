#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

namespace pu {

class Settings;

/// Backs the memory page: lists what the device keeps in flash (NVS), per area (NVS namespace),
/// and changes it.
///
/// Areas shown: every started pu::Settings object (the library's and the project's), plus areas
/// created on the memory page. Entries: everything a Settings object defines (stored or at its
/// default) plus whatever else is stored in those areas. Values of secret entries never leave the
/// device.
///
/// Entries the firmware defines can be changed and reset to their default; others can be
/// changed and deleted. New entries can be added to any area except the library's own, stored the
/// way pu::Settings stores them, so that a later firmware which defines them reads them.
class Memory {
  public:
    /// Registers the memory page's own key in the library settings. Before settings.loadAll().
    static void defineSettings(Settings& library);

    void begin(Settings& library) { library_ = &library; }

    /// Fills `doc` with usage figures and all areas with their entries.
    void describe(JsonDocument& doc) const;

    // Each returns an HTTP status code and a message for the user.
    int set(const String& area, const String& key, const String& value, String& message);
    /// Defined entries: back to the default. Others: deleted.
    int reset(const String& area, const String& key, String& message);
    /// type: "int", "float", "bool" or "text".
    int add(const String& area, const String& key, const String& type, const String& value,
            String& message);

  private:
    std::vector<String> ownAreas() const;
    bool isListed(const String& area) const;

    Settings* library_ = nullptr;
};

} // namespace pu
