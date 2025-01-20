// SPDX-FileCopyrightText: Copyright (c) 2023 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Common string utility functions.
// Make sure this file also compiles natively and all functions are covered by unit tests.

#pragma once

#include <stdint.h>

#include <cstring>

/// @brief Replace characters in string buffer.
/// @param str string buffer
/// @param orig character to replace
/// @param rep replacement character
/// @return number of characters replaced
int replacechar(char *str, char orig, char rep) {
    if (str == NULL) {
        return 0;
    }
    char *ix = str;
    int   n = 0;
    while ((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}
