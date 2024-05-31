//
// Created by Cameloah on 29.10.2023.
//


#include "memory_module.h"
#include "webserial_monitor.h"


MemoryModule config_data;

esp_err_t initialize_nvs() {
    static bool isInitialized = false;
    esp_err_t error = ESP_OK;
    if (!isInitialized) {
        if ((error = nvs_flash_init()) == ESP_OK)
            isInitialized = true;
    }
    return error;
}

MemoryModule::MemoryModule() {
    if ((class_error = initialize_nvs()) != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
};