#include "WifiManager.h"

#include <WiFi.h>

#include "Hostname.h"
#include "RamLog.h"
#include "Settings.h"

namespace pu {

namespace {
const uint16_t kDnsPort = 53;
const uint32_t kRetryIntervalMs = 60000;
const uint32_t kConnectPollMs = 100;

String chipSuffix() {
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "%06X",
             static_cast<unsigned>((ESP.getEfuseMac() >> 24U) & 0xFFFFFFU));
    return String(suffix);
}
} // namespace

void WifiManager::defineSettings(Settings& settings) {
    settings.define("ssid", "");
    settings.define("password", "");
    settings.define("localIP", "");
    settings.define("gateway", "");
    settings.define("subnet", "255.255.255.0");
    settings.define("primaryDNS", "8.8.8.8");
    settings.define("secondaryDNS", "8.8.4.4");
    settings.define("deviceName", "");
}

void WifiManager::begin(const Config& config, Settings& settings, RamLog& log) {
    config_ = &config;
    settings_ = &settings;
    log_ = &log;

    deviceName_ = settings.getString("deviceName");
    deviceName_.trim();
    if (deviceName_.isEmpty()) {
        deviceName_ = config.deviceName + " " + chipSuffix();
    }
    hostname_ = makeHostname(deviceName_.c_str()).c_str();
    ssid_ = settings.getString("ssid");
    ssid_.trim();

    WiFi.persistent(false); // credentials live in Settings, not in the WiFi driver's own storage
    WiFi.setHostname(hostname_.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    bool connected = false;
    if (hasCredentials()) {
        connected = connect();
    } else {
        log_->info("No WiFi configured");
    }
    wasConnected_ = connected;

    if (!connected || config.apMode == ApMode::Always) {
        startPortal();
    }
}

void WifiManager::applyIpConfig() {
    const String ipText = settings_->getString("localIP");
    IPAddress ip;
    if (ipText.isEmpty() || !ip.fromString(ipText)) {
        return; // DHCP
    }
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
    if (!gateway.fromString(settings_->getString("gateway"))) {
        gateway = IPAddress(ip[0], ip[1], ip[2], 1);
    }
    if (!subnet.fromString(settings_->getString("subnet"))) {
        subnet = IPAddress(255, 255, 255, 0);
    }
    if (!dns1.fromString(settings_->getString("primaryDNS"))) {
        dns1 = gateway;
    }
    if (!dns2.fromString(settings_->getString("secondaryDNS"))) {
        dns2 = dns1;
    }
    if (!WiFi.config(ip, gateway, subnet, dns1, dns2)) {
        log_->error("Static IP %s could not be applied, using DHCP", ipText.c_str());
    }
}

bool WifiManager::connect() {
    applyIpConfig();
    const String password = settings_->getString("password");
    log_->info("Connecting to WiFi '%s'", ssid_.c_str());
    WiFi.begin(ssid_.c_str(), password.isEmpty() ? nullptr : password.c_str());

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < config_->wifiConnectTimeoutMs) {
        delay(kConnectPollMs);
    }
    if (WiFi.status() != WL_CONNECTED) {
        log_->warn("WiFi '%s' not reachable within %u ms", ssid_.c_str(),
                   static_cast<unsigned>(config_->wifiConnectTimeoutMs));
        return false;
    }
    log_->info("WiFi connected, IP %s, http://%s.local/", WiFi.localIP().toString().c_str(),
               hostname_.c_str());
    return true;
}

void WifiManager::startPortal() {
    // Keep the station interface alive when credentials exist so the connection can recover.
    WiFi.mode(WIFI_AP_STA); // station side stays on so the settings page can search for networks
    const char* password = config_->apPassword.isEmpty() ? nullptr : config_->apPassword.c_str();
    if (!WiFi.softAP(deviceName_.c_str(), password)) {
        log_->error("Access point '%s' could not be started", deviceName_.c_str());
        return;
    }
    dnsServer_.start(kDnsPort, "*", WiFi.softAPIP()); // answer every lookup -> captive portal
    portalStartMs_ = millis();
    lastRetryMs_ = millis();
    portalActive_.store(true);
    log_->info("Access point '%s' open, settings at http://%s/settings", deviceName_.c_str(),
               WiFi.softAPIP().toString().c_str());
}

void WifiManager::stopPortal() {
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalActive_.store(false);
    log_->info("Access point closed");
}

void WifiManager::loop() {
    const bool connected = isConnected();
    if (connected != wasConnected_) {
        wasConnected_ = connected;
        if (connected) {
            log_->info("WiFi connected, IP %s", WiFi.localIP().toString().c_str());
        } else {
            log_->warn("WiFi connection lost");
        }
    }

    const bool portalOpen = portalActive_.load();
    if (portalOpen) {
        dnsServer_.processNextRequest();
    }
    const uint32_t now = millis();
    const bool portalInUse = portalOpen && WiFi.softAPgetStationNum() > 0;

    // Retrying scans other channels, which disturbs clients of the access point, so only retry
    // while nobody is using the portal.
    if (!connected && hasCredentials() && !portalInUse && now - lastRetryMs_ >= kRetryIntervalMs) {
        lastRetryMs_ = now;
        WiFi.reconnect();
    }

    if (!portalOpen || portalInUse || config_->apMode == ApMode::Always) {
        return;
    }
    // The fallback access point is closed once WiFi is back, or after the configured timeout.
    const bool timedOut = config_->apTimeoutMs > 0 && now - portalStartMs_ >= config_->apTimeoutMs;
    if (connected || timedOut) {
        stopPortal();
    }
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

const char* WifiManager::modeName() {
    switch (WiFi.getMode()) {
    case WIFI_MODE_STA:
        return "STA";
    case WIFI_MODE_AP:
        return "AP";
    case WIFI_MODE_APSTA:
        return "AP+STA";
    default:
        return "off";
    }
}

} // namespace pu
