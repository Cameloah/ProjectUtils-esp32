//
// Created by Camleoah on 04/05/2024.
//

#include <Arduino.h>

bool otherThanWhitespace(const String &str) {
    for (int i = 0; i < str.length(); i++) {
        if (!isWhitespace(str.charAt(i))) {
            return true;
        }
    }
    return false;
}

bool otherThanAsterix(const String &str) {
    for (int i = 0; i < str.length(); i++) {
        if (str.charAt(i) != '*') {
            return true;
        }
    }
    return false;
}