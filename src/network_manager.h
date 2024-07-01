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

/// @brief Initializes the network manager with the specified access point name.
/// @param ap_name The name of the access point to connect to.
/// @return The error code indicating the success or failure of the initialization.
NETWORK_MANAGER_ERROR_t network_manager_init(const String& ap_name);

/// @brief Connects to the Wi-Fi network using the configured credentials.
/// @return The error code indicating the success or failure of the connection.
NETWORK_MANAGER_ERROR_t network_manager_wifi_connect();

/// @brief Checks if the device is currently connected to a Wi-Fi network.
/// @return True if connected, false otherwise.
bool network_manager_is_connected();

/// @brief Gets the current mode of the network manager.
/// @return The mode as a string.
String network_manager_get_mode();

/// @brief Updates the network manager.
/// This function needs to be called periodically to handle network-related tasks.
void network_manager_update();