//
// Created by Cameloah on 29.10.2023.
//


#include "memory_module.h"
#include "webserial_monitor.h"


MemoryModule config_data;

void memory_module_init1() {

    config_data.addParameter("timeout_boot", 20);

    config_data.addParameter("factor", 1.23f);
    config_data.addParameter("milage", (double) 12345.3);
    config_data.addParameter("name", (String) "erster test");
    config_data.addParameter("flag_autostart", true);

    // config_data.saveAll();

    int* timeout = static_cast<int*>(config_data.get("timeout_boot"));
    if (timeout == nullptr) {
        DualSerial.println("nullptr");
        return;
    }

    int result = 2 + *timeout;
    DualSerial.println(result);
    DualSerial.println(*static_cast<float*>(config_data.get("factor")));
    DualSerial.println(*static_cast<double*>(config_data.get("milage")));
    DualSerial.println(*static_cast<String*>(config_data.get("name")));
    config_data.set("flag_autostart", false);
    DualSerial.println(*static_cast<bool*>(config_data.get("flag_autostart")));

}

/*
MEMORY_MODULE_ERROR_t memory_module_init() {
    uint32_t counter_init_timeout = 0;

    while (!EEPROM.begin(EEPROM_SIZE)) {
        delay(10);
        if (10 * counter_init_timeout > TIMEOUT_EEPROM_INIT_MS) {
            return MEMORY_MODULE_ERROR_INIT;
        }
        counter_init_timeout++;
    }

    return MEMORY_MODULE_ERROR_NO_ERROR;
}
*/

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