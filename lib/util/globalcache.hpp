// SPDX-FileCopyrightText: Copyright (c) 2023 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Common utility functions for Global Cache IR codes & TCP srever
// Make sure this file also compiles natively and all functions are covered by unit tests.

#pragma once

#include <stdint.h>

#include <algorithm>
#include <cstring>

#include "util_types.h"

/// @brief Parse a GlobalCache request message
/// @param request request message string **without** terminating line feed.
/// @param msg the GCMsg struct to store the parsed request.
/// @return 0 if successful, iTach error code otherwise.
/// @details Tested with the following iTack request messages:
/// - getversion[,module]
/// - getdevices
/// - blink,<mode>
/// - get_IR,<module>:<port>
/// - set_IR,<module>:<port>,<mode>
/// - sendir,<module>:<port>, <ID>,<freq>,<repeat>,<offset>, <on1>,<off1>,<on2>,<off2>,...,<onN>,<offN>
/// - stopir,<module>:<port>
/// - get_IRL
/// - stop_IRL
uint8_t parseGcRequest(const char *request, GCMsg *msg) {
    if (request == nullptr || msg == nullptr) {
        return false;
    }

    // command
    auto        len = sizeof(msg->command);
    const char *next = strchr(request, ',');
    if (next == NULL) {
        // simple command without module:port or parameters
        auto cmdLen = strlen(request);
        if (cmdLen >= len) {
            return 1;  // command name too long: unknown command
        }
        auto count = std::min(len, cmdLen);
        strncpy(msg->command, request, count);
        msg->command[count] = 0;
        msg->module = 0;
        msg->port = 0;
        msg->param = nullptr;
        return 0;
    }

    if ((next - request) >= len) {
        return 1;  // command name too long: unknown command
    }
    strncpy(msg->command, request, next - request);
    msg->command[next - request] = 0;

    // check for:  <module>:<port>,<param(s)>
    const char *current = next + 1;
    next = strchr(current, ',');
    if (next == NULL) {
        // no comma, check for: <module>:<port>
        next = strchr(current, ':');
        if (next == NULL) {
            msg->module = 0;
            msg->port = 0;
            msg->param = current;
            return 0;
        }
    }
    auto module = atoi(current);
    if (module != 1) {
        return 2;  // invalid module address
    }
    msg->module = module;

    next = strchr(current, ':');
    if (next == NULL) {
        return 3;  // invalid port address
    }
    current = next + 1;
    auto port = atoi(current);
    if (port < 1 || port > 15) {
        return 3;  // invalid port address
    }
    msg->port = port;

    // param(s)
    next = strchr(current, ',');
    if (next == NULL) {
        msg->param = nullptr;
    } else {
        msg->param = next + 1;
    }

    return 0;
}
