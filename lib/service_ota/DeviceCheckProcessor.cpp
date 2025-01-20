// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DeviceCheckProcessor.h"

#include <UpdateProcessorRFC3161.h>

#include <algorithm>
#include <cstdio>

#include "log.h"

const size_t kMaxHeaderLength = 128;

DeviceCheckProcessor::DeviceCheckProcessor(const char *model, const char *hwRev, UpdateProcessor *chain)
    : m_model(model), m_hwRev(hwRev), m_next(chain), m_headerProcessed(false) {
    assert(m_next);
}

DeviceCheckProcessor::~DeviceCheckProcessor() {
    delete m_next;
}

UpdateProcessor::secure_update_processor_err_t DeviceCheckProcessor::process_header(uint32_t *command, uint8_t *buffer,
                                                                                    size_t *len) {
    if (!m_headerProcessed) {
        if (*len < kMaxHeaderLength) {
            return secure_update_processor_AGAIN;
        }

        if ((buffer[0] != REDWAX_MAGIC_HEADER[0])) {
            Log.error(m_ctx, "Invalid header in firmware image");
            return secure_update_processor_ERROR;
        }

        char *p = reinterpret_cast<char *>(memchr(buffer, '\n', *len));

        if (p == NULL) {
            Log.error(m_ctx, "No EOL found in header");
            return secure_update_processor_ERROR;
        }

        *p = '\0';
        char *q = index(reinterpret_cast<char *>(buffer), ' ');
        if (q == NULL) {
            Log.error(m_ctx, "No header key values found");

            return secure_update_processor_ERROR;
        }

        Log.logf(Log.DEBUG, m_ctx, "Header: %s", q);

        String model = getValueFromHeader(q, "model");
        String hwRev = getValueFromHeader(q, "hw");

        if (model.isEmpty() || hwRev.isEmpty()) {
            Log.error(m_ctx, "Model number or hw revision not found in header");
            return secure_update_processor_ERROR;
        }

        if (model != m_model || hwRev != m_hwRev) {
            Log.logf(Log.ERROR, m_ctx, "Invalid firmware image (model '%s' / hw revision '%s'). Required: %s / %s",
                     model.c_str(), hwRev.c_str(), m_model, m_hwRev);
            return secure_update_processor_ERROR;
        }

        // restore for next processor
        *p = '\n';

        m_headerProcessed = true;
    }

    return m_next->process_header(command, buffer, len);
}

String DeviceCheckProcessor::getValueFromHeader(const char *header, const char *key, int max_len) {
    assert(key);

    if (header == NULL) {
        return String();
    }

    char value[kMaxHeaderLength] = {0};
    char search[kMaxHeaderLength];

    snprintf(search, strlen(key) + 2, "%s=", key);
    char *v = strstr(header, search);
    if (v) {
        // move to start of value
        v += strlen(search);
        // find end of value
        char *s = strchr(v, ' ');
        int   len = s ? min(s - v, max_len) : max_len;
        strncpy(value, v, len);
        value[len] = 0;
    }

    return String(value);
}
