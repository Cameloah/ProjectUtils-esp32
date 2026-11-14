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
const uint32_t kRevertRestartDelayMs = 200; // lets the log line reach the console
const char kBackupSeparator = ';';

String nthField(const String& text, int index) {
    int start = 0;
    for (int field = 0; field < index; ++field) {
        start = text.indexOf(kBackupSeparator, start);
        if (start < 0) {
            return String();
        }
        ++start;
    }
    const int end = text.indexOf(kBackupSeparator, start);
    return end < 0 ? text.substring(start) : text.substring(start, end);
}

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
    // Address configuration: 0 = DHCP, 1 = static. -1 = from before this setting existed.
    settings.define("ipMode", -1);
    settings.define("ipPending", false); // a new static address is being tried out
    settings.define("ipBackup", "");     // the configuration to go back to
    settings.define("ipReverted", false);
    // Access point, per device. -1 / apPassOwn = false: use the firmware default.
    settings.define("apMode", -1);
    settings.define("apTimeout", -1); // seconds
    settings.define("apGrace", -1);   // seconds
    settings.define("apPassOwn", false);
    settings.define("apPass", "");
    settings.markSecret("password");
    settings.markSecret("apPass");
}

ApSettings WifiManager::apDefaults(const Config& config) {
    ApSettings defaults;
    defaults.mode = config.apMode;
    defaults.password = config.apPassword;
    defaults.timeoutMs = config.apTimeoutMs;
    defaults.graceMs = config.apGraceMs;
    return defaults;
}

void WifiManager::loadApSettings() {
    ap_ = apDefaults(*config_);
    if (config_->apLocked) {
        return;
    }
    const int32_t mode = settings_->getInt("apMode", -1);
    if (mode >= static_cast<int32_t>(ApMode::Never) &&
        mode <= static_cast<int32_t>(ApMode::Always)) {
        ap_.mode = static_cast<ApMode>(mode);
    }
    const int32_t timeoutSeconds = settings_->getInt("apTimeout", -1);
    if (timeoutSeconds >= 0) {
        ap_.timeoutMs = static_cast<uint32_t>(timeoutSeconds) * 1000U;
    }
    const int32_t graceSeconds = settings_->getInt("apGrace", -1);
    if (graceSeconds >= 0) {
        ap_.graceMs = static_cast<uint32_t>(graceSeconds) * 1000U;
    }
    if (settings_->getBool("apPassOwn")) {
        ap_.password = settings_->getString("apPass");
    }
}

IpConfig WifiManager::ipConfig() const {
    IpConfig config;
    config.isStatic = settings_->getInt("ipMode", 0) == 1;
    config.address = settings_->getString("localIP");
    config.gateway = settings_->getString("gateway");
    config.subnet = settings_->getString("subnet");
    config.dns1 = settings_->getString("primaryDNS");
    config.dns2 = settings_->getString("secondaryDNS");
    return config;
}

void WifiManager::saveIpConfig(const IpConfig& config, bool trial) {
    if (trial) {
        const IpConfig old = ipConfig();
        const String backup = String(old.isStatic ? "1" : "0") + kBackupSeparator + old.address +
                              kBackupSeparator + old.gateway + kBackupSeparator + old.subnet +
                              kBackupSeparator + old.dns1 + kBackupSeparator + old.dns2;
        settings_->set("ipBackup", backup);
    }
    settings_->set("ipMode", config.isStatic ? 1 : 0);
    settings_->set("localIP", config.address);
    settings_->set("gateway", config.gateway);
    settings_->set("subnet", config.subnet);
    settings_->set("primaryDNS", config.dns1);
    settings_->set("secondaryDNS", config.dns2);
    settings_->set("ipPending", trial);
}

void WifiManager::restoreIpBackup(const char* reason) {
    const String backup = settings_->getString("ipBackup");
    IpConfig old;
    old.isStatic = nthField(backup, 0) == "1";
    old.address = nthField(backup, 1);
    old.gateway = nthField(backup, 2);
    old.subnet = nthField(backup, 3);
    old.dns1 = nthField(backup, 4);
    old.dns2 = nthField(backup, 5);
    saveIpConfig(old, false);
    settings_->set("ipBackup", "");
    settings_->set("ipReverted", true);
    log_->warn("%s, back to the previous address settings. Restarting", reason);
    delay(kRevertRestartDelayMs);
    ESP.restart();
}

uint32_t WifiManager::ipTrialRemainingMs() const {
    if (!ipTrial_.load()) {
        return 0;
    }
    const uint32_t elapsed = millis() - ipTrialStartMs_.load();
    return elapsed >= kIpTrialMs ? 1 : kIpTrialMs - elapsed;
}

void WifiManager::loopIpTrial() {
    if (!ipTrial_.load()) {
        confirmRequested_.store(false);
        revertRequested_.store(false);
        return;
    }
    if (confirmRequested_.exchange(false)) {
        ipTrial_.store(false);
        settings_->set("ipPending", false);
        settings_->set("ipBackup", "");
        log_->info("New address settings confirmed");
        return;
    }
    if (revertRequested_.exchange(false)) {
        restoreIpBackup("New address settings rejected");
    }
    if (millis() - ipTrialStartMs_.load() >= kIpTrialMs) {
        restoreIpBackup("New address settings not confirmed in time");
    }
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
    loadApSettings();
    ApPolicy::Options options;
    options.mode = ap_.mode;
    options.timeoutMs = ap_.timeoutMs;
    options.graceMs = ap_.graceMs;
    policy_ = ApPolicy(options);

    if (settings.getInt("ipMode", -1) < 0) { // stored by a version without the switch
        settings.set("ipMode", settings.getString("localIP").isEmpty() ? 0 : 1);
    }
    ipReverted_ = settings.getBool("ipReverted");
    if (ipReverted_) {
        settings.set("ipReverted", false); // reported until the next restart
    }

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

    if (settings.getBool("ipPending")) {
        ipTrialStartMs_.store(millis());
        ipTrial_.store(true);
        log_->info("Trying new address settings, confirm on the settings page within %u s",
                   static_cast<unsigned>(kIpTrialMs / 1000U));
    }

    policy_.update(millis(), hasCredentials(), connected, 0);
    if (policy_.openAtStart(hasCredentials(), connected)) {
        startPortal();
    }
}

void WifiManager::applyIpConfig() {
    const String ipText = settings_->getString("localIP");
    IPAddress ip;
    if (settings_->getInt("ipMode", 0) != 1 || !ip.fromString(ipText)) {
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
    const char* password = ap_.password.isEmpty() ? nullptr : ap_.password.c_str();
    if (!WiFi.softAP(deviceName_.c_str(), password)) {
        log_->error("Access point '%s' could not be started%s", deviceName_.c_str(),
                    ap_.password.length() > 0 && ap_.password.length() < 8
                        ? ": its password needs at least 8 characters"
                        : "");
        policy_.openFailed(millis());
        if (isConnected()) {
            WiFi.mode(WIFI_STA);
        }
        return;
    }
    dnsServer_.start(kDnsPort, "*", WiFi.softAPIP()); // answer every lookup -> captive portal
    policy_.opened(millis());
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
    policy_.closed();
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
    const unsigned clients = portalOpen ? WiFi.softAPgetStationNum() : 0U;
    const bool portalInUse = clients > 0U;

    // Retrying scans other channels, which disturbs clients of the access point, so only retry
    // while nobody is using the portal.
    if (!connected && hasCredentials() && !portalInUse && now - lastRetryMs_ >= kRetryIntervalMs) {
        lastRetryMs_ = now;
        WiFi.reconnect();
    }

    switch (policy_.update(now, hasCredentials(), connected, clients)) {
    case ApPolicy::Action::Open:
        log_->warn("WiFi lost for %u s", static_cast<unsigned>(ap_.graceMs / 1000U));
        startPortal();
        break;
    case ApPolicy::Action::Close:
        stopPortal();
        break;
    default:
        break;
    }
    loopIpTrial();
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
