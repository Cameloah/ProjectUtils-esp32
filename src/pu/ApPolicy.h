#pragma once

#include <cstdint>

namespace pu {

/// When the setup access point opens.
enum class ApMode : uint8_t {
    /// Only while no WiFi is stored, so a device can always be set up.
    Never = 0,
    /// When WiFi is not stored, cannot be joined at start-up, or is lost for a while.
    OnFailure = 1,
    /// From start-up on, in addition to the WiFi connection.
    Always = 2,
};

/// Decides when the setup access point opens and closes. Pure logic without hardware access, so
/// it can be tested on the host; WifiManager feeds it the WiFi state and carries out the result.
///
/// Rules:
/// - Start-up: open when no WiFi is stored (every mode), with Always, or with OnFailure when the
///   connection failed.
/// - While running (OnFailure only): open when WiFi stays lost for longer than the grace period.
/// - Close when WiFi is back (not with Always), or when nobody used the access point for the
///   timeout. A connected client always keeps it open.
/// - After the timeout closed it, it opens again only once WiFi was back and is lost again, so a
///   long outage does not make it close and reopen over and over.
class ApPolicy {
  public:
    enum class Action : uint8_t { None, Open, Close };

    struct Options {
        ApMode mode = ApMode::OnFailure;
        uint32_t timeoutMs = 0; ///< Close after this long without clients. 0 = never.
        uint32_t graceMs = 0;   ///< OnFailure: how long WiFi must be lost before opening.
    };

    explicit ApPolicy(const Options& options) : options_(options) {}

    /// Whether to open the access point right after the first connection attempt.
    bool openAtStart(bool hasCredentials, bool connected) const {
        if (!hasCredentials || options_.mode == ApMode::Always) {
            return true;
        }
        return options_.mode == ApMode::OnFailure && !connected;
    }

    /// Call once the access point actually opened (at start-up or after Action::Open).
    void opened(uint32_t now) {
        open_ = true;
        idleSinceMs_ = now;
    }

    /// Call regularly. `clients` is the number of devices connected to the access point.
    Action update(uint32_t now, bool hasCredentials, bool connected, unsigned clients) {
        if (connected) {
            lostSinceMs_ = now;
            suppressed_ = false;
        }
        if (open_) {
            return updateOpen(now, connected, clients);
        }
        const bool mayReopen = options_.mode == ApMode::OnFailure && hasCredentials && !suppressed_;
        if (!connected && mayReopen && now - lostSinceMs_ >= options_.graceMs) {
            return Action::Open;
        }
        return Action::None;
    }

    /// Call once the access point was closed after Action::Close.
    void closed() { open_ = false; }

    /// Call when opening after Action::Open failed; the next attempt waits another grace period.
    void openFailed(uint32_t now) { lostSinceMs_ = now; }

    bool isOpen() const { return open_; }

  private:
    Action updateOpen(uint32_t now, bool connected, unsigned clients) {
        if (clients > 0) {
            idleSinceMs_ = now;
            return Action::None;
        }
        if (options_.timeoutMs > 0 && now - idleSinceMs_ >= options_.timeoutMs) {
            suppressed_ = !connected; // stay closed for the rest of this outage
            return Action::Close;
        }
        if (connected && options_.mode != ApMode::Always) {
            return Action::Close;
        }
        return Action::None;
    }

    Options options_;
    bool open_ = false;
    bool suppressed_ = false;
    uint32_t idleSinceMs_ = 0;
    uint32_t lostSinceMs_ = 0; ///< Last time WiFi was seen connected (or start-up).
};

} // namespace pu
