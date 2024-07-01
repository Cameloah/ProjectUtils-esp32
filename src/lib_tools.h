//
// Created by Cameloah on 04/05/2024.
//

#pragma once

#include <Arduino.h>

/// @brief checks for meaningful string
///
/// @param str string to be checked
/// @return true if contains non whitespace characters
bool otherThanWhitespace(const String &str);

/// @brief removes whitespace characters and converts string to lowercase
///
/// @param str string to be modified
/// @return modified string with whitespace removed and converted to lowercase
String removeWhitespaceAndConvertToLower(const String &str);