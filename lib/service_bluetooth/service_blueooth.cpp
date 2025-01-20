// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_blueooth.h"

#include "log.h"

BluetoothService::BluetoothService(State *state, Config *config, API *api)
    : m_state(state), m_config(config), m_api(api) {
    assert(state);
    assert(config);
    assert(api);
}

// Bt_Status callback function
void Bt_Status(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_SRV_OPEN_EVT:
            Log.info("BT", "Client Connected");
            break;
        case ESP_SPP_CLOSE_EVT:
            Log.info("BT", "Client disconnected");
            break;
        default:
            Log.logf(Log.DEBUG, "BT", "Event: %d", event);
    }
}

void BluetoothService::init() {
    m_bluetooth.register_callback(Bt_Status);

    if (m_bluetooth.begin(m_config->getHostName())) {
        m_bluetooth.setTimeout(500);
        Log.info(m_ctx, "Initialized. Ready for setup.");
    } else {
        Log.error(m_ctx, "Failed to initialize.");
    }
}

void BluetoothService::handle() {
    if (!m_bluetooth.available()) {
        return;
    }

    // Hackish but simple: rely on single line json messages terminated by newline.
    // Avoids writing a parser with proper curly brackets handling...
    // TODO(zehnm) recheck ArduinoJson library if it supports streaming input
    char buffer[1024];
    auto count = m_bluetooth.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    if (count > 0) {
        buffer[count] = '0';  // readBytesUntil doesn't zero-terminate the buffer!
        m_api->processRequest(buffer, API::Bluetooth, [this](String response) -> void {
            Log.logf(Log.DEBUG, m_ctx, "Sending response: '%s'", response.c_str());
            if (!response.isEmpty()) {
                m_bluetooth.println(response);
                m_bluetooth.flush();
            }
        });
    }
}
