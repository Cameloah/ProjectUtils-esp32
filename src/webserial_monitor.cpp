//
// Created by Cameloah on 08/12/2022.
//

#include "webserial_monitor.h"


#ifdef ENABLE_WEBSERIAL

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

#endif