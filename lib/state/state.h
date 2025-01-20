// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "led_control.h"
#include "mutex.h"
#include "states.h"

// TODO(zehnm) rewrite using a proper state machine with states and events, avoid loop()!
class State {
 public:
    explicit State(LedControl *ledControl);

    void   loop();
    States getState();
    States setState(States state);

    String getUptime();

    // reboots the ESP
    void reboot();
    void printDockInfo();

 private:
    enum Events { IDENTIFY_DONE, LED_SETUP_DONE, CONN_SUCCESS_DONE };

    State(const State &) = delete;  // no copying
    State &operator=(const State &) = delete;

    void event(Events event);

    u_long m_ledSetupTimeout;
    u_long m_identifyTimeout;

    // current state
    States  m_currentState = States::NOT_SET;
    States  m_prevState = States::NOT_SET;
    mutex_t state_mutex;

    // uptime: hours, minutes, seconds
    byte         m_upHH = 0;
    byte         m_upMM = 0;
    byte         m_upSS = 0;
    unsigned int m_upDays = 0;
    // time that the clock last "ticked"
    u_long      m_lastTick = 0;
    LedControl *m_ledControl = nullptr;

    const char *m_ctx = "STATE";
};
