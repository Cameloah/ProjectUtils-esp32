//
// Created by Camleoah on 08/12/2022.
//

#pragma once

#include <HardwareSerial.h>


class DualPrint : public HardwareSerial
{
public:
    explicit DualPrint(int uartNr) : HardwareSerial(uartNr), use_webSerial(false) {}

    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override;
    size_t readBytes(char *buffer, size_t length) override;

    bool use_webSerial;
    String buffer_webserial;
};

extern DualPrint DualSerial;

/// initializes the webserial service
void webserial_monitor_init();