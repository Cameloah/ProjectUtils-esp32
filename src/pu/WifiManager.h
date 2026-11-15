#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <atomic>

#include "ApPolicy.h"
#include "Config.h"

namespace pu {

class RamLog;
class Settings;

/// Access point settings in effect: the firmware defaults from Config, with the values changed
/// on this device's settings page on top (unless Config::apLocked).
struct ApSettings {
    ApMode mode = ApMode::OnFailure;
    String password;
    uint32_t timeoutMs = 0;
    uint32_t graceMs = 0;
};

/// Address configuration of the WiFi connection. Empty DNS fields fall back to the gateway.
struct IpConfig {
    bool isStatic = false;
    String address;
    String gateway;
    String subnet;
    String dns1;
    String dns2;

    bool operator==(const IpConfig& other) const {
        return isStatic == other.isStatic && address == other.address && gateway == other.gateway &&
               subnet == other.subnet && dns1 == other.dns1 && dns2 == other.dns2;
    }
    bool operator!=(const IpConfig& other) const { return !(*this == other); }
};

/// Connects to the configured WiFi network and opens a setup access point with a captive portal
/// that leads to the settings page, as decided by ApPolicy.
///
/// A new static IP address is first tried out: unless it is confirmed (confirmIp()) within
/// kIpTrialMs after the restart, the previous address configuration is restored.
class WifiManager {
  public:
    static const uint32_t kIpTrialMs = 120000;
    static const uint32_t kScanTimeoutMs = 15000;

    /// Registers the WiFi-related keys in `settings`. Call before settings.loadAll().
    static void defineSettings(Settings& settings);

    /// Tries to connect (blocks for at most config.wifiConnectTimeoutMs) and opens the setup
    /// access point if needed.
    void begin(const Config& config, Settings& settings, RamLog& log);

    /// Must be called regularly from loop().
    void loop();

    bool isConnected() const;
    /// True while the setup access point (captive portal) is open. Safe from any task.
    bool portalActive() const { return portalActive_.load(); }

    /// Name chosen by the user, or the configured default plus a chip-specific suffix.
    String deviceName() const { return deviceName_; }
    String hostname() const { return hostname_; }
    /// The name without one chosen by the user: Config::deviceName plus a chip-specific suffix.
    /// Also the access point's name after a factory reset.
    String defaultName() const;
    static const char* modeName();

    /// Starts a network search in the background. Stops connection attempts first while not
    /// connected, since the radio cannot search while it tries to connect. Safe from any task.
    bool startScan();
    /// Like WiFi.scanComplete(), but keeps reporting WIFI_SCAN_RUNNING for a search this class
    /// started until kScanTimeoutMs: with the access point open a search takes longer than the
    /// Arduino core waits (6 s) before it reports a failure. Safe from any task.
    int16_t scanStatus() const;

    const ApSettings& apSettings() const { return ap_; }
    static ApSettings apDefaults(const Config& config);

    /// The stored address configuration.
    IpConfig ipConfig() const;
    /// Stores `config`. With trial = true the previous one is kept and restored after the next
    /// restart unless confirmIp() is called in time. Takes effect after a restart.
    void saveIpConfig(const IpConfig& config, bool trial);

    /// While a new static address is being tried out: milliseconds until it is reverted, else 0.
    uint32_t ipTrialRemainingMs() const;
    /// True if the last try-out was not confirmed and the previous configuration came back.
    bool ipReverted() const { return ipReverted_; }
    /// Keeps the address that is being tried out. Safe from any task.
    void confirmIp() { confirmRequested_.store(true); }
    /// Goes back to the previous address now (restarts). Safe from any task.
    void revertIp() { revertRequested_.store(true); }

  private:
    bool hasCredentials() const { return !ssid_.isEmpty(); }
    bool connect();
    void applyIpConfig();
    void loadApSettings();
    void startPortal();
    void stopPortal();
    void loopIpTrial();
    void restoreIpBackup(const char* reason);

    const Config* config_ = nullptr;
    Settings* settings_ = nullptr;
    RamLog* log_ = nullptr;

    DNSServer dnsServer_;
    ApSettings ap_;
    ApPolicy policy_{ApPolicy::Options{}};
    String deviceName_;
    String hostname_;
    String ssid_;
    std::atomic<bool> portalActive_{false};
    bool wasConnected_ = false;
    std::atomic<uint32_t> lastRetryMs_{0};
    std::atomic<uint32_t> scanStartedMs_{0}; // 0 = no search started by startScan()

    std::atomic<bool> ipTrial_{false};
    std::atomic<uint32_t> ipTrialStartMs_{0};
    bool ipReverted_ = false;
    std::atomic<bool> confirmRequested_{false};
    std::atomic<bool> revertRequested_{false};
};

} // namespace pu
