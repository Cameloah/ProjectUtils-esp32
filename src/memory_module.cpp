//
// Created by Cameloah on 29.10.2023.
//


#include "memory_module.h"
#include "ram_log.h"
#include "webserial_monitor.h"

esp_err_t initialize_nvs() {
    static bool isInitialized = false;
    esp_err_t error = ESP_OK;
    if (!isInitialized) {
        if ((error = nvs_flash_init()) == ESP_OK)
            isInitialized = true;
    }
    return error;
}

MemoryModule::MemoryModule(String name) {
    if ((class_error = initialize_nvs()) != ESP_OK) {
        DualSerial.print("error init nvs. ");
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
    }
    else 
    storage_name = name;
};

esp_err_t MemoryModule::save(const String &key) {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;}

    class_error = ESP_ERR_NOT_FOUND;

    for (const auto &param: _parameters) {
        if (param->key == key) {
            class_error = param->save(my_handle);
            break; // Break after the parameter is found and saved
        }
    }
    if (class_error != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("saving key '" + key + "'").c_str());
    nvs_close(my_handle);
    return class_error;
}

esp_err_t MemoryModule::setByPointer(const String &key, void *newValue, ParameterType newType, bool saveImmediately) {
    class_error = ESP_ERR_NOT_FOUND;
    for (const auto &param: _parameters) {
        if (param->key == key) {
            if (param->getType() == newType) {
                param->setValue(newValue);
                class_error = ESP_OK;
                if (saveImmediately) {
                    class_error = save(key);
                }
            }
            else {
                class_error = ESP_ERR_INVALID_ARG;
            }
            break;
        }
    }
    return class_error;
}

void *MemoryModule::get(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key) {
            return param->getValue();
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("get key: '" + key + "'").c_str());
    return nullptr; // Or handle the key-not-found case as needed
}

int *MemoryModule::getInt(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key && param->getType() == ParameterType::INT) {
            return static_cast<int*>(param->getValue());
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("getInt key: '" + key + "'").c_str());
    return nullptr; // Or handle the key-not-found case as needed
}

float *MemoryModule::getFloat(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key && param->getType() == ParameterType::FLOAT) {
            return static_cast<float*>(param->getValue());
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("getFloat key: '" + key + "'").c_str());
    return nullptr; // Or handle the key-not-found case as needed
}

double *MemoryModule::getDouble(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key && param->getType() == ParameterType::DOUBLE) {
            return static_cast<double*>(param->getValue());
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("getDouble key: '" + key + "'").c_str());
    return nullptr; // Or handle the key-not-found case as needed
}

String MemoryModule::getString(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key && param->getType() == ParameterType::STRING) {
            return *static_cast<String*>(param->getValue());
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("getString key: '" + key + "'").c_str());
    return "String not found!"; // Or handle the key-not-found case as needed
}

bool *MemoryModule::getBool(const String &key) const {
    for (const auto &param: _parameters) {
        if (param->key == key && param->getType() == ParameterType::BOOL) {
            return static_cast<bool*>(param->getValue());
        }
    }
    ram_log_notify(RAM_LOG_ERROR_MEMORY, ESP_ERR_NOT_FOUND, String("getBool key: '" + key + "'").c_str());
    return nullptr; // Or handle the key-not-found case as needed
}

esp_err_t MemoryModule::load(const String &key) {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    class_error = ESP_ERR_NOT_FOUND;

    for (const auto &param: _parameters) {
        if (param->key == key) {
            class_error = param->load(my_handle);
            break; // Break after the parameter is found and loaded
        }
    }
    nvs_close(my_handle);
    if (class_error != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("load key: '" + key + "'").c_str());
    return class_error;
}

esp_err_t MemoryModule::saveAll() {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    for (const auto &param: _parameters) {
        if ((class_error = param->save(my_handle)) != ESP_OK) {
            ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("saveAll key: '" + param->key + "'").c_str());
            break;
        }
    }
    nvs_close(my_handle);
    return class_error;
}

esp_err_t MemoryModule::loadAll() {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    for (const auto &param: _parameters) {
        if ((class_error = param->load(my_handle)) != ESP_OK && class_error != ESP_ERR_NOT_FOUND) {
            ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("loadAll key: '" + param->key + "'").c_str());
            break;
        }
    }
    nvs_close(my_handle);
    return class_error;
}

esp_err_t MemoryModule::loadAllStrict() {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    for (const auto &param: _parameters) {
        if ((class_error = param->load(my_handle)) != ESP_OK) {
            ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("loadAllStrict key: '" + param->key + "'").c_str());
            break;
        }
    }
    nvs_close(my_handle);
    return class_error;
}

esp_err_t MemoryModule::saveRaw(const String &key, uint8_t* value, int size) {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    class_error = nvs_set_blob(my_handle, key.c_str(), value, size);
    if (class_error != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("saveRaw key: '" + key + "'").c_str());
    
    nvs_commit(my_handle);
    if (class_error != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("saveRaw key: '" + key + "'").c_str());
    nvs_close(my_handle);
    return class_error;
}

esp_err_t MemoryModule::loadRaw(const String &key, uint8_t* value, int size) {
    if ((class_error = nvs_open(storage_name.c_str(), NVS_READWRITE, &my_handle)) != ESP_OK) {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error);
        return class_error;
    }

    size_t required_size = size;
    class_error = nvs_get_blob(my_handle, key.c_str(), value, &required_size);
    if (class_error != ESP_OK)
        ram_log_notify(RAM_LOG_ERROR_MEMORY, class_error, String("loadRaw key: '" + key + "'").c_str());
    nvs_close(my_handle);
    return class_error;
}