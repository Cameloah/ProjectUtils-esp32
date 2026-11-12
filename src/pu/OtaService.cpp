#include "OtaService.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "RamLog.h"

namespace pu {

namespace {
RamLog* otaLog = nullptr;

const char* otaErrorName(ota_error_t error) {
    switch (error) {
    case OTA_AUTH_ERROR:
        return "authentication failed";
    case OTA_BEGIN_ERROR:
        return "begin failed";
    case OTA_CONNECT_ERROR:
        return "connect failed";
    case OTA_RECEIVE_ERROR:
        return "receive failed";
    case OTA_END_ERROR:
        return "end failed";
    default:
        return "unknown error";
    }
}
} // namespace

void OtaService::begin(const String& hostname, const String& password, RamLog& log) {
    otaLog = &log;
    ArduinoOTA.setHostname(hostname.c_str());
    if (!password.isEmpty()) {
        ArduinoOTA.setPassword(password.c_str());
    }
    ArduinoOTA.onStart([]() {
        otaLog->info("OTA upload started (%s)",
                     ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
    });
    ArduinoOTA.onEnd([]() { otaLog->info("OTA upload finished"); });
    ArduinoOTA.onError(
        [](ota_error_t error) { otaLog->error("OTA upload: %s", otaErrorName(error)); });
    ArduinoOTA.begin(); // also starts mDNS with the hostname set above
    MDNS.addService("http", "tcp", 80);
    started_ = true;
}

void OtaService::loop() {
    if (started_) {
        ArduinoOTA.handle();
    }
}

} // namespace pu
