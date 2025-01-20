// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_api.h"

#include "log.h"
#include "service_mdns.h"

static const char* sources[] = {"WS", "Serial", "BT"};

static const char* msgType = "type";
static const char* msgTypeDock = "dock";
static const char* msgId = "id";
static const char* msgReqId = "req_id";
static const char* msgCommand = "command";
static const char* msgMsg = "msg";
static const char* msgCode = "code";
static const char* msgError = "error";
static const char* msgToken = "token";
static const char* msgWifiPwd = "wifi_password";

API::API(Config* config, State* state, NetworkService* networkService, InfraredService* irService,
         LedControl* ledControl)
    : m_config(config),
      m_state(state),
      m_networkService(networkService),
      m_irService(irService),
      m_ledControl(ledControl) {
    assert(m_config);
    assert(m_state);
    assert(m_networkService);
    assert(m_irService);
    assert(m_ledControl);
}

void API::init() {
    // initialize the websocket server
    m_webSocketServer.begin();
    m_webSocketServer.onEvent([=](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Log.logf(Log.DEBUG, m_ctx, "[#%u clients=%u] Disconnected", num,
                         m_webSocketServer.connectedClients(false));
                // remove from authenticated clients
                m_authWsClients.erase(num);
                break;

            case WStype_CONNECTED: {
                IPAddress ip = m_webSocketServer.remoteIP(num);
                Log.logf(Log.DEBUG, m_ctx, "[#%u clients=%u] Connected from %d.%d.%d.%d url: %s", num,
                         m_webSocketServer.connectedClients(false), ip[0], ip[1], ip[2], ip[3],
                         payload);

                // send auth request message
                StaticJsonDocument<200> responseDoc;
                responseDoc[msgType] = "auth_required";
                responseDoc["model"] = m_config->getModel();
                responseDoc["revision"] = HW_REVISION;
                responseDoc["version"] = m_config->getSoftwareVersion();
                String message;
                serializeJson(responseDoc, message);
                m_webSocketServer.sendTXT(num, message);
            } break;

            case WStype_TEXT:
                processWsRequest(reinterpret_cast<char*>(payload), num);
                break;

            case WStype_FRAGMENT_TEXT_START:
            case WStype_FRAGMENT_BIN_START:
                Log.error(m_ctx, "WebSocket fragments not supported");
                m_webSocketServer.disconnect(num);
                break;
            case WStype_BIN:
                Log.error(m_ctx, "Binary WebSocket message not supported");
                m_webSocketServer.disconnect(num);
                break;
        }
    });

    Log.logf(Log.DEBUG, m_ctx, "Initialized. Running on core: %d", xPortGetCoreID());
}

void API::loop() {
    m_webSocketServer.loop();
    handleSerial();

    auto response = m_irService->apiResponse();
    if (response) {
        Log.debug(m_ctx, "IR response available");
        // check if response is for a specific client (send IR response), or a learning broadcast
        if (response->clientId >= 0) {
            m_webSocketServer.sendTXT(response->clientId, response->message);
        } else {
            m_webSocketServer.broadcastTXT(response->message);
        }
        delete response;
    }
}

void API::handleSerial() {
    if (!Serial.available()) {
        return;
    }

    char buffer[1024];
    auto count = Serial.readBytes(buffer, sizeof(buffer) - 1);
    if (count > 0) {
        buffer[count] = '0';  // readBytes doesn't zero-terminate the buffer!
        processRequest(buffer, Source::Uart, [this](String response) -> void {
            if (!response.isEmpty()) {
                Log.debug(m_ctx, response);
            }
        });
    }
}

void API::processWsRequest(char* request, int id) {
    auto search = m_authWsClients.find(id);
    bool authenticated = search != m_authWsClients.end();

    auto cb = [this, id](String response) -> void {
        if (!response.isEmpty()) {
            m_webSocketServer.sendTXT(id, response);
        }
    };
    processRequest(request, Source::WebSocket, cb, authenticated, id);
}

bool API::processRequest(char* request, Source source, ApiResponseCallbackFunction cb, bool authenticated, int id) {
    // filter garbage data. First char must be a printable character
    if (request == NULL || !(request[0] >= 32 && request[0] <= 127)) {
        return false;
    }

    // From the docs: "Use StaticJsonDocument for small documents (below 1KB) and switch to a DynamicJsonDocument if
    // it’s too large to fit in the stack memory."
    // Since we use a writeable buffer, ArduinoJson uses zero-copy! Probably 200 is still way too large :-)
    StaticJsonDocument<200> webSocketJsonDocument;
    DeserializationError    error = deserializeJson(webSocketJsonDocument, request);

    if (error) {
        Log.logf(
            Log.WARN, m_ctx, "Error deserializing JSON: %s. %s", error.c_str(),
            error.code() == DeserializationError::IncompleteInput || error.code() == DeserializationError::InvalidInput
                ? request
                : "");
        cb("{\"code\": 500}");
        return false;
    }

    // response json (sysinfo msg is largest with > 300 chars depending on friendly name)
    StaticJsonDocument<400> responseDoc;
    String                  type;
    if (webSocketJsonDocument.containsKey(msgType)) {
        type = webSocketJsonDocument[msgType].as<String>();
    }

    String command;
    if (webSocketJsonDocument.containsKey(msgCommand)) {
        command = webSocketJsonDocument[msgCommand].as<String>();
    }

    // log received data, but filter sensitive information
    if (Log.getFilterLevel() == UCLog::Level::DEBUG) {
        auto filtered = DynamicJsonDocument(webSocketJsonDocument);
        if (webSocketJsonDocument.containsKey(msgWifiPwd)) {
            filtered[msgWifiPwd] = "****";
        }
        if (webSocketJsonDocument.containsKey(msgToken)) {
            filtered[msgToken] = "****";
        }
        String jsonStr;
        serializeJson(filtered, jsonStr);
        Log.logf(Log.DEBUG, m_ctx, "%s request: %s", sources[source], jsonStr.c_str());
    }

    // AUTHENTICATION TO THE API
    if (type == "auth") {
        String message;
        responseDoc[msgType] = "authentication";
        if (webSocketJsonDocument.containsKey(msgId)) {
            responseDoc[msgReqId] = webSocketJsonDocument[msgId].as<int>();
        }
        if (webSocketJsonDocument[msgToken].as<String>() == m_config->getToken()) {
            // token ok
            responseDoc[msgCode] = 200;

            if (source == WebSocket) {
                // add client to authorized clients
                m_authWsClients.insert(id);
            }

            serializeJson(responseDoc, message);
            cb(message);
            return true;
        } else {
            // invalid token: disconnect
            responseDoc[msgCode] = 401;
            responseDoc[msgError] = "Invalid token";
            serializeJson(responseDoc, message);
            cb(message);
            delay(100);
            if (source == WebSocket) {
                m_webSocketServer.disconnect(id);
            }
            return false;
        }
    }

    if (!type.isEmpty()) {
        responseDoc[msgType] = type;
    }
    if (!command.isEmpty()) {
        responseDoc[msgMsg] = command;
    }
    if (webSocketJsonDocument.containsKey(msgId)) {
        responseDoc[msgReqId] = webSocketJsonDocument[msgId].as<int>();
    }
    // default response code
    responseDoc[msgCode] = 200;

    // Allowed non-authorized commands to the dock
    if (type == msgTypeDock) {
        // Get system information
        if (command == "get_sysinfo") {
            responseDoc["name"] = m_config->getFriendlyName();
            responseDoc["hostname"] = m_config->getHostName();
            responseDoc["model"] = m_config->getModel();
            responseDoc["revision"] = m_config->getRevision();
            responseDoc["version"] = m_config->getSoftwareVersion();
            responseDoc["serial"] = m_config->getSerial();
            responseDoc["led_brightness"] = m_config->getLedBrightness();
#if defined(ETH_STATUS_LED)
            responseDoc["eth_led_brightness"] = m_config->getEthLedBrightness();
#endif
            responseDoc["ir_learning"] = m_irService->isIrLearning();
            responseDoc["ethernet"] = m_networkService->isEthConnected();
            responseDoc["wifi"] = m_networkService->isWifiEnabled();
            responseDoc["ssid"] = m_config->getWifiSsid();
            responseDoc["uptime"] = String(m_state->getUptime());
            responseDoc["sntp"] = m_config->isNtpEnabled();

            String message;
            serializeJson(responseDoc, message);
            cb(message);
            return true;
        }
    }

    // Authorized COMMANDS TO THE DOCK
    if (!authenticated) {
        Log.info(m_ctx, "Cannot execute command: WS connection not authorized");
        responseDoc[msgCode] = 401;
        String message;
        serializeJson(responseDoc, message);
        cb(message);
        return false;
    }

    if (type != msgTypeDock) {
        Log.info(m_ctx, "Ignoring message with invalid type field");
        responseDoc[msgCode] = 400;
    } else if (command.isEmpty() && webSocketJsonDocument[msgMsg].as<String>() == "ping") {
        Log.debug(m_ctx, "Sending heartbeat");
        responseDoc.remove(msgCode);
        responseDoc[msgMsg] = "pong";
    } else if (command == "set_config") {
        bool field = false;
        bool ok = false;

        if (webSocketJsonDocument.containsKey("friendly_name")) {
            field = true;
            String dockFriendlyName = webSocketJsonDocument["friendly_name"].as<String>();
            m_config->setFriendlyName(dockFriendlyName);
            // retrieve from config again, since it could be adjusted
            MdnsService.addFriendlyName(m_config->getFriendlyName());
            ok = true;
        }
        if (webSocketJsonDocument.containsKey(msgToken)) {
            field = true;
            String token = webSocketJsonDocument[msgToken].as<String>();
            if (token.isEmpty() || token.length() > 40) {
                responseDoc[msgError] = "Token length must be 4..40";
            } else {
                ok = m_config->setToken(token);
            }
        }
        if (!(field && !ok) &&
            (webSocketJsonDocument.containsKey("ssid") || webSocketJsonDocument.containsKey(msgWifiPwd))) {
            String ssid = webSocketJsonDocument["ssid"].as<String>();
            String pass = webSocketJsonDocument[msgWifiPwd].as<String>();

            if (m_config->setWifi(ssid, pass)) {
                Log.logf(Log.DEBUG, m_ctx, "Saving SSID: %s", ssid.c_str());

                String message;
                responseDoc["reboot"] = true;
                serializeJson(responseDoc, message);
                cb(message);
                delay(200);
                if (source == Source::WebSocket) {
                    m_webSocketServer.disconnect(id);
                }

                m_state->reboot();
                // Log.debug(m_ctx, "Disconnecting any current WiFi connections.");
                // WiFi.disconnect();
                // delay(1000);
                // Log.debug(m_ctx, "Connecting to provided WiFi credentials.");
                // WifiService::getInstance()->connect(ssid, pass);
                return true;
            } else {
                responseDoc[msgError] = "Invalid SSID or password";
            }
        }

        if (!ok) {
            responseDoc[msgCode] = 400;
        }
    } else if (command == "set_brightness") {
        bool ok = false;
        if (webSocketJsonDocument.containsKey("status_led")) {
            m_state->setState(States::LED_SETUP);
            int brightness = webSocketJsonDocument["status_led"].as<int>();
            Log.logf(Log.DEBUG, m_ctx, "Set LED brightness: %d", brightness);
            // set new value
            m_ledControl->setLedMaxBrightness(brightness);
            // persist value
            m_config->setLedBrightness(brightness);
            ok = true;
        }
        if (webSocketJsonDocument.containsKey("eth_led")) {
            int brightness = webSocketJsonDocument["eth_led"].as<int>();
            Log.logf(Log.DEBUG, m_ctx, "Set ETH brightness: %d", brightness);
            // set new value if ethernet link is up
            if (m_networkService->isEthLinkUp()) {
                m_ledControl->setEthLedBrightness(brightness);
            }
            // persist value
            m_config->setEthLedBrightness(brightness);
            ok = true;
        }
        if (!ok) {
            responseDoc[msgCode] = 400;
        }
    } else if (command == "test_mode") {
        bool ok = m_config->setTestMode(true);

        if (!ok) {
            responseDoc[msgCode] = 400;
        }
    } else if (command == "rgb_test") {
        if (m_state->getState() != States::TEST_LED_RED && m_state->getState() != States::TEST_LED_GREEN &&
            m_state->getState() != States::TEST_LED_BLUE) {
            m_prevState = m_state->getState();
        }

        Log.debug(m_ctx, "Led test start");
        // set led to red
        if (webSocketJsonDocument["color"].as<String>() == "red") {
            m_state->setState(States::TEST_LED_RED);
        }

        // set led to green
        if (webSocketJsonDocument["color"].as<String>() == "green") {
            m_state->setState(States::TEST_LED_GREEN);
        }

        // set led to blue
        if (webSocketJsonDocument["color"].as<String>() == "blue") {
            m_state->setState(States::TEST_LED_BLUE);
        }

    } else if (command == "rgb_test_stop") {
        m_state->setState(m_prevState);
        Log.debug(m_ctx, "Led test stop");
    } else if (command == "ir_test") {
        Log.debug(m_ctx, "IR Led test start");
        digitalWrite(IR_SEND_PIN_INT_SIDE, HIGH);
#ifdef IR_SEND_PIN_INT_TOP
        digitalWrite(IR_SEND_PIN_INT_TOP, HIGH);
#endif
        digitalWrite(IR_SEND_PIN_EXT_1, HIGH);
#ifdef IR_SEND_PIN_EXT_2
        digitalWrite(IR_SEND_PIN_EXT_2, HIGH);
#endif
        delay(2500);
        digitalWrite(IR_SEND_PIN_INT_SIDE, LOW);
#ifdef IR_SEND_PIN_INT_TOP
        digitalWrite(IR_SEND_PIN_INT_TOP, LOW);
#endif
        digitalWrite(IR_SEND_PIN_EXT_1, LOW);
#ifdef IR_SEND_PIN_EXT_2
        digitalWrite(IR_SEND_PIN_EXT_2, LOW);
#endif
        Log.debug(m_ctx, "IR Led test ended");
    } else if (command == "ir_send") {
        Log.debug(m_ctx, "IR Send");

        String   code = webSocketJsonDocument["code"].as<String>();
        String   format = webSocketJsonDocument["format"].as<String>();
        bool     success = false;
        uint16_t response = 400;

        if (!code.isEmpty() && !format.isEmpty()) {
            uint16_t repeat = webSocketJsonDocument["repeat"].as<uint16_t>();
            bool     intSide = webSocketJsonDocument["int_side"].as<bool>();
            bool     intTop = webSocketJsonDocument["int_top"].as<bool>();
            bool     ext1 = webSocketJsonDocument["ext1"].as<bool>();
            bool     ext2 = webSocketJsonDocument["ext2"].as<bool>();

            // default outputs if not specified
            if (!(intSide || intTop || ext1 || ext2)) {
                intSide = ext1 = ext2 = true;
            }

            int reqId = webSocketJsonDocument[msgId].as<int>();
            response = m_irService->send(id, reqId, code, format, repeat, intSide, intTop, ext1, ext2);
            if (response == 0) {
                // asynchronous reply
                return true;
            }
        }
        responseDoc[msgCode] = response;
    } else if (command == "ir_stop") {
        m_irService->stopSend();
        responseDoc[msgCode] = 200;
    } else if (command == "ir_receive_on") {
        m_irService->startIrLearn();
        Log.debug(m_ctx, "IR Receive on");
    } else if (command == "ir_receive_off") {
        m_irService->stopIrLearn();
        Log.debug(m_ctx, "IR Receive off");
    } else if (command == "remote_charged") {
        m_state->setState(States::NORMAL_FULLYCHARGED);
    } else if (command == "remote_lowbattery") {
        m_state->setState(States::NORMAL_LOWBATTERY);
    } else if (command == "remote_normal") {
        m_state->setState(States::NORMAL);
    } else if (command == "identify") {
        m_state->setState(States::IDENTIFY);
    } else if (command == "set_logging") {
        bool ok = false;
        if (webSocketJsonDocument.containsKey("log_level")) {
            uint16_t level = webSocketJsonDocument["log_level"].as<uint16_t>();
            if (level >= 0 && level <= 7) {
                auto logLevel = static_cast<UCLog::Level>(level);
                ok = m_config->setLogLevel(logLevel);
                Log.setFilterLevel(logLevel);
            }
        }
        if (webSocketJsonDocument.containsKey("syslog_server")) {
            String   server = webSocketJsonDocument["syslog_server"].as<String>();
            uint16_t port = webSocketJsonDocument["syslog_port"].as<uint16_t>();
            ok = m_config->setSyslogServer(server, port);
        }
        if (webSocketJsonDocument.containsKey("syslog_enabled")) {
            bool syslog = webSocketJsonDocument["syslog_enabled"].as<bool>();
            m_config->enableSyslog(syslog);
            if (syslog) {
                Log.enableSyslog(m_config->getHostName(), m_config->getSyslogServer(), m_config->getSyslogServerPort());
            } else {
                Log.enableSyslog(false);
            }
        }
        responseDoc[msgCode] = ok ? 200 : 400;
    } else if (command == "set_sntp") {
        bool ok = true;
        if (webSocketJsonDocument.containsKey("sntp_server1") || webSocketJsonDocument.containsKey("sntp_server2")) {
            String server1 = webSocketJsonDocument["sntp_server1"].as<String>();
            String server2 = webSocketJsonDocument["sntp_server2"].as<String>();
            if (!m_config->setNtpServer(server1, server2)) {
                ok = false;
            }
        }
        if (webSocketJsonDocument.containsKey("sntp_enabled")) {
            bool enabled = webSocketJsonDocument["sntp_enabled"].as<bool>();
            if (!m_config->enableNtp(enabled)) {
                ok = false;
            }
        }
        responseDoc[msgCode] = ok ? 200 : 400;
    } else if (command == "reboot") {
        Log.warn(m_ctx, "Rebooting");
        String message;
        responseDoc["reboot"] = true;
        serializeJson(responseDoc, message);
        cb(message);
        delay(200);
        if (source == Source::WebSocket) {
            m_webSocketServer.disconnect(id);
        }
        m_state->reboot();
        return true;
    } else if (command == "reset") {
        Log.warn(m_ctx, "Reset");
        String message;
        responseDoc["reboot"] = true;
        serializeJson(responseDoc, message);
        cb(message);
        delay(200);
        if (source == Source::WebSocket) {
            m_webSocketServer.disconnect(id);
        }
        m_config->reset();
        return true;
    } else if (command == "set_ir_config") {
        bool ok = true;
        if (webSocketJsonDocument.containsKey("irlearn_core")) {
            uint16_t value = webSocketJsonDocument["irlearn_core"].as<uint16_t>();
            if (!m_config->setIrLearnCore(value)) {
                ok = false;
            }
        }
        if (webSocketJsonDocument.containsKey("irlearn_prio")) {
            uint16_t value = webSocketJsonDocument["irlearn_prio"].as<uint16_t>();
            if (!m_config->setIrLearnPriority(value)) {
                ok = false;
            }
            m_irService->setIrLearnPriority(value);
        }
        if (webSocketJsonDocument.containsKey("irsend_core")) {
            uint16_t value = webSocketJsonDocument["irsend_core"].as<uint16_t>();
            if (!m_config->setIrSendCore(value)) {
                ok = false;
            }
        }
        if (webSocketJsonDocument.containsKey("irsend_prio")) {
            uint16_t value = webSocketJsonDocument["irsend_prio"].as<uint16_t>();
            if (!m_config->setIrSendPriority(value)) {
                ok = false;
            }
            m_irService->setIrSendPriority(value);
        }
        responseDoc[msgCode] = ok ? 200 : 500;
    } else if (command == "get_ir_config") {
        responseDoc["irlearn_core"] = m_config->getIrLearnCore();
        responseDoc["irlearn_prio"] = m_config->getIrLearnPriority();
        responseDoc["irsend_core"] = m_config->getIrSendCore();
        responseDoc["irsend_prio"] = m_config->getIrSendPriority();
    } else {
        responseDoc[msgCode] = 400;
        responseDoc[msgError] = command.isEmpty() ? "Missing command field" : "Unsupported command";
    }

    String message;
    serializeJson(responseDoc, message);
    cb(message);
    return true;
}

void API::sendMessage(String msg) {
    for (auto iter = m_authWsClients.begin(); iter != m_authWsClients.end(); ++iter) {
        m_webSocketServer.sendTXT((*iter), msg);
    }
}
