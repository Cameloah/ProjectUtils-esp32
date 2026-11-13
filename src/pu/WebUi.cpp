#include "WebUi.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "Clock.h"
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
const char kHtml[] = "text/html";
const size_t kConsoleLineSize = 256;        // longer lines reach the web console in pieces
const size_t kConsoleFrameSize = 1024;      // lines sent to the browser in one message
const size_t kConsoleScrollbackSize = 2048; // recent output shown when a console page opens
const uint32_t kConsoleCleanupMs = 2000;

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

const char* resetReasonText() {
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
        return "power on";
    case ESP_RST_SW:
        return "restart requested by software";
    case ESP_RST_PANIC:
        return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "watchdog (firmware hung)";
    case ESP_RST_DEEPSLEEP:
        return "wake from deep sleep";
    case ESP_RST_BROWNOUT:
        return "supply voltage dropped";
    case ESP_RST_EXT:
        return "reset pin";
    default:
        return "unknown";
    }
}

String randomToken() {
    char text[17];
    snprintf(text, sizeof(text), "%08x%08x", static_cast<unsigned>(esp_random()),
             static_cast<unsigned>(esp_random()));
    return String(text);
}
} // namespace

void WebUi::begin(const Config& config, Settings& settings, WifiManager& wifi,
                  GithubUpdater& updater, RamLog& log, Console& console, Clock& clock) {
    config_ = &config;
    settings_ = &settings;
    wifi_ = &wifi;
    updater_ = &updater;
    log_ = &log;
    console_ = &console;
    clock_ = &clock;
    sketchSize_ = ESP.getSketchSize(); // reads the firmware image, too slow for a request handler

    registerRoutes();
    registerPageApi();
    if (config_->enableConsole) {
        beginConsole(console);
    }
    server_.begin();
}

bool WebUi::hasCredentials(AsyncWebServerRequest* request) const {
    return config_ == nullptr || config_->adminPassword.isEmpty() ||
           request->authenticate(config_->adminUser.c_str(), config_->adminPassword.c_str());
}

bool WebUi::authorize(AsyncWebServerRequest* request) const {
    if (hasCredentials(request)) {
        return true;
    }
    request->requestAuthentication();
    return false;
}

void WebUi::requestRestart(uint32_t delayMs) {
    restartAtMs_.store(millis() + delayMs);
    restartRequested_.store(true);
}

// ---------- project pages ----------

Page& WebUi::page(const char* path, const char* title) {
    for (Page& existing : pages_) {
        if (existing.path() == path) {
            return existing;
        }
    }
    pages_.emplace_back(path, title);
    menu_.push_back({path, title});
    server_.on(path, HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            request->send(200, kHtml, reinterpret_cast<const uint8_t*>(kWebPageHtml),
                          sizeof(kWebPageHtml) - 1);
        }
    });
    return pages_.back();
}

void WebUi::addPage(const char* path, const char* title, const char* html) {
    menu_.push_back({path, title});
    server_.on(path, HTTP_GET, [this, html](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            request->send(200, kHtml, reinterpret_cast<const uint8_t*>(html), strlen(html));
        }
    });
}

void WebUi::addMenuLink(const char* title, const char* path) {
    menu_.push_back({path, title});
}

Page* WebUi::findPage(AsyncWebServerRequest* request, bool post) {
    const AsyncWebParameter* path = request->getParam("path", post);
    if (path != nullptr) {
        for (Page& candidate : pages_) {
            if (candidate.path() == path->value()) {
                return &candidate;
            }
        }
    }
    sendResult(request, 404, "no such page");
    return nullptr;
}

void WebUi::registerPageApi() {
    // The longer paths first: a route also matches everything below it.
    server_.on("/api/pu/page/values", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!authorize(request)) {
            return;
        }
        Page* target = findPage(request, false);
        if (target != nullptr) {
            request->send(200, kJson, target->valuesJson());
        }
    });
    server_.on("/api/pu/page/widget", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!authorize(request)) {
            return;
        }
        Page* target = findPage(request, true);
        if (target == nullptr) {
            return;
        }
        double index = -1.0;
        if (!parseNumber(postValue(request, "widget").c_str(), index) || index < 0.0 ||
            !isWholeNumber(index)) {
            sendResult(request, 400, "missing or invalid parameter 'widget'");
            return;
        }
        const AsyncWebParameter* valueParam = request->getParam("value", true);
        const String* value = valueParam != nullptr ? &valueParam->value() : nullptr;
        String message = "ok";
        const int code = target->handleAction(static_cast<size_t>(index),
                                              postValue(request, "action"), value, message);
        sendResult(request, code, message.c_str());
    });
    server_.on("/api/pu/page", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!authorize(request)) {
            return;
        }
        Page* target = findPage(request, false);
        if (target != nullptr) {
            request->send(200, kJson, target->layoutJson());
        }
    });
}

// ---------- web console ----------

void WebUi::beginConsole(Console& console) {
    consoleToken_ = randomToken();
    // Browsers do not reliably send the login along when opening a websocket, so the console
    // page fetches a token through the protected API and presents that instead.
    consoleSocket_.handleHandshake([this](AsyncWebServerRequest* request) {
        const AsyncWebParameter* token = request->getParam("token");
        return hasCredentials(request) || (token != nullptr && token->value() == consoleToken_);
    });
    consoleSocket_.onEvent([this](AsyncWebSocket* /*socket*/, AsyncWebSocketClient* client,
                                  AwsEventType type, void* arg, uint8_t* data, size_t length) {
        onConsoleEvent(client, type, arg, data, length);
    });
    server_.addHandler(&consoleSocket_);
    server_.on("/api/pu/console/token", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            JsonDocument doc;
            doc["token"] = consoleToken_;
            sendJson(request, 200, doc);
        }
    });
    serveAsset("/console", kHtml, kWebConsoleHtml, sizeof(kWebConsoleHtml) - 1, true);
    server_.on("/webserial", HTTP_GET, // address of the console in older versions
               [](AsyncWebServerRequest* request) { request->redirect("/console"); });

    console.setMirror([this](const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(consoleMutex_);
        consoleOut_.push(data, length);
    });
}

void WebUi::onConsoleEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg,
                           uint8_t* data, size_t length) {
    if (type == WS_EVT_CONNECT) {
        String scrollback;
        {
            std::lock_guard<std::mutex> lock(consoleMutex_);
            scrollback = consoleScrollback_;
        }
        if (!scrollback.isEmpty()) {
            client->text(scrollback);
        }
        return;
    }
    if (type != WS_EVT_DATA) {
        return;
    }
    const AwsFrameInfo* info = static_cast<const AwsFrameInfo*>(arg);
    const bool wholeMessage = info->final != 0 && info->index == 0 && info->len == length;
    if (!wholeMessage || info->opcode != WS_TEXT) {
        return; // commands are short; fragmented or binary messages are not expected
    }
    static const uint8_t kNewline = '\n';
    while (length > 0 &&
           (data[length - 1] == '\0' || data[length - 1] == '\n' || data[length - 1] == '\r')) {
        --length;
    }
    console_->injectInput(data, length);
    console_->injectInput(&kNewline, 1);
}

void WebUi::pumpConsole() {
    if (!config_->enableConsole) {
        return;
    }
    const uint32_t now = millis();
    if (now - consoleLastCleanupMs_ >= kConsoleCleanupMs) {
        consoleLastCleanupMs_ = now;
        consoleSocket_.cleanupClients();
    }
    const bool hasClients = consoleSocket_.count() > 0;
    if (hasClients && !consoleSocket_.availableForWriteAll()) {
        return; // a browser is slow; keep the lines queued instead of losing them
    }

    // Collect the complete lines that are waiting into one message, separated by '\n'.
    String frame;
    {
        std::lock_guard<std::mutex> lock(consoleMutex_);
        char line[kConsoleLineSize];
        while (frame.length() < kConsoleFrameSize) {
            const int length = consoleOut_.pop(line, sizeof(line));
            if (length < 0) {
                break;
            }
            if (!frame.isEmpty()) {
                frame += '\n';
            }
            frame += length > 0 ? line : " "; // keep blank lines visible
        }
        if (frame.isEmpty()) {
            return;
        }
        if (!consoleScrollback_.isEmpty()) {
            consoleScrollback_ += '\n';
        }
        consoleScrollback_ += frame;
        if (consoleScrollback_.length() > kConsoleScrollbackSize) {
            const int excess =
                static_cast<int>(consoleScrollback_.length() - kConsoleScrollbackSize);
            const int lineEnd = consoleScrollback_.indexOf('\n', excess);
            consoleScrollback_.remove(0, lineEnd >= 0 ? lineEnd + 1 : excess);
        }
    }
    if (hasClients) {
        consoleSocket_.textAll(frame);
    }
}

// ---------- routes ----------

void WebUi::serveAsset(const char* path, const char* contentType, const char* data, size_t length,
                       bool isProtected) {
    server_.on(path, HTTP_GET,
               [this, contentType, data, length, isProtected](AsyncWebServerRequest* request) {
                   if (!isProtected || authorize(request)) {
                       request->send(200, contentType, reinterpret_cast<const uint8_t*>(data),
                                     length);
                   }
               });
}

void WebUi::registerRoutes() {
    // Style and script are public so that the login prompt belongs to the page itself.
    serveAsset("/pu/app.css", "text/css", kWebAppCss, sizeof(kWebAppCss) - 1, false);
    serveAsset("/pu/app.js", "text/javascript", kWebAppJs, sizeof(kWebAppJs) - 1, false);
    serveAsset("/settings", kHtml, kWebSettingsHtml, sizeof(kWebSettingsHtml) - 1, true);
    serveAsset("/update", kHtml, kWebUpdateHtml, sizeof(kWebUpdateHtml) - 1, true);
    serveAsset("/info", kHtml, kWebInfoHtml, sizeof(kWebInfoHtml) - 1, true);

    server_.on("/api/pu/nav", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleNav(request);
        }
    });
    server_.on("/api/pu/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleStatus(request);
        }
    });
    server_.on("/api/pu/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleInfo(request);
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
    server_.on("/api/pu/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            handleScanGet(request);
        }
    });
    server_.on("/api/pu/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!authorize(request)) {
            return;
        }
        WiFi.scanDelete();
        if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
            sendResult(request, 503, "the search could not be started, try again");
        } else {
            sendResult(request, 202, "searching");
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
    server_.on(
        "/api/pu/update/upload", HTTP_POST,
        [this](AsyncWebServerRequest* request) { handleUploadDone(request); },
        [this](AsyncWebServerRequest* request, const String& /*filename*/, size_t index,
               uint8_t* data, size_t length,
               bool final) { handleUploadChunk(request, index, data, length, final); });
    server_.on("/api/pu/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (authorize(request)) {
            requestRestart(kRestartDelayMs);
            sendResult(request, 202, "restarting");
        }
    });

    server_.onNotFound([this](AsyncWebServerRequest* request) {
        if (wifi_->portalActive()) {
            // While the setup access point is open every unknown URL leads to the settings
            // page, which is what makes phones and laptops show the captive portal.
            request->redirect("http://" + WiFi.softAPIP().toString() + "/settings");
        } else if (request->url() == "/") {
            request->redirect("/info"); // the project defined no start page
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

void WebUi::handleNav(AsyncWebServerRequest* request) const {
    JsonDocument doc;
    doc["deviceName"] = wifi_->deviceName();
    JsonArray project = doc["project"].to<JsonArray>();
    for (const MenuEntry& entry : menu_) {
        JsonObject item = project.add<JsonObject>();
        item["path"] = entry.path;
        item["title"] = entry.title;
    }
    JsonArray device = doc["device"].to<JsonArray>();
    const auto add = [&device](const char* path, const char* title) {
        JsonObject item = device.add<JsonObject>();
        item["path"] = path;
        item["title"] = title;
    };
    add("/settings", "Settings");
    add("/update", "Update");
    if (config_->enableConsole) {
        add("/console", "Console");
    }
    add("/info", "Device info");
    sendJson(request, 200, doc);
}

void WebUi::handleInfo(AsyncWebServerRequest* request) const {
    char version[24];
    config_->firmwareVersion.format(version, sizeof(version));

    JsonDocument doc;
    doc["deviceName"] = wifi_->deviceName();
    doc["hostname"] = wifi_->hostname();
    doc["firmware"] = version;
    doc["connected"] = wifi_->isConnected();
    doc["mode"] = WifiManager::modeName();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["time"] = clock_->now();
    doc["uptimeSeconds"] = static_cast<uint32_t>(esp_timer_get_time() / 1000000);
    doc["resetReason"] = resetReasonText();
    doc["heapFree"] = ESP.getFreeHeap();
    doc["heapMinFree"] = ESP.getMinFreeHeap();
    doc["heapSize"] = ESP.getHeapSize();
    doc["sketchSize"] = sketchSize_;
    doc["sketchFree"] = ESP.getFreeSketchSpace() - sketchSize_;
    doc["chip"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["cores"] = ESP.getChipCores();
    doc["cpuMHz"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["sdk"] = ESP.getSdkVersion();
    sendJson(request, 200, doc);
}

void WebUi::handleScanGet(AsyncWebServerRequest* request) const {
    const int16_t count = WiFi.scanComplete();
    JsonDocument doc;
    doc["scanning"] = count == WIFI_SCAN_RUNNING;
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int16_t index = 0; index < count; ++index) {
        const String ssid = WiFi.SSID(index);
        bool known = ssid.isEmpty(); // hidden networks cannot be picked from a list
        for (JsonObject network : networks) {
            known = known || network["ssid"] == ssid; // results are sorted, strongest first
        }
        if (!known) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = ssid;
            network["rssi"] = WiFi.RSSI(index);
            network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        }
    }
    sendJson(request, 200, doc);
}

// ---------- file upload ----------

void WebUi::handleUploadChunk(AsyncWebServerRequest* request, size_t index, const uint8_t* data,
                              size_t length, bool final) {
    if (index == 0) {
        uploadStarted_ = true;
        uploadError_ = "";
        if (!hasCredentials(request)) {
            uploadError_ = "login required";
            return;
        }
        const AsyncWebParameter* type = request->getParam("type");
        const AsyncWebParameter* size = request->getParam("size");
        const AsyncWebParameter* md5 = request->getParam("md5");
        const bool filesystem = type != nullptr && type->value() == "filesystem";
        double bytes = 0.0;
        const bool sizeKnown = size != nullptr && parseNumber(size->value().c_str(), bytes) &&
                               bytes > 0.0 && isWholeNumber(bytes);
        if (sizeKnown && !filesystem && bytes > ESP.getFreeSketchSpace()) {
            uploadError_ = "the file is larger than the space for firmware on this device";
            return;
        }
        log_->info("Upload of %s started", filesystem ? "filesystem image" : "firmware");
        if (!Update.begin(sizeKnown ? static_cast<size_t>(bytes) : UPDATE_SIZE_UNKNOWN,
                          filesystem ? U_SPIFFS : U_FLASH)) {
            uploadError_ = Update.errorString();
            return;
        }
        if (md5 != nullptr && !Update.setMD5(md5->value().c_str())) {
            uploadError_ = "invalid MD5 checksum";
            Update.abort();
            return;
        }
        // A browser that goes away half-way must not leave the update open forever.
        request->onDisconnect([]() {
            if (Update.isRunning()) {
                Update.abort();
            }
        });
    }
    if (!uploadError_.isEmpty()) {
        return;
    }
    if (length > 0 && Update.write(const_cast<uint8_t*>(data), length) != length) {
        uploadError_ = Update.errorString();
        Update.abort();
        return;
    }
    if (final && !Update.end(true)) {
        uploadError_ = Update.errorString();
    }
}

void WebUi::handleUploadDone(AsyncWebServerRequest* request) {
    if (!authorize(request)) {
        return;
    }
    if (!uploadStarted_) {
        sendResult(request, 400, "no file received");
        return;
    }
    uploadStarted_ = false;
    if (!uploadError_.isEmpty()) {
        log_->error("Upload failed: %s", uploadError_.c_str());
        sendResult(request, 400, uploadError_.c_str());
        return;
    }
    log_->info("Upload installed, restarting");
    requestRestart(kRestartDelayMs);
    sendResult(request, 200, "Installed. The device restarts now.");
}

// ---------- status and settings ----------

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
    pumpConsole();
    for (Page& target : pages_) {
        target.loop();
    }
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
