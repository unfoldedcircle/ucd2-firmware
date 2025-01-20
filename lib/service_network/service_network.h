// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "board.h"
#include "config.h"
#include "led_control.h"
#include "state.h"


class NetworkService {
 public:
    explicit NetworkService(State *state, Config *config, LedControl *ledControl);
    virtual ~NetworkService() {}

    void init();
    void handleLoop();

    void connect(String ssid, String password);
    void disconnect();

    bool isWifiEnabled() { return m_wifiEnabled; }
    void setWifiEnabled(bool enabled);

    bool isEthLinkUp() { return m_ethLinkUp; }
    bool isEthConnected() { return m_ethConnected; }

 private:
    void WiFiEvent(WiFiEvent_t event);

    State      *m_state;
    Config     *m_config;
    LedControl *m_led;

    bool   m_wifiPrevState = false;  // previous WIFI connection state; 0 - disconnected, 1 - connected
    u_long m_wifiCheckTimedUl = 10000;
    int    m_wifiReconnectCount = 0;
    u_long m_ethLedTimeout = 0;

    bool m_wifiEnabled = true;
    bool m_ethLinkUp = false;
    bool m_ethConnected = false;

    String m_sntpServer1;
    String m_sntpServer2;

    const char *m_ctx = "NET";
};
