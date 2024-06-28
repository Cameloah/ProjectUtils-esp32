//
// Created by Cameloah on 03.03.2022.
//

#pragma once

#include "../../../include/main.h"


#ifdef ENABLE_WIFI
    #ifndef ENABLE_MEMORY
        #error "ProjectUtils error: ENABLE_MEMORY must be defined to use ENABLE_WIFI"
    #endif
    #include <WiFi.h>
    #include <ESPAsyncWebServer.h>
    #include "SPIFFS.h"
    #include "network_manager.h"
    #include "server.h"
    #include "time_module.h"

    extern AsyncWebServer server;
#endif

#ifdef ENABLE_OTA
    #ifndef ENABLE_WIFI
        #error "ProjectUtils error: ENABLE_WIFI must be defined to use ENABLE_OTA"
    #endif
    #include "network_ota.h"
#endif

#ifdef ENABLE_GITHUBUPDATE
    #ifndef ENABLE_WIFI
        #error "ProjectUtils error: ENABLE_WIFI must be defined to use ENABLE_GITHUBUPDATE"
    #endif
    #include "github_update.h"
#endif

#ifdef ENABLE_WEBSERIAL
    #ifndef ENABLE_WIFI
        #error "ProjectUtils error: ENABLE_WIFI must be defined to use ENABLE_WEBSERIAL"
    #endif
    #include "webserial_monitor.h"
#endif

#ifdef ENABLE_MEMORY
    #include "memory_module.h"
#endif


class DualPrint : public HardwareSerial
{
public:
    explicit DualPrint(int uartNr) : HardwareSerial(uartNr), use_webSerial(false) {}

    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override;
    size_t readBytes(char *buffer, size_t length) override;

    bool use_webSerial;
    String buffer_webserial;
};

extern DualPrint DualSerial;
extern MemoryModule device_settings;


void project_utils_init(const String& ap_name);

void project_utils_update();