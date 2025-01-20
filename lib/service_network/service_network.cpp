// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_network.h"

#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <sntp.h>

#include "log.h"


// Callback function (get's called when time adjusts via NTP)
// ⚠️ WARNING: this seems to be called from an ISR!
// If you call any other function causing an interrupt the program crashes!
void timeavailable(struct timeval *t) {
    // do NOT call any `Log` statements or we are dead in the water!
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);

    Serial.println(&timeinfo, "Time adjustment from NTP: %Y-%m-%d %H:%M:%S %Z");
}

NetworkService::NetworkService(State* state, Config* config, LedControl *ledControl)
    : m_state(state), m_config(config), m_led(ledControl) {
    assert(state);
    assert(config);
    assert(ledControl);
}

void NetworkService::init() {
    // Use NTP to get time, accept DHCP server
    //
    // Try and error from: https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Time/SimpleTime/SimpleTime.ino

    if (m_config->isNtpEnabled()) {
        // set notification call-back function
        sntp_set_time_sync_notification_cb(timeavailable);

        // NTP server address could be aquired via DHCP
        // TODO(zehnm) NTP server setting over DHCP doesn't seem to work :-(
        sntp_servermode_dhcp(1);

        // stripped down version from `configTzTime`, setting TZ in main.cpp
        // configTzTime("UTC", "pool.ntp.org");
        sntp_setoperatingmode(SNTP_OPMODE_POLL);

        // Attention: sntp_setservername does NOT make a copy of the provided string! That's why we use class variables
        // not setting index 0 _should_ allow DHCP provided server
        m_sntpServer1 = m_config->getNtpServer1();
        if (!m_sntpServer1.isEmpty()) {
            Log.logf(Log.INFO, m_ctx, "SNTP server 1: %s", m_sntpServer1);
            sntp_setservername(1, m_sntpServer1.c_str());
        }
        m_sntpServer2 = m_config->getNtpServer2();
        if (!m_sntpServer2.isEmpty()) {
            Log.logf(Log.INFO, m_ctx, "SNTP server 2: %s", m_sntpServer2);
            sntp_setservername(2, m_sntpServer2.c_str());
        }
        sntp_init();
        Log.debug(m_ctx, "SNTP initialized");
    }

    WiFi.onEvent(std::bind(&NetworkService::WiFiEvent, this, std::placeholders::_1));
#if defined(HAS_ETHERNET)

    gpio_set_drive_capability((gpio_num_t)ETH_TXD0_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_TXD1_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_TXEN_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_RXD0_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_RXD1_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_CRS_DV_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_MDC_PIN, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability((gpio_num_t)ETH_MDIO_PIN, GPIO_DRIVE_CAP_0);

    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLOCK_GPIO0_IN);
#else
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(m_config->getHostName().c_str());
    WiFi.begin();
#endif

    Log.debug(m_ctx, "Initialized");
}

void NetworkService::handleLoop() {
    if (m_ethLedTimeout != 0 && millis() > m_ethLedTimeout) {
        m_ethLedTimeout = 0;
        m_led->setEthLedBrightness(0);
    }

    if (!m_wifiEnabled) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED && (millis() > m_wifiCheckTimedUl)) {
        m_wifiReconnectCount++;

        if (m_wifiReconnectCount == 5) {
            Log.logf(Log.ERROR, m_ctx, "Wifi reconnection failed after %d attempts. Rebooting!", m_wifiReconnectCount);
            m_state->reboot();
        }

        Log.info(m_ctx, "Wifi disconnected");
        m_wifiPrevState = false;
        disconnect();
        delay(2000);
        m_state->setState(States::CONNECTING);
        Log.info(m_ctx, "Wifi reconnecting");
        connect(m_config->getWifiSsid(), m_config->getWifiPassword());

        m_wifiCheckTimedUl = millis() + 30000;
    }

    // TODO(zehnm) restart MDNS if wifi is connected again
    // This could be put into NetworkService::WiFiEvent. However, when switching network interfaces we are not always
    // getting notified! So this approach might be ok...
    if (WiFi.status() == WL_CONNECTED && m_wifiPrevState == false) {
        m_wifiReconnectCount = 0;
        Log.logf(Log.INFO, m_ctx, "Wifi connected. IP: %s", WiFi.localIP().toString().c_str());
        m_wifiPrevState = true;
        m_state->setState(States::CONN_SUCCESS);
    }
}

void NetworkService::connect(String ssid, String password) {
    Log.info(m_ctx, "Wifi connecting...");
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(m_config->getHostName().c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    m_state->setState(States::CONNECTING);
}

void NetworkService::disconnect() {
    WiFi.disconnect();
}

void NetworkService::setWifiEnabled(bool enabled) {
    m_wifiEnabled = enabled;

    if (m_wifiEnabled) {
        Log.info(m_ctx, "Wifi enabled");
        connect(m_config->getWifiSsid(), m_config->getWifiPassword());
    } else {
        Log.info(m_ctx, "Wifi disabled");
        WiFi.disconnect();
    }
}

void NetworkService::WiFiEvent(WiFiEvent_t event) {
    // DANGER Will Robinson! This switched sometimes from system_event_id_t to arduino_event_id_t! The method signature
    // is wrong!!!
    switch (event) {
#if defined(HAS_ETHERNET)
        case ARDUINO_EVENT_ETH_START:
            Log.debug(m_ctx, "ETH Started");
            ETH.setHostname(m_config->getHostName().c_str());
            break;
        case ARDUINO_EVENT_ETH_CONNECTED: {
            Log.info(m_ctx, "ETH link up: waiting for DHCP response");
            m_ethLinkUp = true;

            int brightness = m_config->getEthLedBrightness();
            if (brightness == 0) {
                brightness = 3;
                Log.debug(m_ctx, "ETH LED brightness set to 0: waiting for IP then disabling LED after 3s");
            }
            m_led->setEthLedBrightness(brightness);
            break;
        }
        // case ARDUINO_EVENT_ETH_GOT_IP6:  // NOT YET TESTED! Not even sure if IPv6 is enabled
        case ARDUINO_EVENT_ETH_GOT_IP:
            Log.logf(Log.INFO, m_ctx, "ETH MAC: %s, IPv4: %s%s, %dMbps", ETH.macAddress().c_str(),
                     ETH.localIP().toString().c_str(), ETH.fullDuplex() ? ", FULL_DUPLEX" : "", ETH.linkSpeed());
            m_ethConnected = true;
            if (m_config->getEthLedBrightness() == 0) {
                m_ethLedTimeout = millis() + 3000;
            }
            setWifiEnabled(false);
            m_state->setState(States::CONN_SUCCESS);
            if (m_config->isNtpEnabled()) {
                for (int i=0; i < 3; i++) {
                    auto server = sntp_getservername(i);
                    if (server) {
                        Log.logf(Log.INFO, m_ctx, "SNTP server name %d: %s", i, server);
                    }
                }
            }
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Log.info(m_ctx, "ETH Disconnected");
            m_ethLinkUp = false;
            m_ethConnected = false;
            m_led->setEthLedBrightness(0);
            if (m_config->isNtpEnabled()) {
                sntp_stop();  // required?
            }
            setWifiEnabled(true);
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Log.info(m_ctx, "ETH Stopped");
            m_ethConnected = false;
            m_led->setEthLedBrightness(0);
            setWifiEnabled(true);
            break;
#endif
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Log.info(m_ctx, "WiFi Disconnected");
            if (m_config->isNtpEnabled()) {
                sntp_stop();  // required?
            }
            break;
        // case ARDUINO_EVENT_WIFI_STA_GOT_IP6:  // NOT YET TESTED! Not even sure if IPv6 is enabled
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            // see NetworkService::handleLoop() above
            // Log.logf(Log.INFO, m_ctx, "Wifi connected. IP: %s", WiFi.localIP().toString().c_str());
            // m_wifiReconnectCount = 0;
            // m_wifiPrevState = true;
            // m_state->setState(States::CONN_SUCCESS);
            if (m_config->isNtpEnabled()) {
                for (int i=0; i < 3; i++) {
                    auto server = sntp_getservername(i);
                    if (server) {
                        Log.logf(Log.INFO, m_ctx, "SNTP server name %d: %s", i, server);
                    }
                }
            }
            break;
        default:
            // just for future debugging, in case the enum changes again
            Log.logf(Log.DEBUG, m_ctx, "WiFiEvent: %d", event);
    }
}
