// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include "state.h"

class OtaService {
 public:
    explicit OtaService(Config *config, State *state);

    void init();
    void loop();

 private:
    void     add_http(WebServer *server, const char *path);
    uint32_t max_sketch_size();
    /// @brief Abort file upload with the given status code and text content
    /// @param server active server
    /// @param code status code
    /// @param content plain text to send in body
    void     abortUpload(WebServer *server, int code, const char *content);

    Config     *m_config;
    State      *m_state;
    const char *m_ctx = "OTA";
    u_long      m_uploadTimeout = 0;

    /// @brief Upload progress reporting chunk size
    static const size_t chunkSize = 102400;
};
