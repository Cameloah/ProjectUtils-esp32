#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <mutex>

#include "Config.h"
#include "LineQueue.h"

namespace pu {

class Console;
class GithubUpdater;
class RamLog;
class Settings;
class WifiManager;

/// Web server with the settings page, its JSON API and (optionally) the web console.
///
/// Request handlers run on the network task and therefore never block: anything slow (update
/// check, flashing, restart) is only requested there and carried out by loop().
class WebUi {
  public:
    void begin(const Config& config, Settings& settings, WifiManager& wifi, GithubUpdater& updater,
               RamLog& log, Console& console);

    /// Must be called regularly from loop().
    void loop();

    /// The underlying server, for registering the project's own routes.
    AsyncWebServer& server() { return server_; }

    /// Restarts the device from loop() after the given delay. Safe from any task.
    void requestRestart(uint32_t delayMs);

    /// Returns true if the request may proceed; otherwise an authentication challenge was sent.
    /// Use this to protect your own routes with the admin password.
    bool authorize(AsyncWebServerRequest* request) const;

  private:
    void registerRoutes();
    void beginWebSerial(Console& console);
    void pumpWebSerial();
    void handleStatus(AsyncWebServerRequest* request) const;
    void handleSettingsPost(AsyncWebServerRequest* request);
    void handleAutoUpdatesPost(AsyncWebServerRequest* request);
    void handleInstallPost(AsyncWebServerRequest* request);

    AsyncWebServer server_{80};
    const Config* config_ = nullptr;
    Settings* settings_ = nullptr;
    WifiManager* wifi_ = nullptr;
    GithubUpdater* updater_ = nullptr;
    RamLog* log_ = nullptr;

    std::atomic<bool> checkRequested_{false};
    std::atomic<bool> installRequested_{false};
    std::atomic<bool> restartRequested_{false};
    std::atomic<uint32_t> restartAtMs_{0};
    std::mutex installMutex_;
    String installTag_;

    // Console output waiting for the web console. Filled from any task, drained by loop(), so
    // the WebSerial library itself is only ever touched from one task.
    static const size_t kWebConsoleQueueSize = 4096;
    std::mutex webConsoleMutex_;
    LineQueue<kWebConsoleQueueSize> webConsoleOut_;
    uint32_t webConsoleLastWriteUs_ = 0;
};

} // namespace pu
