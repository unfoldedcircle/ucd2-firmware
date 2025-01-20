// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "service_ir.h"

#include <ArduinoJson.h>
#include <IRac.h>
#include <IRtimer.h>
#include <IRutils.h>

#include <cstdio>

#include "IRrecv.h"
#include "IRremoteESP8266.h"  // https://platformio.org/lib/show/1089/IRremoteESP8266
#include "IRsend.h"
#include "ir_codes.hpp"
#include "log.h"
#include "util_types.h"

const char *irLog = "IR";
const char *irLogSend = "IRSEND";
const char *irLogLearn = "IRLEARN";

const int IR_LEARNING_BIT = BIT0;
const int IR_REPEAT_BIT = BIT1;
const int IR_REPEAT_STOP_BIT = BIT2;

// good explanation of IRrecv parameters:
// https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/IRrecvDumpV3/IRrecvDumpV3.ino
const uint16_t kCaptureBufferSize = 1024;  // 1024 == ~511 bits
// Suits most messages, while not swallowing many repeats. Not suited for AC IR remotes!
const uint8_t  kTimeout = 15;       // Milli-Seconds.
const uint16_t kFrequency = 38000;  // in Hz. e.g. 38kHz.
// Set the smallest sized "UNKNOWN" message packets we actually care about.
const uint16_t kMinUnknownSize = 12;

// we got a mess with the hpp files / project structure. Since we need a rewrite, keep on hacking 🙈
extern bool    send_string_to_socket(const int socket, const char *buf);
extern uint8_t parseGcRequest(const char *request, GCMsg *msg);

void InfraredService::init(uint16_t sendCore, uint16_t sendPriority, uint16_t learnCore, uint16_t learnPriority,
                           State *state) {
    if (m_eventgroup) {
        Log.error(irLog, "Already initialized");
        return;
    }

    m_state = state;

    m_queue = xQueueCreate(1, sizeof(struct IRSendMessage *));
    if (m_queue == nullptr) {
        Log.error(irLog, "xQueueCreate failed");
        return;
    }
    m_eventgroup = xEventGroupCreate();
    if (m_eventgroup == nullptr) {
        Log.error(irLog, "xEventGroupCreate failed");
        return;
    }
    m_apiResponseQueue = xQueueCreate(5, sizeof(struct IrResponse *));
    if (m_queue == nullptr) {
        Log.error(irLog, "API response queue creation failed");
        return;
    }

    if (sendCore > 1) {
        sendCore = 1;
    }
    if (learnCore > 1) {
        learnCore = 1;
    }

    xTaskCreatePinnedToCore(send_ir_f,     // task function
                            "IR send",     // task name
                            4000,          // stack size: random crashes with 2000!
                            this,          // task parameter
                            sendPriority,  // task priority
                            &m_ir_task,    // Task handle to keep track of created task
                            sendCore);     // core

    xTaskCreatePinnedToCore(learn_ir_f,     // task function
                            "IR learn",     // task name
                            4000,           // stack size: random crashes with 2000!
                            this,           // task parameter
                            learnPriority,  // task priority
                            &m_learn_task,  // Task handle to keep track of created task
                            learnCore);     // core

    Log.logf(Log.DEBUG, irLog, "Initialized: core=%d, priority=%d", xPortGetCoreID(), uxTaskPriorityGet(NULL));
}

void InfraredService::setIrSendPriority(uint16_t priority) {
    // there's an assert in vTaskPrioritySet!
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
    }
    if (m_ir_task) {
        vTaskPrioritySet(m_ir_task, priority);
    }
}

void InfraredService::setIrLearnPriority(uint16_t priority) {
    if (priority >= configMAX_PRIORITIES) {
        priority = configMAX_PRIORITIES - 1;
    }
    if (m_learn_task) {
        vTaskPrioritySet(m_learn_task, priority);
    }
}

void InfraredService::startIrLearn() {
    if (m_state) {
        // TODO(zehnm) rewrite using a proper state machine! This should be an event.
        m_state->setState(States::IR_LEARNING);
    }
    if (m_eventgroup) {
        xEventGroupSetBits(m_eventgroup, IR_LEARNING_BIT);
    }
}

void InfraredService::stopIrLearn() {
    if (m_state) {
        // TODO(zehnm) rewrite using a proper state machine! This should be an event.
        m_state->setState(States::NORMAL);
    }
    if (m_eventgroup) {
        xEventGroupClearBits(m_eventgroup, IR_LEARNING_BIT);
    }
}

bool InfraredService::isIrLearning() {
    if (!m_eventgroup) {
        return false;
    }
    return xEventGroupGetBits(m_eventgroup) & IR_LEARNING_BIT;
}

struct IrResponse *InfraredService::apiResponse() {
    if (m_apiResponseQueue == nullptr) {
        return nullptr;
    }

    struct IrResponse *pIrMsg;
    if (xQueueReceive(m_apiResponseQueue, &(pIrMsg), 0) == pdTRUE) {
        if (pIrMsg->clientId == IR_CLIENT_GC) {
            // should not happen
            delete pIrMsg;
            return nullptr;
        }

        return pIrMsg;
    }

    return nullptr;
}

uint16_t InfraredService::sendGlobalCache(int16_t clientId, uint32_t msgId, const char *sendir, int socket) {
    // module is always 1 (emulating an iTach device)
    if (strncmp(sendir, "sendir,1:", 9) != 0) {
        return 2;  // invalid module address
    }

    // ID
    char *next = strchr(sendir + 9, ',');
    if (next == NULL) {
        return 4;  // invalid ID
    }
    auto port = atoi(sendir + 9);
    if (port < 1 || port > 15) {
        return 3;  // invalid port address
    }

    // frequency
    next = strchr(next + 1, ',');
    if (next == NULL) {
        return 5;  // invalid frequency
    }
    // repeat
    next = strchr(next + 1, ',');
    if (next == NULL) {
        return 6;  // invalid repeat
    }

    int16_t repeat = atoi(next + 1);
    if (repeat < 1 || repeat > 50) {
        return 6;  // invalid repeat
    }

    String code = sendir;
    String format = "gc";

    return send(clientId, msgId, code, format, repeat, port & 1, port & 8, port & 2, port & 4, socket);
}

uint16_t InfraredService::send(int16_t clientId, uint32_t msgId, const String &code, const String &format,
                               uint16_t repeat, bool internal_side, bool internal_top, bool external_1,
                               bool external_2, int gcCocket) {
    if (!m_queue || !m_eventgroup) {
        return 500;
    }

    if (isIrLearning()) {
        return 503;  // service unavailable
    }

    uint32_t pin_mask = 0;
    if (internal_side) {
        pin_mask |= (1UL << IR_SEND_PIN_INT_SIDE);
    }

#ifdef IR_SEND_PIN_INT_TOP
    if (internal_top) {
        pin_mask |= (1UL << IR_SEND_PIN_INT_TOP);
    }
#endif
    if (external_1) {
        pin_mask |= (1UL << IR_SEND_PIN_EXT_1);
    }
#ifdef IR_SEND_PIN_EXT_2
    if (external_2) {
        pin_mask |= (1UL << IR_SEND_PIN_EXT_2);
    }
#endif
    if (pin_mask == 0) {
        return 400;
    }

    IRFormat irFormat;
    if (format == "hex") {
        irFormat = IRFormat::UNFOLDED_CIRCLE;
    } else if (format == "pronto") {
        irFormat = IRFormat::PRONTO;
    } else if (format == "gc") {
        irFormat = IRFormat::GLOBAL_CACHE;
    } else {
        return 400;
    }

    bool sending = uxQueueMessagesWaiting(m_queue) > 0;

    // #65 handle IR repeat if it's the same command. This is a very simple, initial implementation (ignore repeat val)
    if (sending && repeat > 0 && m_currentSendCode == code) {
        Log.logf(Log.DEBUG, irLog, "detected IR repeat for last IR send command (%d)", repeat);
        xEventGroupSetBits(m_eventgroup, IR_REPEAT_BIT);

        return 202;  // accepted IR repeat
    }

    // try to save an allocation if still sending an IR code
    if (sending) {
        return 429;  // too many requests
    }

    // new code, clear repeat flags
    xEventGroupClearBits(m_eventgroup, IR_REPEAT_BIT | IR_REPEAT_STOP_BIT);

    struct IRSendMessage *pxMessage = new IRSendMessage();
    pxMessage->clientId = clientId;
    pxMessage->msgId = msgId;
    pxMessage->format = irFormat;
    pxMessage->message = code;
    pxMessage->repeat = repeat;
    pxMessage->pin_mask = pin_mask;
    pxMessage->gcSocket = gcCocket;

    if (xQueueSendToBack(m_queue, reinterpret_cast<void *>(&pxMessage), 0) == errQUEUE_FULL) {
        // This should never happen with the pre-check!
        delete pxMessage;
        return 429;
    }

    Log.debug(irLog, "queued IRSendMessage");

    m_currentSendCode = code;

    // 0 = asynchronous reply from the the IR send task
    return 0;
}

void InfraredService::stopSend() {
    if (!m_eventgroup) {
        return;
    }
    Log.debug(irLog, "stopping IR repeat");
    xEventGroupSetBits(m_eventgroup, IR_REPEAT_STOP_BIT);
    xEventGroupClearBits(m_eventgroup, IR_REPEAT_BIT);  // shouldn't be required, better be save though
    // TODO(zehnm) what about turning off IR output? That would stop IR sending immediately!
}

void InfraredService::rebootIfMemError(int memError) {
    // Check we malloc'ed successfully.
    if (memError == 1) {  // malloc failed, so give up.
        Log.log(Log.EMERG, irLog, "FATAL: Can't allocate memory for an array for a new message! Forcing a reboot!");
        delay(2000);  // Enough time for messages to be sent.
        ESP.restart();
        delay(5000);  // Enough time to ensure we don't return.
    }
}

void InfraredService::send_ir_f(void *param) {
    if (param == nullptr) {
        Log.error(irLogSend, "BUG: missing send_ir_f param");
        return;
    }

    InfraredService *ir = reinterpret_cast<InfraredService *>(param);
    if (ir->m_queue == nullptr || ir->m_apiResponseQueue == nullptr) {
        Log.error(irLogSend, "terminated: input or output queue missing");
        return;
    }

    pinMode(IR_SEND_PIN_INT_SIDE, OUTPUT);  // sending pin
    digitalWrite(IR_SEND_PIN_INT_SIDE, 0);
#ifdef IR_SEND_PIN_INT_TOP
    pinMode(IR_SEND_PIN_INT_TOP, OUTPUT);  // sending pin
    digitalWrite(IR_SEND_PIN_INT_TOP, 0);
#endif
    pinMode(IR_SEND_PIN_EXT_1, OUTPUT);  // sending pin
    digitalWrite(IR_SEND_PIN_EXT_1, 0);
#ifdef IR_SEND_PIN_EXT_2
    pinMode(IR_SEND_PIN_EXT_2, OUTPUT);  // sending pin
    digitalWrite(IR_SEND_PIN_EXT_2, 0);
#endif

    bool modulation = true;
    // used default output to initialize, active outputs are set with `setPinMask` before calling send
    uint32_t pin_mask = 1UL << IR_SEND_PIN_INT_SIDE;
    IRsend   irsend = IRsend(modulation, pin_mask);

    irsend.begin();

    Log.logf(Log.DEBUG, irLogSend, "initialized: core=%d, priority=%d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    struct IRSendMessage *pIrMsg;
    uint16_t              repeatLimit;
    int                   repeat;
    int                   repeatCount;
    EventGroupHandle_t    eventgroup = ir->m_eventgroup;

    // reference required to persist values during callbacks (also initialization is further down!)
    auto repeatCallback = [&repeatLimit, &repeat, &repeatCount, eventgroup]() -> bool {
        // commented out log statements: depending on IR format this is very time critical!
        // Log.debug(irLogSend, "in callback!");

        // check if there's a command from the API
        auto bits = xEventGroupGetBits(eventgroup);
        if (bits & IR_REPEAT_STOP_BIT) {
            // abort immediately
            repeat = 0;
            Log.debug(irLogSend, "stopping repeat");
        } else if (bits & IR_REPEAT_BIT) {
            // reset repeat count and start counting down again
            Log.logf(Log.DEBUG, irLogSend, "continue repeat: %d -> %d", repeat, repeatLimit);
            repeat = repeatLimit;
            xEventGroupClearBits(eventgroup, IR_REPEAT_BIT);
        }
        if (repeat > 0) {
            // repeat still active: count down
            // Log.logf(Log.DEBUG, irLogSend, "repeat callback #%d, remaining repeats: %d",
            //             ++repeatCount, repeat);
            repeat--;
            return true;
        }
        return false;
    };

    // start the IR sending task
    while (true) {
        // Peek a message on the created queue.
        if (xQueuePeek(ir->m_queue, &(pIrMsg), portMAX_DELAY) == pdFALSE) {
            // timeout
            continue;
        }
        // pIrMsg now points to the struct IRSendMessage variable, but the item still remains on the queue.
        // This blocks the sender from queuing more messages and notify the client with a "busy error".

        Log.logf(Log.DEBUG, irLogSend, "new command: id=%d, format=%d, repeat=%d", pIrMsg->msgId, pIrMsg->format,
                 pIrMsg->repeat);

        // Activate continuous IR repeat
        if (pIrMsg->repeat > 0) {
            // set lambda reference variables
            repeatLimit = pIrMsg->repeat;
            repeat = pIrMsg->repeat;
            repeatCount = 0;
            irsend.setRepeatCallback(repeatCallback);
        } else {
            irsend.setRepeatCallback(nullptr);
        }

        if (irsend.setPinMask(pIrMsg->pin_mask) == 0) {
            Log.error(irLogSend, "failed to set PinMask");
        }

        bool success = false;
        switch (pIrMsg->format) {
            case IRFormat::UNFOLDED_CIRCLE: {
                IRHexData data;
                if (buildIRHexData(pIrMsg->message, &data)) {
                    // Override repeat in code
                    // Note: if only `data.repeat > 1`: some codes have to be sent twice for a single command,
                    // i.e. it's not a repeat indicator yet!
                    if (pIrMsg->repeat > 0) {
                        data.repeat = pIrMsg->repeat;
                    }
                    success = irsend.send(data.protocol, data.command, data.bits, data.repeat);
                } else {
                    Log.warn(irLogSend, "failed to parse UC code");
                }
                break;
            }
            case IRFormat::PRONTO: {
                // #60 use space as default separator
                char separator = ' ';
                if (pIrMsg->message.indexOf(separator) < 1) {
                    // fallback to old comma (dock version <= 0.6.0)
                    separator = ',';
                }

                // operate directly on the underlaying message buffer: avoid String allocations from using
                // "message.substring"!
                auto      msg = pIrMsg->message.c_str();
                uint16_t  count;
                int       memError;
                uint16_t *code_array = prontoBufferToArray(msg, separator, &count, &memError);
                if (!(code_array == NULL || count == 0)) {
                    // Attention: PRONTO codes don't have an embedded repeat count field, some codes might required
                    // to be sent twice to be recognized correctly! One could argue it's an invalid code...
                    // We ignore that here and treat every code the same in regards to the repeat field!
                    success = irsend.sendPronto(code_array, count, pIrMsg->repeat);
                    free(code_array);
                } else {
                    Log.warn(irLogSend, "failed to parse PRONTO code");
                    rebootIfMemError(memError);
                }
                break;
            }
            case IRFormat::GLOBAL_CACHE: {
                // UNDER DEVELOPMENT
                uint16_t  count;
                int       memError;
                uint16_t *code_array = globalCacheBufferToArray(pIrMsg->message.c_str(), &count, &memError);
                if (!(code_array == NULL || count == 0)) {
                    // Override repeat in code
                    if (pIrMsg->repeat > 0) {
                        code_array[1] = pIrMsg->repeat;
                    }
                    irsend.sendGC(code_array, count);
                    success = true;
                    free(code_array);
                } else {
                    Log.warn(irLogSend, "failed to parse GC code");
                    rebootIfMemError(memError);
                }
                break;
            }
            default:
                Log.error(irLogSend, "Invalid IR format");
        }

        irsend.setRepeatCallback(nullptr);

        // quick & dirty hack (TODO callback function or a dedicated queue)
        if (pIrMsg->clientId == IR_CLIENT_GC && pIrMsg->gcSocket > 0) {
            char    response[24];
            uint8_t module = 1;
            uint8_t port = 1;
            GCMsg   req;
            if (parseGcRequest(pIrMsg->message.c_str(), &req) == 0) {
                module = req.module;
                port = req.port;
            }
            snprintf(response, sizeof(response), "completeir,%d:%d,%d\r", module, port, pIrMsg->msgId);
            send_string_to_socket(pIrMsg->gcSocket, response);
        } else {
            StaticJsonDocument<100> responseDoc;
            responseDoc["type"] = "dock";
            responseDoc["msg"] = "ir_send";
            responseDoc["req_id"] = pIrMsg->msgId;
            responseDoc["code"] = success ? 200 : 400;

            struct IrResponse *response = new IrResponse();
            response->clientId = pIrMsg->clientId;
            serializeJson(responseDoc, response->message);

            Log.logf(Log.DEBUG, irLogSend, "queuing response: success=%s", success ? "true" : "false");

            if (xQueueSendToBack(ir->m_apiResponseQueue, reinterpret_cast<void *>(&response), pdMS_TO_TICKS(10)) ==
                errQUEUE_FULL) {
                Log.error(irLogSend, "Error sending ir_send response to API clients: queue full");
                delete response;
            }
        }

        // all done, release queue (reset works because of queue length 1)
        delete pIrMsg;
        xQueueReset(ir->m_queue);
    }
}

void InfraredService::learn_ir_f(void *param) {
    if (param == nullptr) {
        Log.error(irLogLearn, "BUG: missing learn_ir_f param");
        return;
    }

    InfraredService *ir = reinterpret_cast<InfraredService *>(param);
    if (ir->m_eventgroup == nullptr || ir->m_apiResponseQueue == nullptr) {
        Log.error(irLogLearn, "terminated: input or output queue missing");
        return;
    }

    // Use turn on the save buffer feature for more complete capture coverage.
    IRrecv irrecv = IRrecv(IR_RECEIVE_PIN, kCaptureBufferSize, kTimeout, true);

    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(kMinUnknownSize);

    Log.logf(Log.DEBUG, irLogLearn, "initialized: core=%d, priority=%d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    EventBits_t    bits;
    decode_results results;
    // start the IR learning task
    while (true) {
        // wait until learning is requested
        bits = xEventGroupWaitBits(ir->m_eventgroup, IR_LEARNING_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if ((bits & portMAX_DELAY) == 0) {
            // timeout
            continue;
        }

        Log.debug(irLogLearn, "ir_learn task starting");

        // enable IR learning
        irrecv.enableIRIn();
        // #62 Clear buffers to make sure no old data is returned to client
        // Note: I'm not 100% sure if this is really required, but shouldn't hurt either :-)
        //       I couldn't find where the 2nd `params_save` buffer is cleared in enableIRIn().
        irrecv.decode(&results);

        while (xEventGroupGetBits(ir->m_eventgroup) & IR_LEARNING_BIT) {
            // start learning loop
            vTaskDelay(pdMS_TO_TICKS(100));

            if (!irrecv.decode(&results)) {
                continue;
            }

            bool failed = false;
            // #30 make sure to only report successfully decoded IR codes
            if (results.overflow) {
                Log.logf(Log.WARN, irLogLearn, "IR code is too big for buffer (>= %d)", kCaptureBufferSize);
                failed = true;
            } else if (results.decode_type == decode_type_t::UNKNOWN) {
                Log.info(irLogLearn, "Learning failed: unknown code");
                failed = true;
            } else if (results.value == 0 || results.value == UINT64_MAX) {
                Log.info(irLogLearn, "Learning failed: invalid value");
                failed = true;
            }

            if (failed) {
                if (ir->m_state) {
                    // TODO(zehnm) rewrite using a proper state machine! This should be an event.
                    ir->m_state->setState(States::IR_LEARN_FAILED);
                }
                continue;
            }

            if (ir->m_state) {
                ir->m_state->setState(States::IR_LEARN_OK);
            }

            String code;
            code += results.decode_type;
            code += ";";
            code += resultToHexidecimal(&results);
            code += ";";
            code += results.bits;
            code += ";";
            // TODO(zehnm) adjust repeat count for known protocols, e.g. set Sony to 2?
            code += results.repeat;

            // code += ";";
            // code += results.address;
            // code += ";";
            // code += results.command;

            Log.logf(Log.DEBUG, irLogLearn, "Learned: %s", code.c_str());

            StaticJsonDocument<500> responseDoc;
            responseDoc["type"] = "event";
            responseDoc["msg"] = "ir_receive";
            responseDoc["ir_code"] = code;

            struct IrResponse *response = new IrResponse();
            response->clientId = -1;  // broadcast
            serializeJson(responseDoc, response->message);

            if (xQueueSendToBack(ir->m_apiResponseQueue, reinterpret_cast<void *>(&response), pdMS_TO_TICKS(10)) ==
                errQUEUE_FULL) {
                Log.error(irLogLearn, "Error sending learned IR code to API clients: queue full");
                delete response;
            } else {
                Log.logf(Log.INFO, irLogLearn, "Sending message to API clients: %s", response->message.c_str());
            }
        }

        Log.debug("irLogLearn", "ir_learn task stopping");

        // learning turned off: disable processing
        irrecv.disableIRIn();
    }
}

InfraredService &InfraredService::getInstance() {
    static InfraredService instance;
    return instance;
}

InfraredService &irService = irService.getInstance();
