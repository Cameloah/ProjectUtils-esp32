#include "main_project_utils.h"

/*
 * This is the main file that needs to be implemented in a running project.
 * From here one can enable/disable adjacent modules such as a github updater, OTA functionality or webserial.
 */

#ifdef ENABLE_MEMORY
    #include "memory_module.h"
    MemoryModule device_settings;
#endif

// ------------ Dual Serial ------------- //

DualPrint DualSerial(0);

int DualPrint::available()
{
    if (uartAvailable(_uart))
        return uartAvailable(_uart);

    else if (buffer_webserial.length())
        return buffer_webserial.length();

    else return 0;
}

size_t DualPrint::readBytes(char *buffer, size_t length)
{
    if (uartAvailable(_uart)) {
        size_t count = 0;
        while (count < length) {
            int c = timedRead();
            if (c < 0) {
                break;
            }
            *buffer++ = (char) c;
            count++;
        }
        return count;
    }

    else {
        uint16_t len_webbuffer = buffer_webserial.length();
        if (length < len_webbuffer)
            return -1;
        strcpy(buffer, buffer_webserial.c_str());
        buffer_webserial = "";
        return len_webbuffer;
    }
}

size_t DualPrint::write(const uint8_t *buffer, size_t size) {
   
    #ifdef ENABLE_WEBSERIAL
        if (use_webSerial) WebSerial.print((char *) buffer);
    #endif
    size_t n = 0;
    while(size--) {
        n += HardwareSerial::write(*buffer++);
    }
    return n;
}


// --------- member functions ----------- //

void project_utils_init(const String& ap_name = "New ESP-Device") {

    #ifdef ENABLE_MEMORY
        device_settings.addParameter("autoUpdates", false);
        device_settings.loadAllStrict();
    #endif
    


    #ifdef ENABLE_WIFI
        // try to connect to wifi
        uint8_t retval;
        if((retval = network_manager_init(ap_name)) != NETWORK_MANAGER_ERROR_NO_ERROR) 
            ram_log_notify(RAM_LOG_ERROR_NETWORK_MANAGER, retval);

        // init time module
        time_module_init();
    #endif

    #ifdef ENABLE_GITHUBUPDATE
        // check for updates if configured
        if(*device_settings.getBool("autoUpdates") && network_manager_is_connected()) {
            if(github_update_checkforlatest() == GITHUB_UPDATE_ERROR_NO_ERROR)
                if((retval = github_update_firmwareUpdate()) != GITHUB_UPDATE_ERROR_NO_ERROR)
                    ram_log_notify(RAM_LOG_ERROR_GITHUB_UPDATE, retval);
        }
    #endif
    
    // *****----- SERVER MODULES -----*****

    // Systems that do not need SPIFFS
    #ifdef ENABLE_WEBSERIAL
        webserial_monitor_init();
    #endif

    #ifdef ENABLE_OTA
        network_ota_init();
    #endif

    #ifdef ENABLE_WIFI
        server_setup();
    #endif
}


void project_utils_update() {
    #ifdef ENABLE_OTA
        network_ota_update();
    #endif

    #ifdef ENABLE_WIFI
        network_manager_update();
        server_update();
    #endif
}