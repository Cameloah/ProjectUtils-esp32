//
// Created by Camleoah on 08/12/2022.
//

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#include "github_update.h"
#include "cert.h"

String url_fw_bin = URL_FW_BIN;
String url_fs_bin = URL_FS_BIN;
String fw_version;

GITHUB_UPDATE_ERROR_t github_update_firmwareUpdate() {
    // initiate wifi update client
    WiFiClientSecure client;
    // client.setCACert(rootCACertificate);
    client.setInsecure();
    // github redirects you to the latest version if you access ...\latest
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    // lets update the given fw binary url with the latest version
    // TODO: check for proper format of url
    url_fw_bin.replace("<version>", fw_version);
    url_fs_bin.replace("<version>", fw_version);

    // now lets see whether there exists a fs_bin file
    HTTPClient https;
    if (https.begin(client, url_fs_bin)) {
        // start connection and send HTTP header
        delay(100);
        int httpCode;
        // access the url
        httpCode = https.GET();

        if (httpCode == HTTP_CODE_FOUND) {// do download
            DualSerial.println("\nFound filesystem bin. Downloading and installing...");
            t_httpUpdate_return ret_fs = httpUpdate.updateSpiffs(client, url_fs_bin);

            switch (ret_fs) {
                case HTTP_UPDATE_FAILED:
                    DualSerial.printf("FS update failed! Error (%d): %s\n", httpUpdate.getLastError(),
                                      httpUpdate.getLastErrorString().c_str());
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    DualSerial.println("FS no updates");
                    break;

                case HTTP_UPDATE_OK:
                    DualSerial.println("FS successfully updated!");
            }
        }

        https.end();
    }


    DualSerial.printf("\nDownloading and installing Firmware Version %s...\n", fw_version.c_str());
    t_httpUpdate_return ret_fw = httpUpdate.update(client, url_fw_bin);

    switch (ret_fw) {
        case HTTP_UPDATE_FAILED:
            DualSerial.printf("FW update failed! Error (%d): %s\n", httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            return GITHUB_UPDATE_ERROR_HTTP;

        case HTTP_UPDATE_NO_UPDATES:
            DualSerial.println("FW no update");
            return GITHUB_UPDATE_ERROR_NO_UPDATE;

        case HTTP_UPDATE_OK:
            DualSerial.println("FW successfully updated!");
            return GITHUB_UPDATE_ERROR_NO_ERROR;
    }
}

GITHUB_UPDATE_ERROR_t github_update_firmwareUpdate(const char *desired_version) {
    // TODO: check for proper format
    fw_version = desired_version;
    return github_update_firmwareUpdate();
}

GITHUB_UPDATE_ERROR_t github_update_checkforlatest(String *ret_version) {
    
    // if wifi not connected, cancel early
    if (WiFi.status() != WL_CONNECTED) {
        DualSerial.println("Wifi is not connected");
        return GITHUB_UPDATE_ERROR_WIFI;
    }

    int httpCode;
    WiFiClientSecure *client = new WiFiClientSecure;

    if (client) {
        // client->setCACert(rootCACertificate);
        client->setInsecure();

        // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
        HTTPClient https;

        if (https.begin(*client, URL_FW_VERSION)) {
            // start connection and send HTTP header
            delay(100);

            // access the url
            httpCode = https.GET();

            if (httpCode == HTTP_CODE_FOUND) // if version received
            {
                // get updated url after redirect
                String location = https.getLocation();
                delay(100);

                // check for  FW version from the url
                int index_fw_vers = location.indexOf("/releases/tag/");
                if (index_fw_vers == -1) {
                    DualSerial.println("error, no tag found");
                    return GITHUB_UPDATE_ERROR_URL;
                }

                // extract fw version from redirected url string
                fw_version = location.substring(location.indexOf("/releases/tag/") + 14);
            } else {
                DualSerial.print("error in downloading version file: ");
                DualSerial.println(httpCode);
                return GITHUB_UPDATE_ERROR_HTTP;
            }
            https.end();
        }
        delete client;
    }

    if (httpCode == HTTP_CODE_FOUND) // if version received
    {
        fw_version.trim();
        if (ret_version != NULL)
            *ret_version = fw_version;

        // extract firmware numbers
        String fw_version_latest = fw_version;
        uint8_t fw_latest_major = atoi(strtok((char *) fw_version_latest.c_str(), "v.\n"));
        uint8_t fw_latest_minor = atoi(strtok(nullptr, "v.\n"));
        uint8_t fw_latest_patch = atoi(strtok(nullptr, "v.\n"));

        if (fw_latest_major > FW_VERSION_MAJOR) {
            DualSerial.printf("\nNew Firmware detected! Major update available: v%d.%d.%d\n",
                          fw_latest_major, fw_latest_minor, fw_latest_patch);
        } else if (fw_latest_major == FW_VERSION_MAJOR && fw_latest_minor > FW_VERSION_MINOR) {
            DualSerial.printf("\nNew Firmware detected! Minor update available: v%d.%d.%d\n",
                          fw_latest_major, fw_latest_minor, fw_latest_patch);
        } else if (fw_latest_major == FW_VERSION_MAJOR && fw_latest_minor == FW_VERSION_MINOR && fw_latest_patch > FW_VERSION_PATCH) {
            DualSerial.printf("\nNew Firmware detected! New Patch available: v%d.%d.%d\n",
                          fw_latest_major, fw_latest_minor, fw_latest_patch);
        } else {
            DualSerial.printf("\nDevice running on latest firmware version: v%d.%d.%d\n",
                          FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
            if (ret_version != NULL)
                *ret_version = "0";
            return GITHUB_UPDATE_ERROR_NO_UPDATE;
        }

        if (ret_version != NULL)
            *ret_version = fw_version;
        return GITHUB_UPDATE_ERROR_NO_ERROR;
    }
    return GITHUB_UPDATE_ERROR_HTTP;
}