// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "log.h"

#include <Syslog.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <cstdio>

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new empty syslog instance
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

void UCLog::setSyslog(const String hostname, const String server, uint16_t port) {
    m_hostname = hostname;
    m_syslogServer = server;
    m_syslogPort = port;
}

void UCLog::enableSyslog(const String hostname, const String server, uint16_t port) {
    m_hostname = hostname;
    m_syslogServer = server;
    m_syslogPort = port;

    syslog.server(m_syslogServer.c_str(), m_syslogPort);
    syslog.deviceHostname(m_hostname.c_str());
    syslog.appName("UCD2");
    syslog.defaultPriority(LOG_DAEMON);

    m_syslogEnabled = true;
}

void UCLog::enableSyslog(bool enable) {
    m_syslogEnabled = enable;
}

void UCLog::setFilterLevel(Level level) {
    m_logFilter = level;
}

UCLog::Level UCLog::getFilterLevel() {
    return m_logFilter;
}

void UCLog::vlogf(Level level, const char *context, const char *fmt, va_list args) {
    if (level > m_logFilter) {
        return;
    }
    char  *message;
    size_t initialLen;
    size_t len;

    initialLen = strlen(fmt);

    message = new char[initialLen + 1];

    len = vsnprintf(message, initialLen + 1, fmt, args);
    if (len > initialLen) {
        delete[] message;
        message = new char[len + 1];

        vsnprintf(message, len + 1, fmt, args);
    }

    this->writeLog(level, context, message);

    delete[] message;
}

void UCLog::logf(Level level, const char *context, const char *fmt, ...) {
    if (level > m_logFilter) {
        return;
    }
    va_list args;

    va_start(args, fmt);
    this->vlogf(level, context, fmt, args);
    va_end(args);
}

void UCLog::log(Level level, const char *context, const char *message) {
    this->writeLog(level, context, message);
}

void UCLog::log(Level level, const char *context, const String &message) {
    this->writeLog(level, context, message.c_str());
}

void UCLog::log(Level level, const char *context, const __FlashStringHelper *message) {
    this->writeLog(level, context, message);
}

const char *levels[] = {"F", "A", "C", "E", "W", "N", "I", "D"};

inline void UCLog::writeLog(Level level, const char *context, const char *message) {
    if (level > m_logFilter) {
        return;
    }

    if (m_serialEnabled) {
        Serial.printf("%s | %10ld | %-5s | ", levels[static_cast<int>(level)], millis(), context);
        Serial.println(message);
    }

    if (m_syslogEnabled) {
        // TODO(zehnm) ringbuffer/queue & retry later?
        if (isNetworkConnected()) {
            syslog.logf(level, "%-5s | %s", context, message);
        }
    }
}

inline void UCLog::writeLog(Level level, const char *context, const __FlashStringHelper *message) {
    if (level > m_logFilter) {
        return;
    }

    if (m_serialEnabled) {
        Serial.printf("%s | %10ld | %-5s | ", levels[static_cast<int>(level)], millis(), context);
        Serial.println(message);
    }

    if (m_syslogEnabled) {
        // TODO(zehnm) ringbuffer/queue & retry later?
        if (isNetworkConnected()) {
            syslog.log(level, message);
        }
    }
}

bool UCLog::isNetworkConnected() {
    auto status = WiFi.getStatusBits();
    return (status & (ETH_CONNECTED_BIT | STA_CONNECTED_BIT)) &&
           ((status & (ETH_HAS_IP_BIT | ETH_HAS_IP6_BIT)) || (status & (STA_HAS_IP_BIT | STA_HAS_IP6_BIT)));
}

UCLog &UCLog::getInstance() {
    static UCLog instance;
    return instance;
}

UCLog &Log = Log.getInstance();
