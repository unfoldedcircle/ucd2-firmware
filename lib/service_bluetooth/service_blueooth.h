// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <config.h>
#include <service_api.h>
#include <state.h>

class BluetoothService {
 public:
    explicit BluetoothService(State* state, Config* config, API* api);
    virtual ~BluetoothService() {}

    void init();
    void handle();

 private:
    BluetoothSerial m_bluetooth;
    State*          m_state;
    Config*         m_config;
    API*            m_api;

    String m_receivedData;
    bool   m_interestingData = false;

    const char* m_ctx = "BT";
};
