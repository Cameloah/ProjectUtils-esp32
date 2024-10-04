//
// Created by Cameloah on 29.10.2023.
//


#include "memory_module.h"

esp_err_t initialize_nvs() {
    static bool isInitialized = false;
    esp_err_t error = ESP_OK;
    if (!isInitialized) {
        if ((error = nvs_flash_init_partition("nvs2")) == ESP_OK)
            isInitialized = true;
    }
    return error;
}

MemoryModule::MemoryModule(String name) {
    if ((class_error = initialize_nvs()) != ESP_OK)
        Serial.println("error init nvs"); //ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
    storage_name = name;
};