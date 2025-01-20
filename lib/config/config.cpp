// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <Arduino.h>
#include <WiFi.h>

#include <cstdio>

#include "board.h"
#include "efuse.h"
#include "log.h"

Config* Config::s_instance = nullptr;

// initializing config
Config::Config() {
    s_instance = this;

    // hostname and serial number only need to be read once and not for every request
    char    dockHostName[] = "UC-Dock-xxxxxxxxxxxx";
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    snprintf(dockHostName, sizeof(dockHostName), "UC-Dock-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2],
             baseMac[3], baseMac[4], baseMac[5]);

    m_hostname = dockHostName;

    // if no friendly name is set, use mac address
    if (getFriendlyName().isEmpty()) {
        // get the default friendly name
        Log.debug(m_ctx, "Setting default friendly name");
        setFriendlyName(m_hostname);
    }
}

// getter and setter for brightness value
int Config::getLedBrightness() {
    return getIntSetting(m_prefGeneral, "brightness", m_defaultLedBrightness);
}

void Config::setLedBrightness(int value) {
    if (value < 0 || value > 255) {
        Log.debug(m_ctx, "Setting default brightness");
        value = m_defaultLedBrightness;
    }
    m_preferences.begin(m_prefGeneral, false);
    m_preferences.putInt("brightness", value);
    m_preferences.end();
}

int Config::getEthLedBrightness() {
    return getIntSetting(m_prefGeneral, "eth_brightness", m_defaultLedBrightness);
}

void Config::setEthLedBrightness(int value) {
    if (value < 0 || value > 255) {
        Log.debug(m_ctx, "Setting default ETH LED brightness");
        value = m_defaultLedBrightness;
    }
    m_preferences.begin(m_prefGeneral, false);
    m_preferences.putInt("eth_brightness", value);
    m_preferences.end();
}

// getter and setter for dock friendly name
String Config::getFriendlyName() {
    String friendlyName = getStringSetting(m_prefGeneral, "friendly_name", "");

    // quick fix
    if (friendlyName == "null") {
        friendlyName.clear();
    }

    return friendlyName.isEmpty() ? getHostName() : friendlyName;
}

void Config::setFriendlyName(String value) {
    // quick fix
    if (value == "null") {
        value.clear();
    }

    m_preferences.begin(m_prefGeneral, false);
    m_preferences.putString("friendly_name", value.substring(0, 40));
    m_preferences.end();
}

// getter and setter for wifi credentials
String Config::getWifiSsid() {
    return getStringSetting(m_prefWifi, "ssid", "");
}

String Config::getWifiPassword() {
    return getStringSetting(m_prefWifi, "password", "");
}

bool Config::setWifi(String ssid, String password) {
    if (ssid.length() > 32 || password.length() > 63) {
        return false;
    }
    if (!m_preferences.begin(m_prefWifi, false)) {
        return false;
    }
    m_preferences.putString("ssid", ssid);
    m_preferences.putString("password", password);
    m_preferences.end();

    return true;
}

bool Config::setLogLevel(UCLog::Level level) {
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putUShort("log_level", level);

    m_preferences.end();
    return true;
}

bool Config::setSyslogServer(const String& server, uint16_t port) {
    if (server.length() > 64) {
        Log.warn(m_ctx, "Ignoring syslog server: name too long");
        return false;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putString("syslog_server", server);
    if (port == 0) {
        port = 514;
    }
    m_preferences.putUShort("syslog_port", port);

    m_preferences.end();
    return true;
}

bool Config::enableSyslog(bool enable) {
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putBool("syslog_enabled", enable);

    m_preferences.end();
    return true;
}

bool Config::getTestMode() {
    return getBoolSetting(m_prefGeneral, "testmode", false);
}

bool Config::setTestMode(bool enable) {
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putBool("testmode", enable);

    m_preferences.end();
    return true;
}

String Config::getToken() {
    return getStringSetting(m_prefGeneral, "token", m_defToken);
}

bool Config::setToken(String value) {
    if (value.length() > 64) {
        return false;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putString("token", value);
    m_preferences.end();

    return true;
}

UCLog::Level Config::getLogLevel() {
    auto level = getUShortSetting(m_prefGeneral, "log_level", static_cast<uint16_t>(UCLog::Level::DEBUG));

    if (level >= 0 && level <= 7) {
        return static_cast<UCLog::Level>(level);
    }

    return UCLog::Level::DEBUG;
}

String Config::getSyslogServer() {
    return getStringSetting(m_prefGeneral, "syslog_server", "");
}

uint16_t Config::getSyslogServerPort() {
    return getUShortSetting(m_prefGeneral, "syslog_port", 514);
}

bool Config::isSyslogEnabled() {
    return getBoolSetting(m_prefGeneral, "syslog_enabled", false);
}

String Config::getHostName() {
    return m_hostname;
}

const char* Config::getSerial() const {
    return Efuse::getInstance().getSerial();
}

const char* Config::getModel() const {
    auto efuseModel = Efuse::getInstance().getModel();
    return strlen(efuseModel) ? efuseModel : hwModel;
}

const char* Config::getRevision() const {
    auto efuseRev = Efuse::getInstance().getHwRevision();
    return strlen(efuseRev) ? efuseRev : hwRevision;
}

String Config::getSoftwareVersion() {
    if (m_swVersion.startsWith("v")) {
        return m_swVersion.substring(1);
    } else {
        return m_swVersion;
    }
}

bool Config::enableNtp(bool enable) {
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putBool("ntp_enabled", enable);

    m_preferences.end();
    return true;
}

bool Config::isNtpEnabled() {
    return getBoolSetting(m_prefGeneral, "ntp_enabled", false);
}

bool Config::setNtpServer(const String& server1, const String& server2) {
    if (server1.length() > 32 || server2.length() > 32) {
        Log.warn(m_ctx, "Ignoring ntp server: name too long");
        return false;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putString("ntp_server1", server1);
    m_preferences.putString("ntp_server2", server2);

    m_preferences.end();
    return true;
}

String Config::getNtpServer1() {
    return getStringSetting(m_prefGeneral, "ntp_server1", "pool.ntp.org");
}

String Config::getNtpServer2() {
    return getStringSetting(m_prefGeneral, "ntp_server2", "");
}

uint16_t Config::getIrSendCore() {
    return getUShortSetting(m_prefGeneral, "irsend_core", DEF_IRSEND_CORE);
}

bool Config::setIrSendCore(uint16_t core) {
    if (core > 1) {
        core = 1;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putUShort("irsend_core", core);
    m_preferences.end();
    return true;
}

uint16_t Config::getIrSendPriority() {
    return getUShortSetting(m_prefGeneral, "irsend_prio", DEF_IRSEND_PRIO);
}

bool Config::setIrSendPriority(uint16_t priority) {
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putUShort("irsend_prio", priority);
    m_preferences.end();
    return true;
}

uint16_t Config::getIrLearnCore() {
    return getUShortSetting(m_prefGeneral, "irlearn_core", DEF_IRLEARN_CORE);
}

bool Config::setIrLearnCore(uint16_t core) {
    if (core > 1) {
        core = 1;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putUShort("irlearn_core", core);
    m_preferences.end();
    return true;
}

uint16_t Config::getIrLearnPriority() {
    return getUShortSetting(m_prefGeneral, "irlearn_prio", DEF_IRLEARN_PRIO);
}

bool Config::setIrLearnPriority(uint16_t priority) {
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
    }
    if (!m_preferences.begin(m_prefGeneral, false)) {
        return false;
    }
    m_preferences.putUShort("irlearn_prio", priority);
    m_preferences.end();
    return true;
}

// reset config to defaults
void Config::reset() {
    Log.warn(m_ctx, "Resetting configuration.");

    Log.debug(m_ctx, "Resetting general.");
    m_preferences.begin(m_prefGeneral, false);
    m_preferences.clear();
    m_preferences.end();

    Log.debug(m_ctx, "Resetting general done.");

    delay(500);

    Log.debug(m_ctx, "Resetting wifi.");
    m_preferences.begin(m_prefWifi, false);
    m_preferences.clear();
    m_preferences.end();

    Log.debug(m_ctx, "Resetting wifi done.");

    delay(500);

    Log.debug(m_ctx, "Erasing flash.");
    int err;
    err = nvs_flash_init();
    Log.logf(Log.DEBUG, m_ctx, "nvs_flash_init: %d", err);
    err = nvs_flash_erase();
    Log.logf(Log.DEBUG, m_ctx, "nvs_flash_erase: %d", err);

    delay(500);

    ESP.restart();
}

String Config::getStringSetting(const char* partition, const char* key, const String defaultValue) {
    m_preferences.begin(partition, false);
    auto value = m_preferences.getString(key, defaultValue);
    m_preferences.end();

    return value;
}

bool Config::getBoolSetting(const char* partition, const char* key, bool defaultValue) {
    m_preferences.begin(partition, false);
    auto value = m_preferences.getBool(key, defaultValue);
    m_preferences.end();

    return value;
}

uint16_t Config::getUShortSetting(const char* partition, const char* key, uint16_t defaultValue) {
    m_preferences.begin(partition, false);
    auto value = m_preferences.getUShort(key, defaultValue);
    m_preferences.end();

    return value;
}

int Config::getIntSetting(const char* partition, const char* key, int defaultValue) {
    m_preferences.begin(partition, false);
    auto value = m_preferences.getInt(key, defaultValue);
    m_preferences.end();

    return value;
}
