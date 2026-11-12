#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <atomic>

#include "Config.h"

namespace pu {

class RamLog;
class Settings;

/// Connects to the configured WiFi network and, if that is not possible, opens a setup access
/// point with a captive portal that leads to the settings page.
class WifiManager {
  public:
    /// Registers the WiFi-related keys in `settings`. Call before settings.loadAll().
    static void defineSettings(Settings& settings);

    /// Tries to connect (blocks for at most config.wifiConnectTimeoutMs) and opens the setup
    /// access point according to config.apMode.
    void begin(const Config& config, Settings& settings, RamLog& log);

    /// Must be called regularly from loop().
    void loop();

    bool isConnected() const;
    /// True while the setup access point (captive portal) is open. Safe from any task.
    bool portalActive() const { return portalActive_.load(); }

    /// Name chosen by the user, or the configured default plus a chip-specific suffix.
    String deviceName() const { return deviceName_; }
    String hostname() const { return hostname_; }
    static const char* modeName();

  private:
    bool hasCredentials() const { return !ssid_.isEmpty(); }
    bool connect();
    void applyIpConfig();
    void startPortal();
    void stopPortal();

    const Config* config_ = nullptr;
    Settings* settings_ = nullptr;
    RamLog* log_ = nullptr;

    DNSServer dnsServer_;
    String deviceName_;
    String hostname_;
    String ssid_;
    std::atomic<bool> portalActive_{false};
    bool wasConnected_ = false;
    uint32_t portalStartMs_ = 0;
    uint32_t lastRetryMs_ = 0;
};

} // namespace pu
