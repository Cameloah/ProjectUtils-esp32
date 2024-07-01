//
// Created by Cameloah on 04/05/2024.
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

String removeWhitespaceAndConvertToLower(const String &str) {
    String result;
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (!isWhitespace(c)) {
            if (isUpperCase(c)) {
                c = toLowerCase(c);
            }
            result += c;
        }
    }
    return result;
}