// SPDX-FileCopyrightText: Copyright (c) 2023 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Common utility functions for IR codes like conversions.
// Make sure this file also compiles natively and all functions are covered by unit tests.

#pragma once

#include <Arduino.h>

enum class IRFormat {
    UNKNOWN = 0,
    UNFOLDED_CIRCLE = 1,
    PRONTO = 2,
    GLOBAL_CACHE = 3,
};

struct IRSendMessage {
    int16_t  clientId;
    uint32_t msgId;
    IRFormat format;
    String   message;
    uint16_t repeat;
    uint32_t pin_mask;
    // TCP socket of message if received from the GlobalCache server, 0 otherwise.
    int gcSocket;
};

struct IRHexData {
    decode_type_t protocol;
    uint64_t      command;
    uint16_t      bits;
    uint16_t      repeat;
};

u_long parseULong(const char *number, int *error = NULL, int base = 10) {
    if (number == NULL) {
        if (error != NULL) {
            *error = 1;
        }
        return 0;
    }
    char  *end;
    u_long value = strtoul(number, &end, base);
    if (end == number || *end != '\0' || errno == ERANGE) {
        if (error != NULL) {
            *error = 1;
        }
        return 0;
    }

    *error = 0;
    return value;
}

bool buildIRHexData(const String &message, IRHexData *data) {
    // Format is: "<protocol>;<hex-ir-code>;<bits>;<repeat-count>" e.g. "4;0x640C;15;0"
    const int firstIndex = message.indexOf(';');
    const int secondIndex = message.indexOf(';', firstIndex + 1);
    const int thirdIndex = message.indexOf(';', secondIndex + 1);

    if (firstIndex == -1 || secondIndex == -1 || thirdIndex == -1) {
        return false;
    }

    data->protocol = static_cast<decode_type_t>(message.substring(0, firstIndex).toInt());
    if (data->protocol == 0) {
        return false;
    }

    String commandStr = message.substring(firstIndex + 1, secondIndex).c_str();

    const char *str = commandStr.c_str();
    char       *end;
    errno = 0;
    uint64_t command = strtoull(str, &end, 16);
    if (command == 0 && end == str) {
        // str was not a number
        return false;
    } else if (command == UINT64_MAX && errno) {
        // the value of str does not fit in unsigned long long
        return false;
    } else if (*end) {
        // str began with a number but has junk left over at the end
        return false;
    }
    data->command = command;

    int    error;
    String number = message.substring(secondIndex + 1, thirdIndex);
    u_long value = parseULong(number.c_str(), &error, 10);
    if (error || value > 0xFFFF) {
        return false;
    }
    data->bits = value;
    if (data->bits == 0) {
        return false;
    }

    number = message.substring(thirdIndex + 1);
    value = parseULong(number.c_str(), &error, 10);
    if (error || value > 0xFFFF || value > 20) {
        return false;
    }
    data->repeat = value;

    return true;
}

uint16_t countValuesInCStr(const char *str, char sep) {
    if (str == NULL || *str == 0) {
        return 0;
    }

    int16_t  index = 0;
    uint16_t count = 0;

    while (str[index] != 0) {
        if (str[index] == sep) {
            count++;
        }
        index++;
    }

    return count + 1;  // for value after last separator
}

uint16_t *prontoBufferToArray(const char *msg, char separator, uint16_t *codeCount, int *memError = NULL) {
    if (memError) {
        *memError = 0;
    }

    uint16_t count = countValuesInCStr(msg, separator);
    // minimal length is 6:
    // - preamble of 4 (raw, frequency, # code pairs sequence 1, # code pairs sequence 2)
    // - 1 code pair
    if (count < 6) {
        return NULL;
    }

    uint16_t *codeArray = reinterpret_cast<uint16_t *>(malloc(count * sizeof(uint16_t)));
    if (codeArray == NULL) {  // malloc failed, so give up.
        if (memError) {
            *memError = 1;
        }
        return NULL;
    }

    int16_t  index = 0;
    uint16_t startFrom = 0;
    count = 0;
    while (msg[index] != 0) {
        if (msg[index] == separator) {
            codeArray[count] = strtoul(msg + startFrom, NULL, 16);
            startFrom = index + 1;
            count++;
        }
        index++;
    }
    if (index > startFrom) {
        codeArray[count] = strtoul(msg + startFrom, NULL, 16);
        count++;
    }

    // Validate PRONTO code
    // Only raw pronto codes are supported
    if (codeArray[0] != 0) {
        free(codeArray);
        return NULL;
    }

    uint16_t seq1Len = codeArray[2] * 2;
    uint16_t seq2Len = codeArray[3] * 2;
    uint16_t seq1Start = 4;
    uint16_t seq2Start = seq1Start + seq1Len;

    if (seq1Len > 0 && seq1Len + seq1Start > count) {
        free(codeArray);
        return NULL;
    }

    if (seq2Len > 0 && seq2Len + seq2Start > count) {
        free(codeArray);
        return NULL;
    }


    *codeCount = count;
    return codeArray;
}

uint16_t *globalCacheBufferToArray(const char *msg, uint16_t *codeCount, int *memError = NULL) {
    if (memError) {
        *memError = 0;
    }

    char separator = ',';
    uint16_t count = countValuesInCStr(msg, separator);

    uint16_t startIndex = 0;
    if (strncmp(msg, "sendir", 6) == 0) {
        count -= 3;
        startIndex = 3;
    }

    // minimal length is ???:
    if (count < 6) {
        return NULL;
    }

    uint16_t *codeArray = reinterpret_cast<uint16_t *>(malloc(count * sizeof(uint16_t)));
    if (codeArray == NULL) {  // malloc failed, so give up.
        if (memError) {
            *memError = 1;
        }
        return NULL;
    }

    int16_t  msgIndex = 0;
    uint16_t codeIndex = 0;
    uint16_t startFrom = 0;
    count = 0;
    while (msg[msgIndex] != 0) {
        if (msg[msgIndex] == separator) {
            if (count >= startIndex) {
                codeArray[codeIndex] = strtoul(msg + startFrom, NULL, 10);
                codeIndex++;
            }
            startFrom = msgIndex + 1;
            count++;
        }
        msgIndex++;
    }
    if (msgIndex > startFrom) {
        codeArray[codeIndex] = strtoul(msg + startFrom, NULL, 10);
        codeIndex++;
    }

    *codeCount = codeIndex;
    return codeArray;
}
