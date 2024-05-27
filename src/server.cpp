#include "Arduino.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

#include "webserial_monitor.h"
#include "network_ota.h"
#include "network_manager.h"
#include "memory_module.h"
#include "main_project_utils.h"
#include "github_update.h"



// -------------- server -------------- //

AsyncWebServer server(80);



// ---------- http parameters --------- //

const char* GENERAL_INPUT_1 = "Device Name";

const char* WIFI_INPUT_1 = "ssid";
const char* WIFI_INPUT_2 = "pass";
const char* WIFI_INPUT_3 = "ip";
const char* WIFI_INPUT_4 = "gateway";



// ---------- web functions ----------- //

void webfct_settings_serve(AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/settings.html", "text/html");
}

void webfct_get_general_config(AsyncWebServerRequest *request) {

    String payload = wifi_info.getString("deviceName") + "\n";
    request->send(200, "text/plain", payload);
}

void webfct_get_wifi_cred(AsyncWebServerRequest *request) {
    String pw = wifi_config.getString("password");

    String payload = wifi_config.getString("ssid") + "\n"
                    + pw + "\n"
                    + wifi_config.getString("localIP") + "\n"
                    + wifi_config.getString("gateway");
    
    request->send(200, "text/plain", payload);
}

void webfct_get_autoupdate(AsyncWebServerRequest *request) {

    String payload = String(*device_settings.getBool("autoUpdates")) + "\n";
    request->send(200, "text/plain", payload);
}

void webfct_get_newversion(AsyncWebServerRequest *request) {
    // check for new FW version
    String new_version;
    String payload;
    uint8_t retval = github_update_checkforlatest(&new_version);
    if(retval != GITHUB_UPDATE_ERROR_NO_ERROR && retval != GITHUB_UPDATE_ERROR_NO_UPDATE) {
        ram_log_notify(RAM_LOG_ERROR_GITHUB_UPDATE, retval);
        payload = "1";
    }

    else {
        payload = new_version;
    }

    request->send(200, "text/plain", payload);
}

void webfct_settings_post(AsyncWebServerRequest *request) {

    int params = request->params();
    for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
            // save general settings
            if (p->name() == GENERAL_INPUT_1) {
                wifi_info.set("deviceName", p->value(), true);
                DualSerial.print("Device name set to: ");
                DualSerial.println(p->value());
            }
            // save wifi settings
            if (p->name() == WIFI_INPUT_1) {
                wifi_config.set("ssid", p->value(), true);
                DualSerial.print("SSID set to: ");
                DualSerial.println(p->value());
            }
    
            if (p->name() == WIFI_INPUT_2) {
                wifi_config.set("password", p->value(), true);
                DualSerial.print("Password set to: ");
                DualSerial.println(p->value());
            }
            
            if (p->name() == WIFI_INPUT_3) {
                wifi_config.set("localIP", p->value(), true);
                DualSerial.print("IP Address set to: ");
                DualSerial.println(p->value());
            }

            if (p->name() == WIFI_INPUT_4) {
                wifi_config.set("gateway", p->value(), true);
                DualSerial.print("Gateway set to: ");
                DualSerial.println(p->value());
            }
        }
    }
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + wifi_config.getString("localIP"));
    delay(3000);
    ESP.restart();
}

void webfct_adv_settings_get(AsyncWebServerRequest *request){
    if (request->hasParam("autoUpdates")) {
        device_settings.set("autoUpdates", request->getParam("autoUpdates")->value().equals("true"), true);
    }

    else if (request->hasParam("desiredVersion")) {
        String msg = "Trying to install desired FW version " + request->getParam("desiredVersion")->value();
        DualSerial.println(msg);
        
        uint8_t retval = github_update_firmwareUpdate(request->getParam("desiredVersion")->value().c_str());
        if (retval != GITHUB_UPDATE_ERROR_NO_ERROR) {
            ram_log_notify(RAM_LOG_ERROR_GITHUB_UPDATE, retval);
            request->send(200, "text/plain", "Failed");
        }
    }
    request->send(200, "text/plain", "OK");
}



// --------- member functions ----------- //

void server_setup() {
    //initialize spiffs
    if(!SPIFFS.begin(true)){
        ram_log_notify(RAM_LOG_INFO, "SPIFFS failed to initialize!");
        return;
    }

    server.serveStatic("/", SPIFFS, "/");                                                                           // makes everything available in the browser

    server.on("/settings/advanced", HTTP_GET, webfct_adv_settings_get);                                             // Listens for GET requests and saves advanced settings
    server.on("/", HTTP_POST, webfct_settings_post);                                                                // listens for POST requests and saves user wifi credentials    
    
    server.on("/settings/general-config", HTTP_GET, webfct_get_general_config);                                     // publishes general configuration
    server.on("/settings/wifi-config", HTTP_GET, webfct_get_wifi_cred);                                             // publishes wifi credentials
    server.on("/settings/autoupdate", HTTP_GET, webfct_get_autoupdate);                                             // publishes advanced settings
    server.on("/settings/newversion", HTTP_GET, webfct_get_newversion);

    server.on("/settings", HTTP_GET, webfct_settings_serve);                                                        // serves /settings page


    server.begin();
}
