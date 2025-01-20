// SPDX-FileCopyrightText: Copyright (c) 2023 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Common utility functions for ESP32 EventBits.
// Make sure this file also compiles natively and all functions are covered by unit tests.

#pragma once

#include <stdint.h>

/**
 * Test if the eventBits has the bit set specified in mask and that there are no other higher priority bits set
 */
bool bitIsSetAndNoHigherPrioTask(int mask, uint32_t eventBits) {
    if (!(eventBits & mask)) {
        return false;
    }

    if (mask <= 1) {
        return true;
    }

    return (eventBits & (mask - 1)) == 0;
}
