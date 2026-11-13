#pragma once

#include <cstdint>

namespace pu {

/// State of a hold-to-run button that is kept alive by repeated messages from the browser and
/// lets go by itself when they stop (closed tab, lost WiFi). Pure logic, unit-tested on the host.
class HoldState {
  public:
    /// A press or keep-alive arrived. Returns true if this started a new hold.
    bool touch(uint32_t nowMs) {
        lastMs_ = nowMs;
        if (held_) {
            return false;
        }
        held_ = true;
        return true;
    }

    /// The button was let go. Returns true if a hold ended.
    bool release() {
        if (!held_) {
            return false;
        }
        held_ = false;
        return true;
    }

    /// Ends the hold if nothing arrived for `timeoutMs`. Returns true if it did.
    /// Works across the 49-day rollover of the millisecond counter.
    bool expire(uint32_t nowMs, uint32_t timeoutMs) {
        if (held_ && nowMs - lastMs_ >= timeoutMs) {
            held_ = false;
            return true;
        }
        return false;
    }

    bool held() const { return held_; }

  private:
    bool held_ = false;
    uint32_t lastMs_ = 0;
};

} // namespace pu
