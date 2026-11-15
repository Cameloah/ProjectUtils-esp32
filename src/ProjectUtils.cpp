#include "ProjectUtils.h"

#include "pu/Memory.h"

// NOLINTNEXTLINE(readability-identifier-naming): Arduino-style global, like Serial or WiFi
pu::ProjectUtilsClass ProjectUtils;

namespace pu {

namespace {
// Unchanged from v1 so devices keep their stored WiFi credentials across the upgrade.
const char kNvsNamespace[] = "project_utils";
// Typed on the USB console only: removes the admin password (physical access needed).
const char kClearPasswordCommand[] = "password --clear";
} // namespace

String ProjectUtilsClass::adminPassword() const {
    const String stored = settings_.getString(WebUi::kAdminPasswordKey);
    return stored.isEmpty() ? config_.adminPassword : stored;
}

bool ProjectUtilsClass::handleUsbCommand(const String& line) {
    if (line != kClearPasswordCommand) {
        return false;
    }
    if (settings_.getString(WebUi::kAdminPasswordKey).isEmpty()) {
        log_.info(config_.adminPassword.isEmpty()
                      ? "No admin password is set"
                      : "The admin password is set in the firmware and cannot be removed here");
        return true;
    }
    settings_.set(WebUi::kAdminPasswordKey, "");
    log_.warn("Admin password removed via USB: the web pages are open now, OTA uploads after "
              "the next restart");
    return true;
}

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

    settings_.begin(kNvsNamespace, &log_);
    WifiManager::defineSettings(settings_);
    Memory::defineSettings(settings_);
    WebUi::defineSettings(settings_);
    settings_.define("autoUpdates", false);
    settings_.loadAll();
    console_.setLineFilter([this](const String& line) { return handleUsbCommand(line); });

    if (!config_.adminPassword.isEmpty()) {
        log_.warn("Config::adminPassword is deprecated: it is readable in the firmware file. Set "
                  "the password on the settings page instead");
    }
    if (adminPassword().isEmpty()) {
        log_.warn("No admin password set: web interface and OTA are open to the whole network");
    }

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
        ota_.begin(wifi_.hostname(), adminPassword(), log_);
    }
    web_.begin(config_, settings_, wifi_, updater_, log_, console_, clock_);
}

void ProjectUtilsClass::loop() {
    if (!started_) {
        return;
    }
    console_.poll();
    wifi_.loop();
    ota_.loop();
    web_.loop();
}

} // namespace pu
