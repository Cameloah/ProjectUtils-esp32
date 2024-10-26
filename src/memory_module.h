
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
    BOOL,
    BYTEARRAY
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

class ParameterRaw : public Parameter {
public:
    std::unique_ptr<uint8_t[]> value;
    size_t size;

    ParameterRaw(String key, const uint8_t* val, size_t sz) : Parameter(std::move(key)), size(sz) {
        value = std::make_unique<uint8_t[]>(size);
        memcpy(value.get(), val, size);
    }

    esp_err_t save(nvs_handle handle) const override {
        return nvs_set_blob(handle, key.c_str(), value.get(), size);
    }

    esp_err_t load(nvs_handle handle) override {
        size_t required_size = size;
        return nvs_get_blob(handle, key.c_str(), value.get(), &required_size);
    }

    void setValue(void *newValue) override {
        // auto newVal = static_cast<std::pair<uint8_t*, size_t>*>(newValue);
        // size = newVal->second;
        value = std::make_unique<uint8_t[]>(size);
        memcpy(value.get(), newValue, size);
    }

    void *getValue() const override {
        return (void *) value.get();
    }

    ParameterType getType() const override { return ParameterType::BYTEARRAY; }
};

class MemoryModule {
public:
    MemoryModule(String name);

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

    // For raw data
    void addParameter(const String &key, const uint8_t *value, size_t size) {
        _parameters.push_back(std::make_unique<ParameterRaw>(key, value, size));
    }

    esp_err_t save(const String &key);

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

    esp_err_t set(const String &key, uint8_t* value, bool saveImmediately = false) {
        return setByPointer(key, value, ParameterType::BYTEARRAY, saveImmediately);
    }

    esp_err_t setByPointer(const String &key, void *newValue, ParameterType newType, bool saveImmediately = false);

    void *get(const String &key) const;

    int *getInt(const String &key) const;

    float *getFloat(const String &key) const;

    double *getDouble(const String &key) const;

    String getString(const String &key) const;

    bool *getBool(const String &key) const;

    esp_err_t load(const String &key);

    esp_err_t saveAll();

    esp_err_t loadAll();

    esp_err_t loadAllStrict();

    esp_err_t saveRaw(const String &key, uint8_t* value, int size);

    esp_err_t loadRaw(const String &key, uint8_t* value, int size);

private:
    std::vector<std::unique_ptr<Parameter>> _parameters;
    esp_err_t class_error;
    nvs_handle my_handle;
    String storage_name;
};
