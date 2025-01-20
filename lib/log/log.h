// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>

class UCLog {
 public:
    // Mapped to Syslog LOG_EMERG .. LOG_DEBUG
    enum Level {
        /* system is unusable */
        EMERG = 0,
        /* action must be taken immediately */
        ALERT = 1,
        /* critical conditions */
        CRIT = 2,
        /* error conditions */
        ERROR = 3,
        /* warning conditions */
        WARN = 4,
        /* normal but significant condition */
        NOTICE = 5,
        /* informational */
        INFO = 6,
        /* debug-level messages */
        DEBUG = 7
    };

    static UCLog &getInstance();

    void enableSerial(bool enable) { m_serialEnabled = enable; }

    void setSyslog(const String hostname, const String server, uint16_t port = 514);
    void enableSyslog(bool enable);
    void enableSyslog(const String hostname, const String server, uint16_t port = 514);

    void  setFilterLevel(Level level);
    Level getFilterLevel();

    void vlogf(Level level, const char *context, const char *fmt, va_list args) __attribute__((format(printf, 4, 0)));
    void logf(Level level, const char *context, const char *fmt, ...) __attribute__((format(printf, 4, 5)));

    void log(Level level, const char *context, const String &message);
    void log(Level level, const char *context, const char *message);
    void log(Level level, const char *context, const __FlashStringHelper *message);

    void debug(const char *context, const String &message) { log(DEBUG, context, message); }
    // void debug(char *)
    // void debug(const char *)
    // void debug(const __FlashStringHelper *)
    template <typename TChar>
    void debug(const char *context, TChar *message) {
        log(DEBUG, context, message);
    }

    void info(const char *context, const String &message) { log(INFO, context, message); }
    // void info(char *)
    // void info(const char *)
    // void info(const __FlashStringHelper *)
    template <typename TChar>
    void info(const char *context, TChar *message) {
        log(INFO, context, message);
    }

    void warn(const char *context, const String &message) { log(WARN, context, message); }
    // void warn(char *)
    // void warn(const char *)
    // void warn(const __FlashStringHelper *)
    template <typename TChar>
    void warn(const char *context, TChar *message) {
        log(WARN, context, message);
    }

    void error(const char *context, const String &message) { log(ERROR, context, message); }
    // void error(char *)
    // void error(const char *)
    // void error(const __FlashStringHelper *)
    template <typename TChar>
    void error(const char *context, TChar *message) {
        log(ERROR, context, message);
    }

 private:
    UCLog() = default;

    UCLog(const UCLog &) = delete;  // no copying
    UCLog &operator=(const UCLog &) = delete;

    void writeLog(Level level, const char *context, const char *message);
    void writeLog(Level level, const char *context, const __FlashStringHelper *message);

    bool isNetworkConnected();

    Level m_logFilter = Level::INFO;

    bool m_serialEnabled = true;
    bool m_syslogEnabled = false;

    String   m_hostname;
    String   m_syslogServer;
    uint16_t m_syslogPort;
};

extern UCLog &Log;
