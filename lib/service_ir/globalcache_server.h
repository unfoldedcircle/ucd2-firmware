// SPDX-FileCopyrightText: Copyright (c) 2023 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "config.h"
#include "service_ir.h"
#include "state.h"

/// GlobalCache iTach device emulation
class GlobalCacheServer {
 public:
    GlobalCacheServer(State *state, InfraredService *irService, Config *config);

 private:
    static void tcp_server_task(void *pvParameters);
    static void socket_task(void *pvParameters);
    static void beacon_task(void *param);

    State           *m_state;
    InfraredService *m_irService;
    Config          *m_config;
};
