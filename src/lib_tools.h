//
// Created by Camleoah on 04/05/2024.
//

#pragma once

#include <Arduino.h>

/// @brief checks for meaningful string
///
/// @param str string to be checked
/// @return true if contains non whitespace characters
bool otherThanWhitespace(const String &str);