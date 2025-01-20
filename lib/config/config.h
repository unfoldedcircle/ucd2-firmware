// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "log.h"

class Config {
 public:
    // FIXME singleton handling
    Config();
    virtual ~Config() {}

    static Config* getInstance() { return s_instance; }

    /**
     * Returns the LED brightness value. Range is 5..255.
     */
    int getLedBrightness();
    /**
     * Set LED brightness value between 5..255. Values outside that range will set the default value.
     */
    void setLedBrightness(int value);

    int  getEthLedBrightness();
    void setEthLedBrightness(int value);

    // getter and setter for dock friendly name
    String getFriendlyName();
    /**
     * Maximum length is 40 characters. Longer names will be cut.
     */
    void setFriendlyName(String value);

    // getter and setter for wifi settings
    String getWifiSsid();
    String getWifiPassword();

    /**
     * Sets the WiFi SSID and password. Returns false if ssid > 32 or password > 63.
     *
     * Returns true if the values were stored.
     */
    bool setWifi(String ssid, String password);

    String getToken();
    /**
     * Sets the WebSocket connection token. Maximum length is 64 characters. Longer tokens are ignored.
     *
     * Returns true if the token was stored.
     */
    bool setToken(String value);

    bool setLogLevel(UCLog::Level level);
    bool setSyslogServer(const String& server, uint16_t port = 514);
    bool enableSyslog(bool enable);

    bool getTestMode();
    bool setTestMode(bool enable);

    UCLog::Level getLogLevel();
    String       getSyslogServer();
    uint16_t     getSyslogServerPort();
    bool         isSyslogEnabled();

    // get hostname
    String getHostName();

    // get serial from device
    const char* getSerial() const;
    // get model number from device
    const char* getModel() const;
    // get device hardware revision
    const char* getRevision() const;

    // get software version
    String getSoftwareVersion();

    // SNTP server
    bool   enableNtp(bool enable);
    bool   isNtpEnabled();
    bool   setNtpServer(const String& server1, const String& server2);
    String getNtpServer1();
    String getNtpServer2();

    // IR sending configuration
    uint16_t getIrSendCore();
    bool     setIrSendCore(uint16_t core);
    uint16_t getIrSendPriority();
    bool     setIrSendPriority(uint16_t priority);
    uint16_t getIrLearnCore();
    bool     setIrLearnCore(uint16_t core);
    uint16_t getIrLearnPriority();
    bool     setIrLearnPriority(uint16_t priority);

    // reset config to defaults
    void reset();

    static const int OTA_port = 80;
    static const int API_port = 946;

 private:
    static Config* s_instance;

    String   getStringSetting(const char* partition, const char* key, const String defaultValue = String());
    bool     getBoolSetting(const char* partition, const char* key, bool defaultValue = false);
    uint16_t getUShortSetting(const char* partition, const char* key, uint16_t defaultValue = 0);
    int      getIntSetting(const char* partition, const char* key, int defaultValue = 0);

    Preferences m_preferences;
    int         m_defaultLedBrightness = 50;
    String      m_hostname;
    String      m_swVersion = DOCK_VERSION;  // see build_firmware_version.py

    const char* m_prefGeneral = "general";
    const char* m_prefWifi = "wifi";
    const char* m_defToken = "0000";
    const char* m_ctx = "CFG";

    static const uint16_t DEF_IRSEND_CORE = 1;   // doesn't work well on core 0, only with a very high priority
    static const uint16_t DEF_IRSEND_PRIO = 12;  // seems to work well with half of max priority 24
    static const uint16_t DEF_IRLEARN_CORE = 1;  // never tested on core 0, seems to work well on 1
    static const uint16_t DEF_IRLEARN_PRIO = 5;
};
