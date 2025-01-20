// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <UpdateProcessor.h>

/**
 * UpdateProcessor for verifying device model & hardware revision from the
 * upload image's header.
 *
 * The model & hw revision are stored as key / value pairs after the RedWax
 * header marker within the first 128 bytes.
 */
class DeviceCheckProcessor : public UpdateProcessor {
 public:
    DeviceCheckProcessor(const char *model, const char *hwRev, UpdateProcessor *chain = NULL);
    ~DeviceCheckProcessor();

    void reset() {}

    secure_update_processor_err_t process_header(uint32_t *command, uint8_t *buffer, size_t *len);
    secure_update_processor_err_t process_payload(uint8_t *buff, size_t *len) { return secure_update_processor_OK; }
    secure_update_processor_err_t process_end() { return secure_update_processor_OK; }

 private:
    String getValueFromHeader(const char *header, const char *key, int max_len = 64);

    bool        m_headerProcessed;
    const char *m_ctx = "OTA";

    const char      *m_model;
    const char      *m_hwRev;
    UpdateProcessor *m_next;
};
