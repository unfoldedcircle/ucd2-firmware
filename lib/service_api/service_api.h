// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <config.h>
#include <led_control.h>
#include <service_ir.h>
#include <service_network.h>
#include <state.h>

#include <unordered_set>

// Callback function for the API response message
typedef std::function<void(String)> ApiResponseCallbackFunction;

class API {
 public:
    enum Source {
        WebSocket = 0,
        Uart = 1,
        Bluetooth = 2,
    };
    explicit API(Config* config, State* state, NetworkService* networkService, InfraredService* irService,
                 LedControl* ledControl);
    virtual ~API() {}

    void init();
    void loop();

    /**
     * Process an API request.
     * 
     * The buffer must contain a JSON message and must be writeable for the ArduinoJson library to use zero-copy
    */
    bool processRequest(char* request, Source source, ApiResponseCallbackFunction cb, bool authenticated = true,
                        int id = -1);

    /**
     * Send a message to all authenticated clients
     */
    void sendMessage(String msg);

 private:
    void handleSerial();
    // writeable buffer required to use ArduinoJson zero-copy
    void processWsRequest(char* request, int id);

    WebSocketsServer            m_webSocketServer = WebSocketsServer(Config::API_port);
    std::unordered_set<uint8_t> m_authWsClients;

    Config*          m_config;
    State*           m_state;
    NetworkService*  m_networkService;
    InfraredService* m_irService;
    LedControl*      m_ledControl;

    States m_prevState;

    const char* m_ctx = "API";
};
