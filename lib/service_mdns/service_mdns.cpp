// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_mdns.h"

#include <config.h>

#include "board.h"
#include "log.h"

void MDNSService::loop() {
    const u_long  fiveMinutes = 1 * 60 * 1000UL;
    static u_long lastSampleTime = 0 - fiveMinutes;

    u_long now = millis();
    if (now - lastSampleTime >= fiveMinutes) {
        lastSampleTime += fiveMinutes;
        MDNS.end();
        delay(100);

        Config *config = Config::getInstance();

        if (!MDNS.begin(config->getHostName().c_str())) {
            Log.error(m_ctx, "Error setting up MDNS responder!");
            return;
        }

        // Add mDNS service
        MDNS.addService(m_serviceName, m_proto, config->API_port);
        addFriendlyName(config->getFriendlyName());
        MDNS.addServiceTxt(m_serviceName, m_proto, "ver", config->getSoftwareVersion());
        MDNS.addServiceTxt(m_serviceName, m_proto, "model", config->getModel());
        MDNS.addServiceTxt(m_serviceName, m_proto, "rev", hwRevision);
        Log.debug(m_ctx, "Services updated");
    }
}

void MDNSService::addFriendlyName(String name) {
    MDNS.addServiceTxt(m_serviceName, m_proto, "name", name);
}

MDNSService &MDNSService::getInstance() {
    static MDNSService instance;
    return instance;
}

MDNSService &MdnsService = MdnsService.getInstance();
