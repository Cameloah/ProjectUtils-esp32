//
// Created by Cameloah on 31.03.2023.
// The wifi manager provides an interface to setup and configure the wifi network, the esp is connecting to. 
// In the future, it should be able to store multipe sets of wifi credentials, show available networks and give the option for static or dynamic IP addresses. 
//

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include <DNSServer.h>
#include "EEPROM.h"

#include "network_manager.h"
#include "main_project_utils.h"
#include "webserial_monitor.h"
#include "ram_log.h"
#include "memory_module.h"
#include "lib_tools.h"
#include "../../../include/main.h"

// ----------- DNS server ------------ //

const byte DNS_PORT = 53;
DNSServer dnsServer;



// ---------- wifi variables ---------- //

MemoryModule wifi_config;
MemoryModule wifi_info;

int timer_wifi_connect = 0;



// ------- flags and variables -------- //

bool flag_ap_active = false;
bool flag_wifi_config_loaded = false;



// --------- member functions --------- //

NETWORK_MANAGER_ERROR_t wifi_connect() {
    // if we are in access point mode we don't look for Wi-Fi
    if (WiFi.getMode() == 2)
        return NETWORK_MANAGER_ERROR_CONNECT;

    DualSerial.println("Waiting for WiFi");

#ifdef SYS_CONTROL_STAT_IP

    IPAddress local_IP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress primaryDNS;
    IPAddress secondaryDNS;

    local_IP.fromString(wifi_config.getString("localIP"));
    gateway.fromString(wifi_config.getString("gateway"));
    subnet.fromString(wifi_info.getString("subnet"));
    primaryDNS.fromString(wifi_info.getString("primaryDNS"));
    secondaryDNS.fromString(wifi_info.getString("secondaryDNS"));

    DualSerial.println("Using static IP...");

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        DualSerial.println("Static IP failed to configure");
        return NETWORK_MANAGER_ERROR_CONFIG;
    }
#endif

    WiFi.begin(wifi_config.getString("ssid").c_str(), wifi_config.getString("password").c_str());

    while ((WiFiClass::status() != WL_CONNECTED)) {
        delay(500);
        DualSerial.print(".");
        timer_wifi_connect++;
        if (500 * timer_wifi_connect > TIMEOUT_WIFI_CONNECT_MS) {
            timer_wifi_connect = 0;
            return NETWORK_MANAGER_ERROR_CONNECT;
        }
    }

#ifdef SYS_CONTROL_STAT_IP
    if(wifi_config.getString("localIP") != WiFi.localIP().toString()) {
        DualSerial.println("Static IP failed to configure");
        return NETWORK_MANAGER_ERROR_CONFIG;
    }
#endif

    DualSerial.println("");
    String payload = "Connected to " + wifi_config.getString("ssid") + ", IP: " + WiFi.localIP().toString() + ", Wifi mode: " + network_manager_get_mode();
    ram_log_notify(RAM_LOG_INFO, payload.c_str(), true);

    return NETWORK_MANAGER_ERROR_NO_ERROR;
}

bool network_manager_is_connected() {
    return WiFi.isConnected();
}

String network_manager_get_mode() {
    switch (WiFi.getMode()) {
        case 1:
            return "STA Mode";
        case 2:
            return "AP Mode";
        case 3:
            return "STA and AP Mode";
        default:
            return "unknown";
    }
}

NETWORK_MANAGER_ERROR_t _wifi_load(MemoryModule* user_config) {

    // Load values from NVS
    if (user_config->loadAllStrict() != ESP_OK)
        return NETWORK_MANAGER_ERROR_CONFIG;

         // lets check for meaningful credentials
        if(!otherThanWhitespace(user_config->getString("ssid")) || !otherThanWhitespace(user_config->getString("password")))
            return NETWORK_MANAGER_ERROR_CONFIG;

        flag_wifi_config_loaded = true;
        return NETWORK_MANAGER_ERROR_NO_ERROR;
    }

NETWORK_MANAGER_ERROR_t _wifi_manager_AP(const String& ap_name) {
    // Connect to Wi-Fi network with SSID and password
    String payload = "Starting Access Point: " + ap_name;
    ram_log_notify(RAM_LOG_INFO, payload.c_str(), true);

    WiFi.enableAP(true);
    if(!WiFi.softAP(ap_name.c_str(), AP_PASSWORD))
        return NETWORK_MANAGER_ERROR_AP;

    payload = "AP active. IP: " + WiFi.softAPIP().toString() + ", Wifi mode: " + network_manager_get_mode();
    ram_log_notify(RAM_LOG_INFO, payload.c_str(), true);

    // if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Web Server Root URL
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/settings.html");
    });

    flag_ap_active = true;
    return NETWORK_MANAGER_ERROR_NO_ERROR;
}

NETWORK_MANAGER_ERROR_t network_manager_init(const String& ap_name) {
    NETWORK_MANAGER_ERROR_t retval = NETWORK_MANAGER_ERROR_UNKNOWN;

    // lets populate the wifi_config object with empty parameters
    wifi_config.addParameter("ssid");
    wifi_config.addParameter("password");
    wifi_config.addParameter("localIP");
    wifi_config.addParameter("gateway");

    // the wifi_info object will contain general information such as identification
    wifi_info.addParameter("APname");
    wifi_info.addParameter("deviceName");
    wifi_info.addParameter("subnet", (String) "255.255.255.0");
    wifi_info.addParameter("primaryDNS", (String) "8.8.8.8");
    wifi_info.addParameter("secondaryDNS", (String) "8.8.4.4");

    // load wifi info
    if (wifi_info.load("deviceName") != ESP_OK) {
        // name does not exist yet. generate one
        String device_name = ap_name + " " + String(random(100, 1000));
        wifi_info.set("deviceName", device_name);
    }
        

    wifi_info.set("APname", ap_name);

    // try to load wifi config and credentials from wifi manager
    if ((retval = _wifi_load(&wifi_config)) == NETWORK_MANAGER_ERROR_NO_ERROR) {
        // we have data, therefore connect normally
        String output = "Loaded WiFi: '" + wifi_config.getString("ssid") + "'\n" +
                "With Password: '" + wifi_config.getString("password") + "'\n" +
                "IP-Address: " + wifi_config.getString("localIP") + "\n";
        DualSerial.println(output);

        std::string device_ip = wifi_config.getString("localIP").c_str();

        // establish connection, spawn AP anyway if wanted
        if ((retval = wifi_connect()) == NETWORK_MANAGER_ERROR_CONNECT || AP_VERBOSITY == 2)
            // we couldn't connect, use AP
            if((retval = _wifi_manager_AP(wifi_info.getString("deviceName"))) != NETWORK_MANAGER_ERROR_NO_ERROR) return retval;
    }

    // if no config we need an access point
    else if (retval == NETWORK_MANAGER_ERROR_CONFIG) {
        if((retval = _wifi_manager_AP(wifi_info.getString("APname"))) != NETWORK_MANAGER_ERROR_NO_ERROR) return retval;
    }

    return retval;
}

void network_manager_update() {
    if (flag_ap_active)
        dnsServer.processNextRequest();
        

#ifdef AP_TIMEOUT
    if (flag_ap_active && millis() > AP_TIMEOUT) {
        // Mr. Gorbatschow, tear down that Access Point
        ram_log_notify(RAM_LOG_INFO, "Stopping access point", true);
        dnsServer.stop();
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        flag_ap_active = false;
        wifi_handler_connect();
    }
#endif
}