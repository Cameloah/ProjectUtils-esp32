//
// Created by Cameloah on 05.02.2023.
//

#include "network_ota.h"
#include "Arduino.h"
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "webserial_monitor.h"
#include "ram_log.h"


void network_ota_init() {
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname("myesp8266");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {  // U_FS
            type = "filesystem";
        }

        // NOTE: if updating FS this would be the place to unmount FS using FS.end()
        DualSerial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        DualSerial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        DualSerial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        String error_str = "OTA Error[%u]: ";
        if (error == OTA_AUTH_ERROR) {
            error_str.concat("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            error_str.concat("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            error_str.concat("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            error_str.concat("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            error_str.concat("End Failed");
        }
        ram_log_notify(RAM_LOG_INFO, error_str.c_str(), true);
    });
    ArduinoOTA.begin();
}

void network_ota_update() {
    ArduinoOTA.handle();
}