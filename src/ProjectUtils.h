#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "pu/Clock.h"
#include "pu/Config.h"
#include "pu/Console.h"
#include "pu/GithubUpdater.h"
#include "pu/OtaService.h"
#include "pu/Page.h"
#include "pu/RamLog.h"
#include "pu/Semver.h"
#include "pu/Settings.h"
#include "pu/WebUi.h"
#include "pu/WifiManager.h"

namespace pu {

/// Entry point of the library: owns all modules and wires them together.
/// Use the global instance `ProjectUtils`.
class ProjectUtilsClass {
  public:
    /// Starts everything according to `config`. Call once from setup(), after Serial.begin().
    /// Blocks for at most config.wifiConnectTimeoutMs while connecting to WiFi (longer if an
    /// automatic firmware update is installed).
    void begin(const Config& config);

    /// Call from loop() on every iteration.
    void loop();

    /// Serial replacement that also shows up in the web console. Usable before begin().
    Console& console() { return console_; }
    RamLog& log() { return log_; }
    /// The library's own settings (WiFi credentials, device name, ...). For project settings
    /// create a separate pu::Settings with its own namespace.
    Settings& settings() { return settings_; }
    WifiManager& wifi() { return wifi_; }
    Clock& clock() { return clock_; }
    GithubUpdater& updater() { return updater_; }
    WebUi& web() { return web_; }

    /// Erases everything stored in flash (WiFi, admin password, all settings) and restarts into
    /// the setup access point. The firmware stays. Call from loop(), e.g. for a reset button.
    void factoryReset() { web_.factoryReset(); }
    /// Web server for the project's own routes; register them after begin().
    AsyncWebServer& server() { return web_.server(); }
    const Config& config() const { return config_; }

  private:
    String adminPassword() const;
    bool handleUsbCommand(const String& line);

    Config config_;
    Console console_;
    RamLog log_;
    Clock clock_;
    Settings settings_;
    WifiManager wifi_;
    GithubUpdater updater_;
    OtaService ota_;
    WebUi web_;
    bool started_ = false;
};

} // namespace pu

// NOLINTNEXTLINE(readability-identifier-naming): Arduino-style global, like Serial or WiFi
extern pu::ProjectUtilsClass ProjectUtils;
