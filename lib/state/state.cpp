// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "state.h"

#include <Arduino.h>

#include <cstdio>

#include "config.h"
#include "efuse.h"
#include "led_control.h"
#include "log.h"

State::State(LedControl *ledControl) : m_ledControl(ledControl), m_ledSetupTimeout(0), m_identifyTimeout(0) {
    assert(m_ledControl);
    CreateMutux(&state_mutex);

    printDockInfo();
}

void State::loop() {
    // uptime calculation, yup this rolls over after ~47 days...
    auto now = millis();
    if (now < m_lastTick) {
        m_lastTick = 0;
    }
    if ((now - m_lastTick) >= 1000UL) {
        m_lastTick += 1000UL;
        m_upSS++;
        if (m_upSS >= 60) {
            m_upSS -= 60;
            m_upMM++;
        }
        if (m_upMM >= 60) {
            m_upMM -= 60;
            m_upHH++;
        }
        if (m_upHH >= 24) {
            m_upHH -= 24;
            m_upDays++;
        }
    }

    // check for timeouts. Note: if corresponding message to trigger action is sent again before the timeout, the
    // timeout is expanded!
    if (m_ledSetupTimeout != 0 && millis() > m_ledSetupTimeout) {
        m_ledSetupTimeout = 0;
        Log.debug(m_ctx, "LED setup timeout");
        event(LED_SETUP_DONE);
    }

    if (m_identifyTimeout != 0 && millis() > m_identifyTimeout) {
        m_identifyTimeout = 0;
        Log.debug(m_ctx, "Identify timeout");
        event(IDENTIFY_DONE);
    }
}

States State::getState() {
    while (!GetMutex(&state_mutex)) {
        delay(1);
    }
    auto state = m_currentState;
    ReleaseMutex(&state_mutex);

    return state;
}

States State::setState(States state) {
    while (!GetMutex(&state_mutex)) {
        delay(1);
    }

    // simple state transfer logic
    switch (state) {
        case LED_SETUP: {
            int active = 2000;
            m_ledSetupTimeout = millis() + active;
            if (m_prevState != state && m_currentState != state) {
                m_prevState = m_currentState;
            }
            break;
        }
        case IDENTIFY:
            m_identifyTimeout = millis() + 3000;
            if (m_prevState != state && m_currentState != state) {
                m_prevState = m_currentState;
            }
            break;

        default:
            m_prevState = m_currentState;
            break;
    }

    m_currentState = state;

    ReleaseMutex(&state_mutex);

    if (m_prevState != state) {
        Log.logf(Log.DEBUG, m_ctx, "%d -> %d", m_prevState, state);
        m_ledControl->setState(state);
    }

    // only run LED pattern but don't remain in this pseudo state representing an event.
    if (state == States::IR_LEARN_FAILED || state == States::IR_LEARN_OK) {
        m_currentState = m_prevState;
    }

    return m_prevState;
}

void State::event(Events event) {
    while (!GetMutex(&state_mutex)) {
        delay(1);  // "
    }

    switch (event) {
        case LED_SETUP_DONE:
        case IDENTIFY_DONE:
            Log.logf(Log.DEBUG, m_ctx, "Restoring state: %d", m_prevState);
            m_currentState = m_prevState;
            break;
        case CONN_SUCCESS_DONE:
            m_currentState = NORMAL;
            break;

        default:
            break;
    }

    ReleaseMutex(&state_mutex);

    m_ledControl->setState(m_currentState);
}

String State::getUptime() {
    char timestring[20];
    snprintf(timestring, sizeof(timestring), "%d days %02d:%02d:%02d", m_upDays, m_upHH, m_upMM, m_upSS);
    return String(timestring);
}

void State::reboot() {
    Log.warn(m_ctx, "About to reboot...");
    delay(2000);
    Log.warn(m_ctx, "Now rebooting...");
    ESP.restart();
}

void State::printDockInfo() {
    auto   cfg = Config::getInstance();
    Efuse &efuse = Efuse::getInstance();
    Serial.println("");
    Serial.println("");
    Serial.println("############################################################");
    Serial.printf("## Unfolded Circle Smart Charging Dock %-19s##\n", cfg->getModel());
    Serial.printf("## Version  : %-44s##\n", cfg->getSoftwareVersion().c_str());
    Serial.printf("## Device   : %-6s / %-35s##\n", efuse.getModel(), efuse.getHwRevision());
    Serial.printf("## Serial   : %-44s##\n", cfg->getSerial());
    Serial.printf("## Hostname : %-44s##\n", cfg->getHostName().c_str());
    Serial.println("## Visit http://unfoldedcircle.com/ for more information  ##");
    Serial.println("############################################################");
    Serial.println("");
}
