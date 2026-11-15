#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include "Config.h"
#include "LineQueue.h"
#include "Memory.h"
#include "Page.h"

namespace pu {

class Clock;
class Console;
class GithubUpdater;
class RamLog;
class Settings;
class WifiManager;
struct IpConfig;

/// Web server with the built-in pages (settings, update, console, device info), their JSON
/// API, and the project's own pages. All pages share one menu and one stylesheet.
///
/// Request handlers run on the network task and therefore never block: anything slow (update
/// check, flashing, restart) is only requested there and carried out by loop().
class WebUi {
  public:
    /// Settings key of the admin password set on the settings page (secret).
    static constexpr const char* kAdminPasswordKey = "adminPass";
    /// Registers the admin password key in the library settings. Before settings.loadAll().
    static void defineSettings(Settings& settings);

    void begin(const Config& config, Settings& settings, WifiManager& wifi, GithubUpdater& updater,
               RamLog& log, Console& console, Clock& clock);

    /// Must be called regularly from loop().
    void loop();

    // --- the project's own pages; call from setup() ---

    /// The project's start page at "/", declared in C++. Without one, "/" shows the device info.
    Page& home(const char* title) { return page("/", title); }
    /// A further page declared in C++, listed in the menu under `title`.
    Page& page(const char* path, const char* title);
    /// A page written by hand. `html` must stay valid forever (a string literal). Include
    /// /pu/app.css and /pu/app.js in it to get the shared style and the menu.
    void addPage(const char* path, const char* title, const char* html);
    /// A menu entry for a route the project serves itself through server().
    void addMenuLink(const char* title, const char* path);

    /// The underlying server, for registering the project's own routes.
    AsyncWebServer& server() { return server_; }

    /// Restarts the device from loop() after the given delay. Safe from any task.
    void requestRestart(uint32_t delayMs);

    /// Erases everything stored in flash (NVS: WiFi, admin password, all settings, also other
    /// libraries' values) and restarts. The firmware and a file system partition stay. Call from
    /// loop(), not from a request handler.
    void factoryReset();

    /// Returns true if the request may proceed; otherwise an authentication challenge was sent.
    /// Use this to protect your own routes with the admin password.
    bool authorize(AsyncWebServerRequest* request) const;

  private:
    struct MenuEntry {
        String path;
        String title;
    };

    void registerRoutes();
    void registerPageApi();
    void serveAsset(const char* path, const char* contentType, const char* data, size_t length,
                    bool isProtected);
    Page* findPage(AsyncWebServerRequest* request, bool post);
    bool hasCredentials(AsyncWebServerRequest* request) const;
    String adminPassword() const;
    bool saveAdminPassword(AsyncWebServerRequest* request, bool write, String& error);

    void beginConsole(Console& console);
    void pumpConsole();
    void onConsoleEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
                        size_t length);

    void registerMemoryApi();
    void handleNav(AsyncWebServerRequest* request) const;
    void handleStatus(AsyncWebServerRequest* request) const;
    void handleInfo(AsyncWebServerRequest* request) const;
    void handleSettingsPost(AsyncWebServerRequest* request);
    bool arrivedViaAp(AsyncWebServerRequest* request) const;
    bool readIpConfig(AsyncWebServerRequest* request, IpConfig& out, String& error) const;
    bool saveApSettings(AsyncWebServerRequest* request, String& error);
    void handleAutoUpdatesPost(AsyncWebServerRequest* request);
    void handleInstallPost(AsyncWebServerRequest* request);
    void handleScanGet(AsyncWebServerRequest* request) const;
    void handleUploadChunk(AsyncWebServerRequest* request, size_t index, const uint8_t* data,
                           size_t length, bool final);
    void handleUploadDone(AsyncWebServerRequest* request);

    AsyncWebServer server_{80};
    const Config* config_ = nullptr;
    Settings* settings_ = nullptr;
    WifiManager* wifi_ = nullptr;
    GithubUpdater* updater_ = nullptr;
    RamLog* log_ = nullptr;
    Console* console_ = nullptr;
    Clock* clock_ = nullptr;

    Memory memory_;
    std::deque<Page> pages_; // deque: references handed out by page() stay valid
    std::vector<MenuEntry> menu_;
    uint32_t sketchSize_ = 0;

    std::atomic<bool> checkRequested_{false};
    std::atomic<bool> installRequested_{false};
    std::atomic<bool> restartRequested_{false};
    std::atomic<bool> factoryResetRequested_{false};
    std::atomic<uint32_t> restartAtMs_{0};
    std::mutex installMutex_;
    String installTag_;

    // File upload from the update page. One upload at a time.
    bool uploadStarted_ = false;
    String uploadError_;

    // Web console. Output is queued from any task and sent by loop(), so the socket is only
    // written from one task. The scrollback is what a newly opened console page gets to see.
    static const size_t kConsoleQueueSize = 4096;
    AsyncWebSocket consoleSocket_{"/api/pu/console/ws"};
    String consoleToken_;
    std::mutex consoleMutex_;
    LineQueue<kConsoleQueueSize> consoleOut_;
    String consoleScrollback_;
    uint32_t consoleLastCleanupMs_ = 0;
};

} // namespace pu
