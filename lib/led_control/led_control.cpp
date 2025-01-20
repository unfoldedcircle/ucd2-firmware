// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "led_control.h"

#include <algorithm>

#include "board.h"
#include "event_bits.hpp"
#include "log.h"
#include "states.h"

// bit masks of LED pattern tasks, ordered by prio, highest prio == lowest bit
const int TEST_MODE_RED_BIT = BIT0;  // test mode has highest prio
const int TEST_MODE_GREEN_BIT = BIT1;
const int TEST_MODE_BLUE_BIT = BIT0 | BIT1;  // save a bit
const int TEST_MODE_BITS = BIT0 | BIT1;

const int OTA_BIT = BIT2;
const int LED_SETUP_BIT = BIT3;
const int IDENTIFY_BIT = BIT4;  // auto clear
const int SETUP_BIT = BIT5;
const int CONNECTING_BIT = BIT6;
const int CONNECTED_BIT = BIT7;  // auto clear

const int IR_LEARN_FAILED_BIT = BIT8;  // auto clear. Must have higher prio than IR_LEARN_ON!
const int IR_LEARN_OK_BIT = BIT9;      // auto clearn. "
const int IR_LEARN_ON_BIT = BIT10;

const int FULLY_CHARGED_BIT = BIT11;
const int LOW_BATTERY_BIT = BIT12;
const int ERROR_BIT = BIT13;
const int CHARGING_BIT = BIT14;

const int LED_PWM_FREQ = 12000;
const int LED_RESOLUTION = 8;

void LedControl::init(bool testMode) {
    if (m_eventgroup) {
        Log.error(m_ctx, "Already initialized");
        return;
    }

    m_eventgroup = xEventGroupCreate();
    m_testMode = testMode;

    // Ethernet LED setup. This is controlled from the main loop, so don't set it up in the task running on core 1
#if defined(ETH_STATUS_LED)
    if (!m_testMode) {
        pinMode(ETH_STATUS_LED, OUTPUT);
        ledcSetup(ETH_CHANNEL, LED_PWM_FREQ, LED_RESOLUTION);
        ledcAttachPin(ETH_STATUS_LED, ETH_CHANNEL);
    }
#endif

    UBaseType_t priority = 4;  // priority 1 can delay a pattern up to 2 seconds!
    BaseType_t  core = 0;      // pin to first core (Protocol CPU)

    xTaskCreatePinnedToCore(&LedControl::ledTask, "LedTask", 2000, this, priority, &m_ledTask, core);

    Log.debug(m_ctx, "Initialized");
}

void LedControl::setState(States state) {
    eTaskState taskState = eTaskGetState(m_ledTask);

    Log.logf(Log.DEBUG, m_ctx, "Set state: %d Task state: %d", state, taskState);

    // TODO(zehnm) The State class client requires a rewrite with a proper state machine!
    //           This logic is quick and dirty to replicate the old behaviour!
    //           We have to clear the old pattern to make it work, but we loose the priority ordering :-(
    //           E.g. during OTA we can easily override the pattern with identify or anything else!
    xEventGroupClearBits(m_eventgroup, 0xFFFF);

    switch (state) {
        case States::TEST_LED_RED:
            xEventGroupSetBits(m_eventgroup, TEST_MODE_RED_BIT);
            break;
        case States::TEST_LED_GREEN:
            xEventGroupSetBits(m_eventgroup, TEST_MODE_GREEN_BIT);
            break;
        case States::TEST_LED_BLUE:
            xEventGroupSetBits(m_eventgroup, TEST_MODE_BLUE_BIT);
            break;
        case States::NORMAL_CHARGING:
            xEventGroupSetBits(m_eventgroup, CHARGING_BIT);
            break;
        case States::SETUP:
            xEventGroupSetBits(m_eventgroup, SETUP_BIT);
            break;
        case States::CONNECTING:
            xEventGroupSetBits(m_eventgroup, CONNECTING_BIT);
            break;
        case States::CONN_SUCCESS:
            xEventGroupSetBits(m_eventgroup, CONNECTED_BIT);
            break;
        case States::LED_SETUP:
            xEventGroupSetBits(m_eventgroup, LED_SETUP_BIT);
            break;
        case States::NORMAL_FULLYCHARGED:
            xEventGroupSetBits(m_eventgroup, FULLY_CHARGED_BIT);
            break;
        case States::NORMAL_LOWBATTERY:
            xEventGroupSetBits(m_eventgroup, LOW_BATTERY_BIT);
            break;
        case States::ERROR:
            xEventGroupSetBits(m_eventgroup, ERROR_BIT);
            break;
        case States::OTA:
            xEventGroupSetBits(m_eventgroup, OTA_BIT);
            break;
        case States::IDENTIFY:
            xEventGroupSetBits(m_eventgroup, IDENTIFY_BIT);
            break;
        case States::IR_LEARNING:
            xEventGroupSetBits(m_eventgroup, IR_LEARN_ON_BIT);
            break;
        case States::IR_LEARN_OK:
            xEventGroupSetBits(m_eventgroup, IR_LEARN_OK_BIT);
            break;
        case States::IR_LEARN_FAILED:
            xEventGroupSetBits(m_eventgroup, IR_LEARN_FAILED_BIT);
            break;
        default: {
            // ignore / normal operation mode
        }
    }
}

// TODO(zehnm) replace custom LED pattern task with a ready made solution
// E.g. https://components.espressif.com/components/espressif/led_indicator if that works with PIO
void LedControl::ledTask(void *pvParameter) {
    EventBits_t bits;
    LedControl *led = reinterpret_cast<LedControl *>(pvParameter);

    // Status LED setup
    pinMode(STATUS_LED_R_PIN, OUTPUT);
    ledcSetup(RED_CHANNEL, LED_PWM_FREQ, LED_RESOLUTION);
    ledcAttachPin(STATUS_LED_R_PIN, RED_CHANNEL);

    pinMode(STATUS_LED_G_PIN, OUTPUT);
    ledcSetup(GREEN_CHANNEL, LED_PWM_FREQ, LED_RESOLUTION);
    ledcAttachPin(STATUS_LED_G_PIN, GREEN_CHANNEL);

    pinMode(STATUS_LED_B_PIN, OUTPUT);
    ledcSetup(BLUE_CHANNEL, LED_PWM_FREQ, LED_RESOLUTION);
    ledcAttachPin(STATUS_LED_B_PIN, BLUE_CHANNEL);

    Log.logf(Log.DEBUG, led->m_ctx, "LED task initialized. Running on core: %d", xPortGetCoreID());

    // Log counters: don't log LED pattern in every blink-cycle loop
    u_int8_t chargingCount = 0;
    u_int8_t otaCount = 0;
    // Perform one blink-cycle per loop.
    // A cycle can be one regular blink, or also blinking twice shortly with a longer delay
    while (true) {
        // run as long as bit is set (don't clear it)
        bits = xEventGroupWaitBits(led->m_eventgroup, 0xFFFF, pdFALSE, pdFALSE, pdMS_TO_TICKS(300));

        if (!bits) {
            // timeout while waiting for set bits: nothing set means normal operation, LEDs off
            ledWrite(0, 0, 0);
            continue;
        }

        if (bits & TEST_MODE_BITS) {
            testModePattern(bits);
            continue;
        }

        int brightness = led->m_maxBrightness;
        // minimal status LED brigthness for important events if user disabled status LED
        int minBrightness = max(led->m_minBrightness, brightness);

        // only one state can blink at a time, process by priority
        if (bits & OTA_BIT) {
            if (otaCount == 0) {
                Log.debug(led->m_ctx, "ota");
            }
            otaCount++;
            // log statement ~ every 10 seconds
            if (otaCount == 4) {
                otaCount = 0;
            }
            // important event: OTA updates might fail due to network conditions
            otaPattern(minBrightness);
        } else if (bits & LED_SETUP_BIT) {
            // LED brightness setup. Timeout is handled in State.
            // caller has to set new max brightness before changing state!
            ledWrite(brightness, brightness, brightness);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else if (bits & IDENTIFY_BIT) {
            Log.debug(led->m_ctx, "identify");
            // Blink green, amber, blue and red after each other (aka rainbow).
            // Run twice, then clear task bit at end to stop running.
            // Important message: ignore led brightness
            for (int i = 0; i < 2; i++) {
                blink(0, 0, 255);
                blink(255, 55, 0);
                blink(0, 255, 0);
                blink(255, 0, 0);
            }
            xEventGroupClearBits(led->m_eventgroup, IDENTIFY_BIT);
        } else if (bits & SETUP_BIT) {
            Log.debug(led->m_ctx, "setup");
            // needs setup LED flashes amber color
            // important message: ignore led brightness
            blink(255, 55, 0, 1000);
        } else if (bits & CONNECTING_BIT) {
            Log.debug(led->m_ctx, "connecting");
            // connecting to wifi, turning on OTA, LED flashes white
            blink(brightness, brightness, brightness);
        } else if (bits & CONNECTED_BIT) {
            Log.debug(led->m_ctx, "connected");
            // successful connection
            // run once, manually cleared at end
            // Blink the LED 3 times to indicate successful connection (don't interrupt by any other task)
            for (int i = 0; i < 4; i++) {
                blink(brightness, brightness, brightness, 100);
            }

            xEventGroupClearBits(led->m_eventgroup, CONNECTED_BIT);
        } else if (bits & IR_LEARN_FAILED_BIT) {
            blink(brightness, 0, 0, 100);
            blink(brightness, 0, 0, 100);
            xEventGroupClearBits(led->m_eventgroup, IR_LEARN_FAILED_BIT);
        } else if (bits & IR_LEARN_OK_BIT) {
            blink(0, brightness, 0, 100);
            blink(0, brightness, 0, 100);
            xEventGroupClearBits(led->m_eventgroup, IR_LEARN_OK_BIT);
        } else if (bits & IR_LEARN_ON_BIT) {
            ledWrite(0, brightness, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else if (bits & FULLY_CHARGED_BIT) {
            // no LED pattern defined
            ledWrite(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else if (bits & LOW_BATTERY_BIT) {
            Log.debug(led->m_ctx, "low battery");
            // normal operation, blinks twice every 4s to indicate remote is low battery
            for (int i = 0; i < 2; i++) {
                blink(brightness, brightness, brightness, 100);
            }
            // split long delay to check if state is still active
            for (int i = 0; i < 40; i++) {
                if (!bitIsSetAndNoHigherPrioTask(LOW_BATTERY_BIT, xEventGroupGetBits(led->m_eventgroup))) {
                    goto abort;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else if (bits & ERROR_BIT) {
            Log.debug(led->m_ctx, "error notification");
            // error, LED blinks red
            blink(minBrightness, 0, 0);
        } else if (bits & CHARGING_BIT) {
            if (chargingCount == 0) {
                Log.debug(led->m_ctx, "charging");
            }
            chargingCount++;
            // log statement ~ every minute
            if (chargingCount == 22) {
                chargingCount = 0;
            }
            // if the remote is charging, pulsate the LED in white
            chargingPattern(brightness, led->m_eventgroup);
        }

    // label to abort an inner loop
    abort : {}
    }
}

void LedControl::testModePattern(EventBits_t bits) {
    if ((bits & TEST_MODE_RED_BIT) && (bits & TEST_MODE_GREEN_BIT)) {
        ledWrite(0, 0, 255);  // BLUE
    } else if (bits & TEST_MODE_RED_BIT) {
        ledWrite(255, 0, 0);
    } else if (bits & TEST_MODE_GREEN_BIT) {
        ledWrite(0, 255, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

void LedControl::chargingPattern(int brightness, EventGroupHandle_t eventgroup) {
    EventBits_t bits;

    int ledMapDelay = map(brightness, 5, 255, 30, 5);
    int ledMapPause = map(brightness, 5, 255, 1200, 0);

    // increase the LED brightness
    for (int dutyCycle = 0; dutyCycle <= brightness; dutyCycle++) {
        ledWrite(dutyCycle, dutyCycle, dutyCycle);
        vTaskDelay(pdMS_TO_TICKS(ledMapDelay));
        if (!bitIsSetAndNoHigherPrioTask(CHARGING_BIT, xEventGroupGetBits(eventgroup))) {
            return;
        }
    }

    // pause and check every 100ms if state is still active
    if (ledMapPause > 100) {
        int remainder = ledMapPause % 100;
        vTaskDelay(pdMS_TO_TICKS(remainder));
        for (int i = 0; i < ledMapPause / 100; i++) {
            if (!bitIsSetAndNoHigherPrioTask(CHARGING_BIT, xEventGroupGetBits(eventgroup))) {
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(ledMapPause));
    }

    if (!bitIsSetAndNoHigherPrioTask(CHARGING_BIT, xEventGroupGetBits(eventgroup))) {
        return;
    }

    // decrease the LED brightness
    for (int dutyCycle = brightness; dutyCycle >= 0; dutyCycle--) {
        ledWrite(dutyCycle, dutyCycle, dutyCycle);
        vTaskDelay(pdMS_TO_TICKS(ledMapDelay));
        if (!bitIsSetAndNoHigherPrioTask(CHARGING_BIT, xEventGroupGetBits(eventgroup))) {
            return;
        }
    }

    // split long delay to check if state is still active. Abort and switch if higher prio pattern
    for (int i = 0; i < 5; i++) {
        if (!bitIsSetAndNoHigherPrioTask(CHARGING_BIT, xEventGroupGetBits(eventgroup))) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void LedControl::otaPattern(int brightness) {
    EventBits_t bits;

    // increase the LED brightness
    for (int dutyCycle = 0; dutyCycle <= brightness; dutyCycle++) {
        ledWrite(dutyCycle, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // decrease the LED brightness
    for (int dutyCycle = brightness; dutyCycle >= 0; dutyCycle--) {
        ledWrite(dutyCycle, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // no need to split delay for OTA: highest prio and restart after successful update
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void LedControl::setLedMaxBrightness(int value) {
    if (value >= 0 && value <= 255) {
        m_maxBrightness = value;
    }
}

void LedControl::blink(uint8_t r, uint32_t g, uint32_t b, int delay_ms /* = 200 */) {
    ledWrite(r, g, b);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    ledWrite(0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

void LedControl::setEthLedBrightness(int value) {
#if defined(ETH_STATUS_LED)
    if (!m_testMode) {
        if (value >= 0 && value <= 255) {
            ledcWrite(ETH_CHANNEL, invertBrightness(value));
        }
    }
#endif
}

int LedControl::invertBrightness(int value) {
    int returnValue = abs(value - 255);
    if (returnValue > 255) {
        returnValue = 255;
    }

    return returnValue;
}

void LedControl::ledWrite(uint8_t r, uint32_t g, uint32_t b) {
    // changing the LED brightness with PWM
    ledcWrite(RED_CHANNEL, invertBrightness(r));
    ledcWrite(GREEN_CHANNEL, invertBrightness(g));
    ledcWrite(BLUE_CHANNEL, invertBrightness(b));
}

LedControl &LedControl::getInstance() {
    static LedControl instance;
    return instance;
}

LedControl &ledControl = ledControl.getInstance();
