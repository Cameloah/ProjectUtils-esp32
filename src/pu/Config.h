#pragma once

#include <Arduino.h>

#include "ApPolicy.h"
#include "Semver.h"

namespace pu {

/// Everything the library needs to know about the project. Fill in what differs from the
/// defaults and pass it to ProjectUtils.begin().
struct Config {
    /// Shown in the web UI and used (plus a chip-specific suffix) as access point name until the
    /// user picks a name on the settings page.
    String deviceName = "ESP32 Device";
    Semver firmwareVersion = Semver{0, 0, 0};

    // --- WiFi ---
    uint32_t wifiConnectTimeoutMs = 10000; ///< How long begin() waits for the WiFi connection.

    // --- Setup access point ---
    // Defaults for every device running this firmware. Each device can change them on its
    // settings page (stored on the device) unless apLocked is set.
    ApMode apMode = ApMode::OnFailure;
    String apPassword;           ///< Empty = open access point. Otherwise 8 to 63 characters.
    uint32_t apTimeoutMs = 0;    ///< Close after this long without anybody connected. 0 = never.
    uint32_t apGraceMs = 120000; ///< OnFailure: open once WiFi has been lost this long.
    bool apLocked = false;       ///< true = the settings page only shows the values above.

    // --- Security ---
    /// User name for the admin login. The password is set on each device's settings page.
    String adminUser = "admin";
    /// Deprecated: readable as plain text in the firmware file and the repository. Used only while
    /// no password is set on the device. Will be removed in 3.0.
    String adminPassword;

    // --- Time ---
    String timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; ///< POSIX TZ string.
    String ntpServer = "pool.ntp.org";

    // --- Updates from GitHub releases ---
    String githubRepo;                           ///< "user/repository". Empty = updater disabled.
    String githubFirmwareAsset = "firmware.bin"; ///< "<version>" is replaced by the release tag.
    String githubFilesystemAsset;                ///< Optional filesystem image, same placeholder.

    // --- Services ---
    bool enableConsole = true; ///< Web console at /console.
    bool enableOta = true;
    Stream* serial = nullptr; ///< Console output/input stream. nullptr = Serial.
};

} // namespace pu
