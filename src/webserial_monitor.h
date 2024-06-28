//
// Created by Cameloah on 08/12/2022.
//

#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include "main_project_utils.h"

/// initializes the webserial service
void webserial_monitor_init();