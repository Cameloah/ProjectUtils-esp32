//
// Created by Cameloah on 29.10.2023.
//

#pragma once

#include "EEPROM.h"
#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>

#include <utility>
#include <vector>
#include <memory>

#include "ram_log.h"
#include "webserial_monitor.h"

/*
#define EEPROM_SIZE                                 128
#define TIMEOUT_EEPROM_INIT_MS                      2000
*/

namespace std {
    template<class T>
    struct _Unique_if {
        typedef unique_ptr<T> _Single_object;
    };

    template<class T>
    struct _Unique_if<T[]> {
        typedef unique_ptr<T[]> _Unknown_bound;
    };

    template<class T, size_t N>
    struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
    typename _Unique_if<T>::_Single_object
    make_unique(Args &&... args) {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    template<class T>
    typename _Unique_if<T>::_Unknown_bound
    make_unique(size_t n) {
        typedef typename remove_extent<T>::type U;
        return unique_ptr<T>(new U[n]());
    }

    template<class T, class... Args>
    typename _Unique_if<T>::_Known_bound
    make_unique(Args &&...) = delete;
}


typedef enum {
    MEMORY_MODULE_ERROR_NO_ERROR = 0x00,
    MEMORY_MODULE_ERROR_INIT = 0x01,
    MEMORY_MODULE_ERROR_NO_MEM = 0x02,
    MEMORY_MODULE_ERROR_SET_NOT_FOUND = 0x03,
    MEMORY_MODULE_ERROR_LOAD_NOT_FOUND = 0x04,
    MEMORY_MODULE_ERROR_UNKNOWN = 0xFF
} MEMORY_MODULE_ERROR_t;

enum class ParameterType {
    INT,
    FLOAT,
    DOUBLE,
    STRING,
    BOOL
};

class Parameter {
public:
    virtual ~Parameter() = default;

    virtual esp_err_t save(nvs_handle handle) const = 0;

    virtual esp_err_t load(nvs_handle handle) = 0;

    virtual void setValue(void *newValue) = 0;

    virtual void *getValue() const = 0;

    virtual ParameterType getType() const = 0;

    String key;

protected:
    explicit Parameter(String k) : key(std::move(k)) {}
};

class ParameterInt : public Parameter {
public:
    ParameterInt(String key, int val) : Parameter(std::move(key)), value(val) {}

    esp_err_t save(nvs_handle handle) const override {
        return nvs_set_i32(handle, key.c_str(), value);
    }

    esp_err_t load(nvs_handle handle) override {
        return nvs_get_i32(handle, key.c_str(), &value);
    }

    void setValue(void *newValue) override {
        value = *static_cast<int *>(newValue);
    }

    void *getValue() const override {
        return (void *) &value;
    }

    ParameterType getType() const override { return ParameterType::INT; }

    int value;
};

class ParameterFloat : public Parameter {
public:
    ParameterFloat(String key, float val) : Parameter(std::move(key)), value(val) {}

    esp_err_t save(nvs_handle handle) const override {
        return nvs_set_blob(handle, key.c_str(), &value, sizeof(value));
    }

    esp_err_t load(nvs_handle handle) override {
        size_t required_size = sizeof(value);
        return nvs_get_blob(handle, key.c_str(), &value, &required_size);
    }

    void setValue(void *newValue) override {
        value = *static_cast<float *>(newValue);
    }

    void *getValue() const override {
        return (void *) &value;
    }

    ParameterType getType() const override { return ParameterType::FLOAT; }

    float value;
};

class ParameterDouble : public Parameter {
public:
    double value;

    ParameterDouble(String key, double val) : Parameter(std::move(key)), value(val) {}

    esp_err_t save(nvs_handle handle) const override {
        // NVS does not directly support double, so store as blob
        return nvs_set_blob(handle, key.c_str(), &value, sizeof(value));
    }

    esp_err_t load(nvs_handle handle) override {
        size_t required_size = sizeof(value);
        return nvs_get_blob(handle, key.c_str(), &value, &required_size);
    }

    void setValue(void *newValue) override {
        value = *static_cast<double *>(newValue);
    }

    void *getValue() const override {
        return (void *) &value;
    }

    ParameterType getType() const override { return ParameterType::DOUBLE; }
};

class ParameterString : public Parameter {
public:
    String value;

    ParameterString(String key, String val) : Parameter(std::move(key)), value(std::move(val)) {}

    esp_err_t save(nvs_handle handle) const override {
        return nvs_set_str(handle, key.c_str(), value.c_str());
    }

    esp_err_t load(nvs_handle handle) override {
        size_t required_size;
        esp_err_t error = nvs_get_str(handle, key.c_str(), nullptr, &required_size);
        if (error != ESP_OK)
            return error;

        char *buf = new char[required_size];
        error = nvs_get_str(handle, key.c_str(), buf, &required_size);
        value = String(buf);
        delete[] buf;
        return error;
    }

    void setValue(void *newValue) override {
        value = *static_cast<String*>(newValue);
    }

    void *getValue() const override {
        return (void *) &value;
    }

    ParameterType getType() const override { return ParameterType::STRING; }
};

class ParameterBool : public Parameter {
public:
    bool value;

    ParameterBool(String key, bool val) : Parameter(std::move(key)), value(val) {}

    esp_err_t save(nvs_handle handle) const override {
        uint8_t val = value ? 1 : 0;
        return nvs_set_u8(handle, key.c_str(), val);
    }

    esp_err_t load(nvs_handle handle) override {
        uint8_t val;
        esp_err_t error = nvs_get_u8(handle, key.c_str(), &val);
        value = (val != 0);
        return error;
    }

    void setValue(void *newValue) override {
        value = *static_cast<bool *>(newValue);
    }

    void *getValue() const override {
        return (void *) &value;
    }

    ParameterType getType() const override { return ParameterType::BOOL; }
};


class MemoryModule {
public:
    MemoryModule();

    // For int
    void addParameter(const String &key, int value) {
        _parameters.push_back(std::make_unique<ParameterInt>(key, value));
    }

    // For float
    void addParameter(const String &key, float value) {
        _parameters.push_back(std::make_unique<ParameterFloat>(key, value));
    }

    // For double
    void addParameter(const String &key, double value) {
        _parameters.push_back(std::make_unique<ParameterDouble>(key, value));
    }

    // For String
    void addParameter(const String &key, const String &value = (String) "") {
        _parameters.push_back(std::make_unique<ParameterString>(key, value));
    }

    // For bool
    void addParameter(const String &key, bool value) {
        _parameters.push_back(std::make_unique<ParameterBool>(key, value));
    }

    esp_err_t save(const String &key) {
        if ((class_error = nvs_open("storage", NVS_READWRITE, &my_handle)) != ESP_OK)
            return class_error;

        class_error = ESP_ERR_NOT_FOUND;

        for (const auto &param: _parameters) {
            if (param->key == key) {
                class_error = param->save(my_handle);
                break; // Break after the parameter is found and saved
            }
        }
        nvs_close(my_handle);
        return class_error;
    }

    esp_err_t set(const String &key, int value, bool saveImmediately = false) {
        return setByPointer(key, &value, ParameterType::INT, saveImmediately);
    }

    esp_err_t set(const String &key, float value, bool saveImmediately = false) {
        return setByPointer(key, &value, ParameterType::FLOAT, saveImmediately);
    }

    esp_err_t set(const String &key, double value, bool saveImmediately = false) {
        return setByPointer(key, &value, ParameterType::DOUBLE, saveImmediately);
    }

    esp_err_t set(const String &key, String value, bool saveImmediately = false) {
        return setByPointer(key, &value, ParameterType::STRING, saveImmediately);
    }

    esp_err_t set(const String &key, bool value, bool saveImmediately = false) {
        return setByPointer(key, &value, ParameterType::BOOL, saveImmediately);
    }

    esp_err_t setByPointer(const String &key, void *newValue, ParameterType newType, bool saveImmediately = false) {
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
        if (class_error == ESP_ERR_NOT_FOUND)
            setKeyNotFound(key);
        return class_error;
    }

    void *get(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key) {
                return param->getValue();
            }
        }
        loadKeyNotFound(key);
        return nullptr; // Or handle the key-not-found case as needed
    }

    int *getInt(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key && param->getType() == ParameterType::INT) {
                return static_cast<int*>(param->getValue());
            }
        }
        loadKeyNotFound(key);
        return nullptr; // Or handle the key-not-found case as needed
    }

    float *getFloat(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key && param->getType() == ParameterType::FLOAT) {
                return static_cast<float*>(param->getValue());
            }
        }
        loadKeyNotFound(key);
        return nullptr; // Or handle the key-not-found case as needed
    }

    double *getDouble(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key && param->getType() == ParameterType::DOUBLE) {
                return static_cast<double*>(param->getValue());
            }
        }
        loadKeyNotFound(key);
        return nullptr; // Or handle the key-not-found case as needed
    }

    String getString(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key && param->getType() == ParameterType::STRING) {
                return *static_cast<String*>(param->getValue());
            }
        }
        loadKeyNotFound(key);
        return "String not found!"; // Or handle the key-not-found case as needed
    }

    bool *getBool(const String &key) const {
        for (const auto &param: _parameters) {
            if (param->key == key && param->getType() == ParameterType::BOOL) {
                return static_cast<bool*>(param->getValue());
            }
        }
        loadKeyNotFound(key);
        return nullptr; // Or handle the key-not-found case as needed
    }

    esp_err_t load(const String &key) {
        if ((class_error = nvs_open("storage", NVS_READONLY, &my_handle)) != ESP_OK)
            return class_error;

        class_error = ESP_ERR_NOT_FOUND;

        for (const auto &param: _parameters) {
            if (param->key == key) {
                class_error = param->load(my_handle);
                break; // Break after the parameter is found and loaded
            }
        }
        nvs_close(my_handle);
        if (class_error == ESP_ERR_NOT_FOUND)
            loadKeyNotFound(key);
        return class_error;
    }

    esp_err_t saveAll() {
        if ((class_error = nvs_open("storage", NVS_READWRITE, &my_handle)) != ESP_OK)
            return class_error;

        for (const auto &param: _parameters) {
            if ((class_error = param->save(my_handle)) != ESP_OK)
                break;
        }
        nvs_close(my_handle);
        return class_error;
    }

    esp_err_t loadAll() {
        if ((class_error = nvs_open("storage", NVS_READONLY, &my_handle)) != ESP_OK)
            return class_error;

        for (const auto &param: _parameters) {
            if ((class_error = param->load(my_handle)) != ESP_OK && class_error != ESP_ERR_NVS_NOT_FOUND)
                break;
        }
        nvs_close(my_handle);
        return class_error;
    }

    esp_err_t loadAllStrict() {
        if ((class_error = nvs_open("storage", NVS_READONLY, &my_handle)) != ESP_OK)
            return class_error;

        for (const auto &param: _parameters) {
            if ((class_error = param->load(my_handle)) != ESP_OK) {
                if (class_error == ESP_ERR_NOT_FOUND)
                    loadKeyNotFound(param->key);
                break;
            }
        }
        nvs_close(my_handle);
        return class_error;
    }

private:
    std::vector<std::unique_ptr<Parameter>> _parameters;
    esp_err_t class_error;
    nvs_handle my_handle;

    void setKeyNotFound(const String &key) const {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, MEMORY_MODULE_ERROR_SET_NOT_FOUND, String("Parameter: '" + key + "'").c_str());
    }

    void loadKeyNotFound(const String &key) const {
        ram_log_notify(RAM_LOG_ERROR_MEMORY, MEMORY_MODULE_ERROR_LOAD_NOT_FOUND, String("Parameter: '" + key + "'").c_str());
    }
};

void memory_module_init1();
