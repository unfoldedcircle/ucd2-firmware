// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_ota.h"

#include <esp_system.h>
#if ESP_IDF_VERSION_MAJOR >= 4
#include <esp32/rom/rtc.h>
#else
#include <rom/rtc.h>
#endif

#include <UpdateEx.h>
#include <UpdateProcessor.h>
#include <UpdateProcessorRFC3161.h>
#include <WebServer.h>

#include <cstdio>

#include "DeviceCheckProcessor.h"
#include "board.h"
#include "log.h"
#include "service_api.h"

WebServer OTAServer(Config::OTA_port);

// Embedded file:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#embedding-binary-data
// PEM-encoded public key chain
extern const uint8_t ota_public_key_chain_start[] asm("_binary_ota_public_key_pem_start");
extern const uint8_t ota_public_key_chain_end[] asm("_binary_ota_public_key_pem_end");

extern const uint8_t status_header[] asm("_binary_resources_status_header_html_start");
extern const uint8_t status_footer[] asm("_binary_resources_status_footer_html_start");

const char *getResetReason() {
    // see https://github.com/espressif/esp-idf/blob/master/components/esp_rom/include/esp32/rom/rtc.h
    switch (rtc_get_reset_reason(0)) {
        case POWERON_RESET:
            return "Power on reset";
        case SW_RESET:
            return "Software reset digital core";
        case OWDT_RESET:
            return "Legacy watch dog reset digital core";
        case DEEPSLEEP_RESET:
            return "Deep sleep reset digital core";
        case SDIO_RESET:
            return "Reset by SLC module, reset digital core";
        case TG0WDT_SYS_RESET:
            return "Timer group0 watch dog reset digital core";
        case TG1WDT_SYS_RESET:
            return "Timer group1 watch dog reset digital core";
        case RTCWDT_SYS_RESET:
            return "RTC watch dog reset digital core";
        case INTRUSION_RESET:
            return "Instrusion tested to reset CPU";
        case TGWDT_CPU_RESET:
            return "Timer group reset CPU";
        case SW_CPU_RESET:
            return "Software reset CPU";
        case RTCWDT_CPU_RESET:
            return "RTC watch dog reset CPU";
        case EXT_CPU_RESET:
            return "External CPU reset";
        case RTCWDT_BROWN_OUT_RESET:
            return "Voltage unstable reset";
        case RTCWDT_RTC_RESET:
            return "RTC watch dog reset digital core and RTC module";
        default:
            return "Unknown reset reason";
    }
}

static const char *UPDATE_FAIL = "Update: fail";
static const char *TEXT_PLAIN = "text/plain";

OtaService::OtaService(Config *config, State *state) : m_config(config), m_state(state) {
    assert(m_config);
    assert(m_state);
}

void OtaService::init() {
    auto rfcChecker = new UpdateProcessorRFC3161();

    // Specify a (root) signature we trust during updates.
    int ret =
        rfcChecker->addTrustedCerts(ota_public_key_chain_start, ota_public_key_chain_end - ota_public_key_chain_start);
    if (ret) {
        Log.logf(Log.CRIT, m_ctx, "Failed to initialize OTA certificates: %d", ret);
        return;
    }

    UpdateEx.setProcessor(new DeviceCheckProcessor(m_config->getModel(), m_config->getRevision(), rfcChecker));

    add_http(&OTAServer, "/update");

    Log.debug(m_ctx, "Initialized");
}

void OtaService::loop() {
    OTAServer.handleClient();

    if (m_uploadTimeout != 0 && millis() > m_uploadTimeout) {
        Log.error(m_ctx, "File upload timeout due to no data received: Rebooting");
        OTAServer.stop();
        delay(500);
        ESP.restart();
        return;
    }
}

uint32_t OtaService::max_sketch_size() {
    return (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
}

const char *getTableLine(char *buf, size_t len, const char *name, const char *value) {
    snprintf(buf, len, "<li><div class=\"title\">%s</div><div class=\"content\">%s</div></li>", name, value);
    return buf;
}

void OtaService::add_http(WebServer *server, const char *path) {
    server->on("/", HTTP_GET, [server, this]() {
        char buffer[200];
        int  len = sizeof(buffer);

        String status = String((const char *)status_header);
        status += getTableLine(buffer, len, "Name", m_config->getFriendlyName().c_str());
        status += getTableLine(buffer, len, "Hostname", m_config->getHostName().c_str());
        status += getTableLine(buffer, len, "Version", m_config->getSoftwareVersion().c_str());
        status += getTableLine(buffer, len, "Serial", m_config->getSerial());
        status += getTableLine(buffer, len, "Model", m_config->getModel());
        status += getTableLine(buffer, len, "Revision", m_config->getRevision());
        char buf[1 + 8 * sizeof(uint32_t)];
        utoa(ESP.getFreeHeap(), buf, 10);
        status += getTableLine(buffer, len, "Free heap", buf);
        status += getTableLine(buffer, len, "Uptime", m_state->getUptime().c_str());
        status += getTableLine(buffer, len, "Reset reason", getResetReason());
        if (m_config->isNtpEnabled()) {
            time_t    now;
            char      strftime_buf[64];
            struct tm timeinfo;

            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
            status += getTableLine(buffer, len, "Time", strftime_buf);
        }
        status += (const char *)status_footer;
        server->send(200, "text/html", status);
    });

    server->on(
        path, HTTP_POST,
        [server, this]() {
            server->send(UpdateEx.hasError() ? 400 : 200, TEXT_PLAIN,
                         (UpdateEx.hasError()) ? UPDATE_FAIL : "Update: OK!");
            delay(500);
            ESP.restart();
        },
        [server, this]() {
            HTTPUpload &upload = server->upload();
            // Store the next milestone to output
            static size_t next = chunkSize;

            switch (upload.status) {
              case UPLOAD_FILE_START: {
                auto contentLength = server->clientContentLength();
                Log.logf(Log.NOTICE, m_ctx, "Firmware update initiated: %s (%u)", upload.filename.c_str(),
                         contentLength);

                if (contentLength <= 0) {
                    abortUpload(server, 411, UPDATE_FAIL);
                    return;
                }

                uint32_t maxSketchSpace = this->max_sketch_size();

                if (contentLength > maxSketchSpace) {
                    Log.error(m_ctx, "Firmware file too big");
                    abortUpload(server, 413, "Update: fail\r\nError: firmware file too big");
                    return;
                }
                if (!UpdateEx.begin(maxSketchSpace)) {
                    Log.error(m_ctx, UpdateEx.hasError() ? UpdateEx.errorString() : "Could not start update");
                    abortUpload(server, 400, UPDATE_FAIL);
                    return;
                }

                // preconditions ok, start OTA
                next = chunkSize;
                m_state->setState(States::OTA);
                m_uploadTimeout = millis() + HTTP_MAX_POST_WAIT + 2000;
                break;
              }
              case UPLOAD_FILE_WRITE: {
                if (UpdateEx.hasError()) {
                    return;
                }
                // flashing firmware to ESP
                if (UpdateEx.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Log.error(m_ctx, UpdateEx.hasError() ? UpdateEx.errorString() : "Write error");
                    int code = 500;
                    switch (UpdateEx.getError()) {
                        case UPDATE_ERROR_SIZE:
                            code = 413;
                            break;
                        case UPDATE_ERROR_CHECKSUM:
                        case UPDATE_ERROR_MAGIC_BYTE:
                        case UPDATE_ERROR_BAD_ARGUMENT:
                            code = 400;
                            break;
                        default:
                            break;
                    }
                    abortUpload(server, code, UPDATE_FAIL);
                    delay(500);
                    ESP.restart();
                    return;
                }

                // Check if we need to output a milestone (100k 200k 300k)
                if (upload.totalSize >= next) {
                    Log.logf(Log.DEBUG, m_ctx, "%u KB", next / 1024);
                    next += chunkSize;
                }
                m_uploadTimeout = millis() + HTTP_MAX_POST_WAIT + 2000;
                break;
              }
              case UPLOAD_FILE_END: {
                m_uploadTimeout = 0;
                if (UpdateEx.end(true)) {  // true to set the size to the current progress
                    if (!UpdateEx.activate()) {
                        Log.error(m_ctx, "Failed to activate new firmware version");
                    } else {
                        Log.logf(Log.NOTICE, m_ctx, "Firmware update successful: %u bytes. Rebooting...",
                                 upload.totalSize);
                    }
                } else {
                    Log.error(m_ctx, UpdateEx.hasError() ? UpdateEx.errorString() : "Upload not finished");
                    if (!UpdateEx.hasError()) {
                        UpdateEx.abort();
                    }
                }
                break;
              }
              case UPLOAD_FILE_ABORTED: {
                Log.error(m_ctx, "File upload was aborted: Rebooting");
                abortUpload(server, 408, UPDATE_FAIL);
                delay(500);
                ESP.restart();
                return;
              }
              default:
                Log.logf(Log.ERROR, m_ctx, "Unknown file upload status: %d", upload.status);
                m_uploadTimeout = 0;
            }
        });

    server->begin();
}

void OtaService::abortUpload(WebServer *server, int code, const char *content) {
    if (!UpdateEx.hasError()) {
        UpdateEx.abort();
    }
    server->send(code, TEXT_PLAIN, content);
}
