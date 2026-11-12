#pragma once

#include <Arduino.h>

#include "Semver.h"

namespace pu {

enum class ApMode : uint8_t {
    OnFailure, ///< Open the setup access point only when WiFi is unconfigured or unreachable.
    Always,    ///< Always open the access point in addition to the WiFi connection.
};

/// Everything the library needs to know about the project. Fill in what differs from the
/// defaults and pass it to ProjectUtils.begin().
struct Config {
    /// Shown in the web UI and used (plus a chip-specific suffix) as access point name until the
    /// user picks a name on the settings page.
    String deviceName = "ESP32 Device";
    Semver firmwareVersion = Semver{0, 0, 0};

    // --- WiFi ---
    ApMode apMode = ApMode::OnFailure;
    String apPassword;        ///< Empty = open access point. Otherwise >= 8 chars.
    uint32_t apTimeoutMs = 0; ///< Close an unused access point after this time. 0 = never.
    uint32_t wifiConnectTimeoutMs = 10000; ///< How long begin() waits for the WiFi connection.

    // --- Security ---
    /// When set, the settings page, the JSON API, WebSerial and ArduinoOTA require this password.
    /// Empty = unprotected (anyone on the network can reconfigure and reflash the device).
    String adminUser = "admin";
    String adminPassword;

    // --- Time ---
    String timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; ///< POSIX TZ string.
    String ntpServer = "pool.ntp.org";

    // --- Updates from GitHub releases ---
    String githubRepo;                           ///< "user/repository". Empty = updater disabled.
    String githubFirmwareAsset = "firmware.bin"; ///< "<version>" is replaced by the release tag.
    String githubFilesystemAsset;                ///< Optional filesystem image, same placeholder.

    // --- Services ---
    bool enableWebSerial = true;
    bool enableOta = true;
    Stream* serial = nullptr; ///< Console output/input stream. nullptr = Serial.
};

} // namespace pu
