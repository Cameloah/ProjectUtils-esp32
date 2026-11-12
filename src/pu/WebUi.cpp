#include "WebUi.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#ifndef PU_DISABLE_WEBSERIAL
#include <WebSerial.h>
#endif

#include "Console.h"
#include "GithubUpdater.h"
#include "RamLog.h"
#include "Settings.h"
#include "WifiManager.h"
#include "web_assets.gen.h"

namespace pu {

namespace {
const uint32_t kRestartDelayMs = 1500; // enough for the HTTP reply to reach the browser
const char kJson[] = "application/json";

void sendJson(AsyncWebServerRequest* request, int code, const JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    request->send(code, kJson, body);
}

void sendResult(AsyncWebServerRequest* request, int code, const char* message) {
    JsonDocument doc;
    doc["ok"] = code < 400;
    doc["message"] = message;
    sendJson(request, code, doc);
}

String postValue(AsyncWebServerRequest* request, const char* name) {
    const AsyncWebParameter* param = request->getParam(name, true);
    if (param == nullptr) {
        return String();
    }
    String value = param->value();
    value.trim();
    return value;
}

bool isIpOrEmpty(const String& text) {
    IPAddress ip;
    return text.isEmpty() || ip.fromString(text);
}
} // namespace

void WebUi::begin(const Config& config, Settings& settings, WifiManager& wifi,
                  GithubUpdater& updater, RamLog& log, Console& console) {
    config_ = &config;
    settings_ = &settings;
    wifi_ = &wifi;
    updater_ = &updater;
    log_ = &log;

    registerRoutes();
    beginWebSerial(console);
    server_.begin();
}

bool WebUi::authorize(AsyncWebServerRequest* request) const {
    if (config_->adminPassword.isEmpty()) {
        return true;
    }
    if (request->authenticate(config_->adminUser.c_str(), config_->adminPassword.c_str())) {
        return true;
    }
    request->requestAuthentication();
    return false;
}

void WebUi::requestRestart(uint32_t delayMs) {
    restartAtMs_.store(millis() + delayMs);
    restartRequested_.store(true);
}

void WebUi::beginWebSerial(Console& console) {
#ifndef PU_DISABLE_WEBSERIAL
    if (!config_->enableWebSerial) {
        return;
    }
    WebSerial.begin(&server_); // served at /webserial
    if (!config_->adminPassword.isEmpty()) {
        WebSerial.setAuthentication(config_->adminUser.c_str(), config_->adminPassword.c_str());
    }
    WebSerial.onMessage([&console](uint8_t* data, size_t length) {
        static const uint8_t kNewline = '\n';
        console.injectInput(data, length);
        console.injectInput(&kNewline, 1);
    });
    console.setMirror([](const uint8_t* data, size_t length) { WebSerial.write(data, length); });
#else
    (void)console;
#endif
}

void WebUi::registerRoutes() {
    server_.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            request->send(200, "text/html", reinterpret_cast<const uint8_t*>(kSettingsHtml),
                          sizeof(kSettingsHtml) - 1);
        }
    });
    server_.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/css", reinterpret_cast<const uint8_t*>(kStyleCss),
                      sizeof(kStyleCss) - 1);
    });

    server_.on("/api/pu/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleStatus(request);
        }
    });
    server_.on("/api/pu/log", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            AsyncResponseStream* response = request->beginResponseStream("text/plain");
            log_->printTo(*response);
            request->send(response);
        }
    });
    server_.on("/api/pu/settings", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleSettingsPost(request);
        }
    });
    server_.on("/api/pu/autoupdates", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleAutoUpdatesPost(request);
        }
    });
    server_.on("/api/pu/update/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            checkRequested_.store(true);
            sendResult(request, 202, "checking");
        }
    });
    server_.on("/api/pu/update/install", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleInstallPost(request);
        }
    });
    server_.on("/api/pu/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            requestRestart(kRestartDelayMs);
            sendResult(request, 202, "restarting");
        }
    });

    // While the setup access point is open every unknown URL leads to the settings page, which
    // is what makes phones and laptops show the captive portal automatically.
    server_.onNotFound([this](AsyncWebServerRequest* request) {
        if (wifi_->portalActive()) {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/settings");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

void WebUi::handleStatus(AsyncWebServerRequest* request) const {
    char version[24];
    config_->firmwareVersion.format(version, sizeof(version));
    const UpdateStatus update = updater_->status();

    JsonDocument doc;
    doc["deviceName"] = wifi_->deviceName();
    doc["hostname"] = wifi_->hostname();
    doc["firmware"] = version;
    doc["autoUpdates"] = settings_->getBool("autoUpdates");

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = wifi_->isConnected();
    wifi["portal"] = wifi_->portalActive();
    wifi["mode"] = WifiManager::modeName();
    wifi["ip"] = WiFi.localIP().toString();
    wifi["ssid"] = settings_->getString("ssid");
    wifi["passwordSet"] = !settings_->getString("password").isEmpty(); // never the password itself
    wifi["localIP"] = settings_->getString("localIP");
    wifi["gateway"] = settings_->getString("gateway");

    JsonObject updateJson = doc["update"].to<JsonObject>();
    updateJson["configured"] = updater_->configured();
    updateJson["state"] = GithubUpdater::stateName(update.state);
    updateJson["latest"] = update.latestTag;
    updateJson["message"] = update.message;

    sendJson(request, 200, doc);
}

void WebUi::handleSettingsPost(AsyncWebServerRequest* request) {
    const String localIp = postValue(request, "ip");
    const String gateway = postValue(request, "gateway");
    if (!isIpOrEmpty(localIp) || !isIpOrEmpty(gateway)) {
        sendResult(request, 400, "IP address or gateway is not a valid IPv4 address");
        return;
    }

    if (request->hasParam("deviceName", true)) {
        settings_->set("deviceName", postValue(request, "deviceName"));
    }
    if (request->hasParam("ssid", true)) {
        settings_->set("ssid", postValue(request, "ssid"));
    }
    if (request->hasParam("ip", true)) {
        settings_->set("localIP", localIp);
    }
    if (request->hasParam("gateway", true)) {
        settings_->set("gateway", gateway);
    }
    // An empty password field means "keep the stored one"; clearing needs the explicit flag.
    const AsyncWebParameter* password = request->getParam("pass", true);
    if (postValue(request, "clearPass") == "true") {
        settings_->set("password", "");
    } else if (password != nullptr && !password->value().isEmpty()) {
        settings_->set("password", password->value());
    }

    log_->info("Settings changed via web interface, restarting");
    requestRestart(kRestartDelayMs);
    sendResult(request, 200, "Saved. The device restarts and connects to the configured WiFi.");
}

void WebUi::handleAutoUpdatesPost(AsyncWebServerRequest* request) {
    if (!request->hasParam("enabled", true)) {
        sendResult(request, 400, "missing parameter 'enabled'");
        return;
    }
    const bool enabled = postValue(request, "enabled") == "true";
    if (settings_->set("autoUpdates", enabled) != ESP_OK) {
        sendResult(request, 500, "could not save setting");
        return;
    }
    sendResult(request, 200, enabled ? "automatic updates on" : "automatic updates off");
}

void WebUi::handleInstallPost(AsyncWebServerRequest* request) {
    {
        std::lock_guard<std::mutex> lock(installMutex_);
        installTag_ = postValue(request, "version"); // empty = latest found by the last check
    }
    installRequested_.store(true);
    sendResult(request, 202, "installing");
}

void WebUi::loop() {
    if (checkRequested_.exchange(false)) {
        updater_->check();
    }
    if (installRequested_.exchange(false)) {
        String tag;
        {
            std::lock_guard<std::mutex> lock(installMutex_);
            tag = installTag_;
        }
        if (updater_->install(tag)) {
            requestRestart(kRestartDelayMs); // lets the page fetch the "installed" state first
        }
    }
    if (restartRequested_.load() && static_cast<int32_t>(millis() - restartAtMs_.load()) >= 0) {
        ESP.restart();
    }
}

} // namespace pu
