//
// Created by Cameloah on 31.03.2023.
//

#pragma once

#include "main_project_utils.h"
#include "memory_module.h"

extern MemoryModule wifi_config;
extern MemoryModule wifi_info;


typedef enum{
    NETWORK_MANAGER_ERROR_NO_ERROR        = 0x00,
    NETWORK_MANAGER_ERROR_CONFIG          = 0x01,
    NETWORK_MANAGER_ERROR_CONNECT         = 0x02,
    NETWORK_MANAGER_ERROR_SPIFFS          = 0x03,
    NETWORK_MANAGER_ERROR_AP              = 0x04,
    NETWORK_MANAGER_ERROR_UNKNOWN         = 0xFF
} NETWORK_MANAGER_ERROR_t;


NETWORK_MANAGER_ERROR_t network_manager_init(const String& ap_name);

bool network_manager_is_connected();

String network_manager_get_mode();

/// \brief needs to be run periodically
void network_manager_update();