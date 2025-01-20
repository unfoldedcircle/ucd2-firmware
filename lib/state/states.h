// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

enum States {
    NOT_SET = 0,
    // factory LED test
    TEST_LED_RED = 2,
    TEST_LED_GREEN = 3,
    TEST_LED_BLUE = 4,
    // 5 - starting up
    STARTUP = 5,
    // 6 - needs setup
    SETUP = 6,
    // 7 - connecting to wifi, turning on OTA
    CONNECTING = 7,
    // 8 - successful connection
    CONN_SUCCESS = 8,
    // 9 - normal operation, LED off, turns on when charging
    NORMAL = 9,
    // 10 - normal operation, LED breathing to indicate charging
    NORMAL_CHARGING = 10,
    // 11 - error
    ERROR = 11,
    // 12 - LED brightness setup
    LED_SETUP = 12,
    // 13 - normal operation, remote fully charged
    NORMAL_FULLYCHARGED = 13,
    // 14 - normal operation, blinks to indicate remote is low battery
    NORMAL_LOWBATTERY = 14,
    // 15 - OTA update
    OTA = 15,
    // 16 - identify dock: blink green, amber, blue and red after each other(aka rainbow)
    IDENTIFY = 16,
    IR_LEARNING = 17,
    IR_LEARN_OK = 18,
    IR_LEARN_FAILED = 19
};
