#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

#include "main_project_utils.h"
#include "network_manager.h"
#include "network_ota.h"
#include "server.h"
#include "memory_module.h"
#include "github_update.h"
#include "../../../include/main.h"

/*
 * This is the main file that needs to be implemented in a running project.
 * From here one can enable/disable adjacent modules such as a github updater, OTA functionality or webserial.
 */


MemoryModule device_settings;

// --------- member functions ----------- //

void project_utils_init(const String& ap_name = "New ESP-Device") {
    // initialize memory
    device_settings.addParameter("autoUpdates", false);
    device_settings.loadAllStrict();


    uint8_t retval;

    // establish connection
    if((retval = network_manager_init(ap_name)) != NETWORK_MANAGER_ERROR_NO_ERROR) 
        ram_log_notify(RAM_LOG_ERROR_NETWORK_MANAGER, retval);

    // check for updates if configured
    if(*device_settings.getBool("autoUpdates") && network_manager_is_connected()) {
        if(github_update_checkforlatest() == GITHUB_UPDATE_ERROR_NO_ERROR)
            if((retval = github_update_firmwareUpdate()) != GITHUB_UPDATE_ERROR_NO_ERROR)
                ram_log_notify(RAM_LOG_ERROR_GITHUB_UPDATE, retval);
    }

    // *****----- SERVER MODULES -----*****

    // Systems that do not need SPIFFS
#ifdef SYS_CONTROL_WEBSERIAL
    webserial_monitor_init();
#endif

    network_ota_init();

    server_setup();
}


void project_utils_update() {
    network_ota_update();
    network_manager_update();
}