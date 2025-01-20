// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ESPmDNS.h>

class MDNSService {
 public:
    static MDNSService &getInstance();

    void loop();
    void addFriendlyName(String name);

 private:
    MDNSService() = default;

    MDNSService(const MDNSService &) = delete;  // no copying
    MDNSService &operator=(const MDNSService &) = delete;

    const String m_serviceName = "_uc-dock";
    const String m_proto = "_tcp";

    const char *m_ctx = "MDNS";
};

extern MDNSService &MdnsService;
