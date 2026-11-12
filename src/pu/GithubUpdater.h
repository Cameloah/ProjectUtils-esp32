#pragma once

#include <Arduino.h>
#include <mutex>

#include "Config.h"

namespace pu {

class RamLog;

enum class UpdateState : uint8_t {
    Idle,       ///< Nothing checked yet.
    Checking,   ///< Looking up the latest release.
    UpToDate,   ///< Running the latest release (or newer).
    Available,  ///< A newer release exists.
    Installing, ///< Download and flashing in progress.
    Installed,  ///< New firmware flashed; a restart activates it.
    Failed,     ///< Last check or installation failed; see message.
};

struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    String latestTag; ///< Tag of the latest release found by check(), e.g. "v1.2.3".
    String message;   ///< Human-readable detail for Failed.
};

/// Firmware updates from the releases of a public GitHub repository.
///
/// check() and install() block for seconds and must run on the main loop task, never inside a
/// web request handler. Note: the HTTPS certificate is not verified.
class GithubUpdater {
  public:
    void begin(const Config& config, RamLog& log);

    /// True if a repository is configured.
    bool configured() const;

    /// Looks up the latest release and compares it to config.firmwareVersion.
    UpdateState check();

    /// Downloads and flashes the release with the given tag (e.g. "v1.2.3"; older versions are
    /// allowed). Empty tag = latest release found by check(). Returns true when a restart is
    /// needed to activate the new firmware.
    bool install(const String& tag = String());

    /// Snapshot of the current state. Safe from any task.
    UpdateStatus status() const;

    static const char* stateName(UpdateState state);

  private:
    void setState(UpdateState state);
    void fail(const String& message);
    String assetUrl(const String& tag, const String& assetTemplate) const;

    const Config* config_ = nullptr;
    RamLog* log_ = nullptr;
    mutable std::mutex mutex_;
    UpdateStatus status_;
};

} // namespace pu
