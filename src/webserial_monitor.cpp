//
// Created by Camleoah on 08/12/2022.
//

#include "webserial_monitor.h"

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include "main_project_utils.h"


int DualPrint::available()
{
    if (uartAvailable(_uart))
        return uartAvailable(_uart);

    else if (buffer_webserial.length())
        return buffer_webserial.length();

    else return 0;
}

size_t DualPrint::readBytes(char *buffer, size_t length)
{
    if (uartAvailable(_uart)) {
        size_t count = 0;
        while (count < length) {
            int c = timedRead();
            if (c < 0) {
                break;
            }
            *buffer++ = (char) c;
            count++;
        }
        return count;
    }

    else {
        uint16_t len_webbuffer = buffer_webserial.length();
        if (length < len_webbuffer)
            return -1;
        strcpy(buffer, buffer_webserial.c_str());
        buffer_webserial = "";
        return len_webbuffer;
    }
}

size_t DualPrint::write(const uint8_t *buffer, size_t size) {
    if (use_webSerial) WebSerial.print((char *) buffer);
    size_t n = 0;
    while(size--) {
        n += HardwareSerial::write(*buffer++);
    }
    return n;
}

DualPrint DualSerial(0);


void recvMsg(uint8_t *data, size_t len){
    DualSerial.buffer_webserial = "";
    for(int i=0; i < len; i++){
        DualSerial.buffer_webserial += char(data[i]);
    }
    DualSerial.print("WebUser>>>> "); DualSerial.println(DualSerial.buffer_webserial);
}

void webserial_monitor_init() {
    DualSerial.use_webSerial = true;

    // WebSerial is accessible at "<IP Address>/webserial" in browser
    WebSerial.begin(&server);
    WebSerial.msgCallback(recvMsg);
}

