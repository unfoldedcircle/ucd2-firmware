// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "efuse.h"

#include <esp_efuse.h>
#include <soc/efuse_reg.h>

#include "log.h"

static const esp_efuse_desc_t DOCK_SERIAL[] = {
    {EFUSE_BLK3, 8, 64},
};

static const esp_efuse_desc_t DOCK_MODEL[] = {
    {EFUSE_BLK3, 72, 56},
};

static const esp_efuse_desc_t DOCK_HW_REV[] = {
    {EFUSE_BLK3, 160, 48},
};

const esp_efuse_desc_t* ESP_EFUSE_DOCK_SERIAL[] = {&DOCK_SERIAL[0], NULL};
const esp_efuse_desc_t* ESP_EFUSE_DOCK_MODEL[] = {&DOCK_MODEL[0], NULL};
const esp_efuse_desc_t* ESP_EFUSE_DOCK_HW_REV[] = {&DOCK_HW_REV[0], NULL};

typedef struct {
    char serial[9];  // always 1 byte more for zero terminator
    char model[8];
    char revision[7];
} device_desc_t;

static device_desc_t device_desc = {0};

Efuse::Efuse() {
    esp_efuse_read_field_blob(ESP_EFUSE_DOCK_SERIAL, &device_desc.serial, ESP_EFUSE_DOCK_SERIAL[0]->bit_count);
    esp_efuse_read_field_blob(ESP_EFUSE_DOCK_MODEL, &device_desc.model, ESP_EFUSE_DOCK_MODEL[0]->bit_count);
    esp_efuse_read_field_blob(ESP_EFUSE_DOCK_HW_REV, &device_desc.revision, ESP_EFUSE_DOCK_HW_REV[0]->bit_count);
    if (device_desc.serial[0] == 0) {
        strncpy(device_desc.serial, "00000000", sizeof(device_desc.serial));
    }
    Log.logf(Log.DEBUG, m_ctx, "serial: %s, model: %s, revision: %s", device_desc.serial, device_desc.model,
             device_desc.revision);
}

const char* Efuse::getSerial() const {
    return device_desc.serial;
}

const char* Efuse::getModel() const {
    return device_desc.model;
}

const char* Efuse::getHwRevision() const {
    return device_desc.revision;
}

Efuse& Efuse::getInstance() {
    static Efuse instance;
    return instance;
}
