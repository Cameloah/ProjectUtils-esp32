#include "ProjectUtils.h"

// NOLINTNEXTLINE(readability-identifier-naming): Arduino-style global, like Serial or WiFi
pu::ProjectUtilsClass ProjectUtils;

namespace pu {

namespace {
// Unchanged from v1 so devices keep their stored WiFi credentials across the upgrade.
const char kNvsNamespace[] = "project_utils";
} // namespace

void ProjectUtilsClass::begin(const Config& config) {
    if (started_) {
        return;
    }
    started_ = true;
    config_ = config;

    if (config_.serial != nullptr) {
        console_.begin(*config_.serial);
    }
    log_.setClock(&clock_);
    log_.setEcho(&console_);

    char version[24];
    config_.firmwareVersion.format(version, sizeof(version));
    log_.info("%s, firmware %s", config_.deviceName.c_str(), version);
    if (config_.adminPassword.isEmpty()) {
        log_.warn("No admin password set: web interface and OTA are open to the whole network");
    }

    settings_.begin(kNvsNamespace, &log_);
    WifiManager::defineSettings(settings_);
    settings_.define("autoUpdates", false);
    settings_.loadAll();

    wifi_.begin(config_, settings_, log_);
    clock_.begin(config_.timezone.c_str(), config_.ntpServer.c_str());

    updater_.begin(config_, log_);
    if (updater_.configured() && wifi_.isConnected() && settings_.getBool("autoUpdates")) {
        if (updater_.check() == UpdateState::Available && updater_.install()) {
            log_.info("Restarting into new firmware");
            console_.flush();
            ESP.restart();
        }
    }

    if (config_.enableOta) {
        ota_.begin(wifi_.hostname(), config_.adminPassword, log_);
    }
    web_.begin(config_, settings_, wifi_, updater_, log_, console_);
}

void ProjectUtilsClass::loop() {
    if (!started_) {
        return;
    }
    wifi_.loop();
    ota_.loop();
    web_.loop();
}

} // namespace pu
