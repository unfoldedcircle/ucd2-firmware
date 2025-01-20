// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <Arduino.h>
#include <WiFiUdp.h>
#include <config.h>
#include <led_control.h>
#include <log.h>
#include <service_api.h>
#include <service_blueooth.h>
#include <service_ir.h>
#include <service_mdns.h>
#include <service_network.h>
#include <service_ota.h>
#include <state.h>

#include "board.h"
#include "globalcache_server.h"

// Services
Config*            config = nullptr;
State*             state = nullptr;
GlobalCacheServer* gcServer = nullptr;
NetworkService*    networkService = nullptr;
BluetoothService*  bluetoothService = nullptr;
OtaService*        otaService = nullptr;
API*               api = nullptr;

bool buttonPressed = false;
bool resetMarker = false;

const int64_t TIMER_RESET_TIME = 9223372036854775807;
int64_t       buttonTimerSet = TIMER_RESET_TIME;

int         counter = 0;
const char* ctx = "MAIN";

// in case we ever have to increase the stack size: https://github.com/espressif/arduino-esp32/pull/5173
// SET_LOOP_TASK_STACK_SIZE( 16*1024 ); // 16KB

////////////////////////////////////////////////////////////////
// CHARGING PIN SETUP
////////////////////////////////////////////////////////////////
// charging pin loop handle
void handleCharging() {
    // TODO(marton) use millis() timeout
    counter++;
    if (counter == 4000) {
        int value = analogRead(CHARGE_SENSE_GPIO);

        // Log.logf(Log.DEBUG, ctx, "Charge sense pin value: %d", value);

        if (value > 300 && state->getState() != States::NORMAL_CHARGING) {
            Log.info(ctx, "Remote is charging");
            state->setState(States::NORMAL_CHARGING);
        } else if (value < 300 && state->getState() == States::NORMAL_CHARGING) {
            Log.info(ctx, "Remote is not charging");
            state->setState(States::NORMAL);
        }
        counter = 0;
    }
}

void setupChargingPin() {
    pinMode(CHARGE_SENSE_GPIO, INPUT);
    adcAttachPin(CHARGE_SENSE_GPIO);
    analogReadResolution(11);
    analogSetAttenuation(ADC_6db);

    int value = analogRead(CHARGE_SENSE_GPIO);
    if (value > 200) {
        state->setState(States::NORMAL_CHARGING);
    }
#if defined(CHARGE_ENABLE_GPIO)
    pinMode(CHARGE_ENABLE_GPIO, OUTPUT);
    digitalWrite(CHARGE_ENABLE_GPIO, 1);
#endif
}

// button press interrupt callback
void handleButtonPress() {
    // TODO(zehnm) simplify with millis(). See
    // https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover/12588#12588 for correct
    // implementation
    const int64_t timerCurrent = esp_timer_get_time();
    if (digitalRead(BUTTON_GPIO) == LOW && !buttonPressed) {
        buttonPressed = true;
        buttonTimerSet = timerCurrent;
        // we can't use the logger in an ISR!
        Serial.println("Button is pressed");
    } else if (digitalRead(BUTTON_GPIO) == HIGH && buttonPressed) {
        buttonPressed = false;
        const int elapsedTimeInMiliSeconds = ((timerCurrent - buttonTimerSet) / 1000);
        // we can't use the logger in an ISR!
        Serial.printf("Button held for %d ms\n", elapsedTimeInMiliSeconds);
        buttonTimerSet = TIMER_RESET_TIME;

        if (elapsedTimeInMiliSeconds > 3000) {
            resetMarker = true;
        }
    }
}

// GPIO button pin
void setupButtonPin() {
    pinMode(BUTTON_GPIO, INPUT);
    attachInterrupt(BUTTON_GPIO, handleButtonPress, CHANGE);
}

// Ethernet clock enable
void setupEthClock() {
#if defined(HAS_ETHERNET)
    pinMode(ETH_CLK_EN, OUTPUT);
    digitalWrite(ETH_CLK_EN, 1);

    delay(2000);
#endif
}

////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////
void setup() {
    Serial.begin(115200);

    setenv("TZ", "UTC", 1);
    tzset();

    // FIXME singleton handling
    config = new Config();

    Log.setFilterLevel(config->getLogLevel());

    ledControl.setLedMaxBrightness(config->getLedBrightness());
    ledControl.init(config->getTestMode());
    ledControl.setEthLedBrightness(0);  // controlled in NetworkService

    state = new State(&ledControl);
    // TODO(#69) old startup logic quirk to stay in BT setup mode if network is not active & amber blinking at start
    state->setState(States::SETUP);

    networkService = new NetworkService(state, config, &ledControl);
    otaService = new OtaService(config, state);

    // BUTTON PIN setup
    setupButtonPin();

    // CHARGING PIN setup
    setupChargingPin();

    // Ethernet clock enable setup
    setupEthClock();

    // initialize all services
    networkService->init();
    irService.init(config->getIrSendCore(), config->getIrSendPriority(), config->getIrLearnCore(),
                   config->getIrLearnPriority(), state);

    gcServer = new GlobalCacheServer(state, &irService, config);

    api = new API(config, state, networkService, &irService, &ledControl);
    api->init();

    bluetoothService = new BluetoothService(state, config, api);
    otaService->init();

    // FIXME(#54) ETH might not yet be connected at this time, better use a state machine
    if (networkService->isEthConnected()) {
        state->setState(States::CONN_SUCCESS);
        networkService->setWifiEnabled(false);
    } else if (networkService->isEthLinkUp()) {
        state->setState(States::CONNECTING);
        networkService->setWifiEnabled(false);
    } else if (!(config->getWifiSsid().isEmpty() || config->getWifiPassword().isEmpty())) {
        // TODO(#54) this doesn't work with an open network
        Log.info(ctx, "ETH not connected and Wifi cfg available: connecting to Wifi...");
        state->setState(States::CONNECTING);
        // trigger WiFi connection, otherwise we have to wait until WiFi reconnect kicks in (~30 sec!)
        networkService->setWifiEnabled(true);
    } else {
        networkService->setWifiEnabled(false);
    }

    // TODO(zehnm) add handler for network connection, or buffer syslog messages
    if (config->isSyslogEnabled()) {
        auto server = config->getSyslogServer();
        auto port = config->getSyslogServerPort();
        Log.logf(Log.DEBUG, ctx, "Enabling syslog: %s:%d, level %d", server.c_str(), port, config->getLogLevel());
        if (!server.isEmpty()) {
            Log.enableSyslog(config->getHostName(), server, port);
        }
    }

    // initialize Bluetooth
    if (state->getState() == States::SETUP) {
        bluetoothService->init();
    } else {
        // make sure Bluetooth is not running
        btStop();
    }

    Log.debug(ctx, "Initialized, entering main loop");
}

////////////////////////////////////////////////////////////////
// Main LOOP
////////////////////////////////////////////////////////////////
void loop() {
    // handle charging
    handleCharging();

    // Handle api calls
    api->loop();

    if (state->getState() == States::SETUP) {
        // Handle incoming bluetooth serial data
        bluetoothService->handle();
    } else {
        // Handle wifi disconnects.
        networkService->handleLoop();

        // handle MDNS
        MdnsService.loop();

        // Handle OTA updates.
        otaService->loop();
    }

    // reset if marker is set
    if (resetMarker) {
        // quick and dirty LED notification
        state->setState(States::ERROR);
        // config reset will never come back and reboot the ESP
        config->reset();
    }

    state->loop();
}
