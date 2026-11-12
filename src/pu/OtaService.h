#pragma once

#include <Arduino.h>

namespace pu {

class RamLog;

/// Firmware upload over the network from PlatformIO / Arduino IDE (ArduinoOTA).
/// Also announces the device as <hostname>.local via mDNS.
class OtaService {
  public:
    /// password: empty = uploads are accepted from anyone on the network.
    void begin(const String& hostname, const String& password, RamLog& log);
    void loop();

  private:
    bool started_ = false;
};

} // namespace pu
