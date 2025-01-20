// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>

#include "freertos/event_groups.h"
#include "states.h"

class LedControl {
 public:
    static const int RED_CHANNEL = 1;
    static const int GREEN_CHANNEL = 2;
    static const int BLUE_CHANNEL = 3;
    static const int ETH_CHANNEL = 4;

    void init(bool testMode);

    void setState(States state);

    /**
     * Do not use, LedControl should only be used by State
     */
    static LedControl &getInstance();

    void setLedMaxBrightness(int value);
    int  getLedMaxBrightness() { return m_maxBrightness; }

    void setEthLedBrightness(int value);

 private:
    LedControl() = default;
    LedControl(const LedControl &) = delete;  // no copying
    LedControl &operator=(const LedControl &) = delete;

    bool m_testMode = false;

    const char *m_ctx = "LED";

    // Minimal brightness for important LED notifications
    int m_minBrightness = 5;
    // Default value if not initialized with setLedMaxBrightness
    int m_maxBrightness = 127;

    TaskHandle_t       m_ledTask = nullptr;
    EventGroupHandle_t m_eventgroup = nullptr;

    static void ledTask(void *pvParameter);

    static void testModePattern(EventBits_t bits);
    static void chargingPattern(int brightness, EventGroupHandle_t eventgroup);
    static void otaPattern(int brightness);

    /**
     * Make the LED blink once.
     */
    static void blink(uint8_t r, uint32_t g, uint32_t b, int delay_ms = 200);
    // !!
    // RGB values are inverted due to common anode. So if you want R=256, you need to set it to R=0
    static int invertBrightness(int value);

    /**
     * Write RGB LED values.
     */
    static void ledWrite(uint8_t r, uint32_t g, uint32_t b);
};

extern LedControl &ledControl;
