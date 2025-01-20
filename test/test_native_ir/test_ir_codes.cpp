#include <unity.h>

// @hack had no better idea than this. Including IRremoteESP8266 just didn't work
#include "IRremoteESP8266_mock.h"
#include "ir_codes.hpp"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_buildIRHexData(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;15;1";
    TEST_ASSERT_EQUAL(true, buildIRHexData(msg, &data));
    TEST_ASSERT_EQUAL(4, data.protocol);
    TEST_ASSERT_EQUAL(0x640C, data.command);
    TEST_ASSERT_EQUAL(15, data.bits);
    TEST_ASSERT_EQUAL(1, data.repeat);
}

void test_buildIRHexData_empty_string(void) {
    struct IRHexData data;
    String           msg;
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_invalid_separator(void) {
    struct IRHexData data;
    String           msg = "4,0x640C,15,0";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_missing_protocol_value(void) {
    struct IRHexData data;
    String           msg = ";0x640C;15;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_missing_command_value(void) {
    struct IRHexData data;
    String           msg = "4;;15;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_missing_bits_value(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_missing_repeat_value(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;15;";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_missing_repeat(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;15";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_invalid_protocol_value(void) {
    struct IRHexData data;
    String           msg = "z;0x640C;15;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_invalid_command_value(void) {
    struct IRHexData data;
    String           msg = "4;hello;15;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_invalid_bits_value(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;2tt;1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_invalid_repeat_value(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;15;z1";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_buildIRHexData_repeat_too_high(void) {
    struct IRHexData data;
    String           msg = "4;0x640C;15;20";
    TEST_ASSERT_EQUAL(true, buildIRHexData(msg, &data));
    msg = "4;0x640C;15;21";
    TEST_ASSERT_EQUAL(false, buildIRHexData(msg, &data));
}

void test_countValuesInCStr_null_input(void) {
    TEST_ASSERT_EQUAL(0, countValuesInCStr(NULL, ','));
}

void test_countValuesInCStr_empty_input(void) {
    TEST_ASSERT_EQUAL(0, countValuesInCStr("", ','));
}

void test_countValuesInCStr_without_separator(void) {
    TEST_ASSERT_EQUAL(1, countValuesInCStr("h", ','));
    TEST_ASSERT_EQUAL(1, countValuesInCStr("hi", ','));
    TEST_ASSERT_EQUAL(1, countValuesInCStr("hi there", ','));
}

void test_countValuesInCStr(void) {
    TEST_ASSERT_EQUAL(2, countValuesInCStr("0,1", ','));
    TEST_ASSERT_EQUAL(3, countValuesInCStr("0,1,2", ','));
}

void test_prontoBufferToArray_emptyInput(void) {
    uint16_t codeCount;
    TEST_ASSERT_NULL(prontoBufferToArray("", ',', &codeCount));

}

void test_prontoBufferToArray_notEnoughInput(void) {
    uint16_t codeCount;
    TEST_ASSERT_NULL(prontoBufferToArray("0000", ' ', &codeCount));
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066", ' ', &codeCount));
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066 0000", ' ', &codeCount));
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066 0000 0001", ' ', &codeCount));
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066 0000 0001 0050", ' ', &codeCount));
}

void test_prontoBufferToArray_inputTooShort(void) {
    uint16_t codeCount;
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066 0000 0018 0050 0051", ' ', &codeCount));
    TEST_ASSERT_NULL(prontoBufferToArray("0000 0066 0000 0002 0050 0051", ' ', &codeCount));
}

void test_prontoBufferToArray_minLength(void) {
    uint16_t codeCount;
    auto buffer = prontoBufferToArray("0000 0066 0000 0001 0050 0051", ' ', &codeCount);
    TEST_ASSERT_EQUAL(6, codeCount);
    TEST_ASSERT_NOT_NULL(buffer);
    free(buffer);
}

void test_prontoBufferToArray(void) {
    uint16_t codeCount;
    int memError;
    auto buffer = prontoBufferToArray("0000,0066,0000,0018,0050,0051,0015,008e,0051,0050,0015,008f,0014,008f,0050,0051,0050,0051,0015,05af,0051,0050,0015,008e,0051,0051,0014,008f,0015,008e,0050,0051,0051,0050,0015,05af,0051,0050,0015,008e,0051,0051,0015,008e,0015,008e,0050,0051,0051,0050,0015,0ff1",
        ',', &codeCount, &memError);
    TEST_ASSERT_EQUAL(0, memError);
    TEST_ASSERT_NOT_NULL(buffer);
    free(buffer);
}

void test_globalCacheBufferToArray_emptyInput(void) {
    uint16_t codeCount;
    TEST_ASSERT_NULL(globalCacheBufferToArray("", &codeCount));
}

void test_globalCacheBufferToArray_short(void) {
    uint16_t codeCount;
    int memError;
    auto buffer = globalCacheBufferToArray("38000,1,69,340,171,21,21,21,21,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,21,21,65,21,65,21,65,21,65,21,65,21,21,21,65,21,21,21,21,21,21,21,21,21,21,21,21,21,65,21,21,21,65,21,65,21,65,21,65,21,65,21,65,21,1555,340,86,21,3678",
        &codeCount, &memError);
    TEST_ASSERT_EQUAL(0, memError);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL(75, codeCount);
    TEST_ASSERT_EQUAL(38000, buffer[0]);
    TEST_ASSERT_EQUAL(3678, buffer[codeCount-1]);
    free(buffer);
}

void test_globalCacheBufferToArray_full(void) {
    uint16_t codeCount;
    int memError;
    auto buffer = globalCacheBufferToArray("sendir,1:1,1,38000,1,69,340,171,21,21,21,21,21,65,21,21,21,21,21,21,21,21,21,21,21,65,21,65,21,21,21,65,21,65,21,65,21,65,21,65,21,21,21,65,21,21,21,21,21,21,21,21,21,21,21,21,21,65,21,21,21,65,21,65,21,65,21,65,21,65,21,65,21,1555,340,86,21,3678",
        &codeCount, &memError);
    TEST_ASSERT_EQUAL(0, memError);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL(75, codeCount);
    TEST_ASSERT_EQUAL(38000, buffer[0]);
    TEST_ASSERT_EQUAL(3678, buffer[codeCount-1]);
    free(buffer);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_buildIRHexData);
    RUN_TEST(test_buildIRHexData_empty_string);
    RUN_TEST(test_buildIRHexData_invalid_separator);
    RUN_TEST(test_buildIRHexData_missing_protocol_value);
    RUN_TEST(test_buildIRHexData_missing_command_value);
    RUN_TEST(test_buildIRHexData_missing_bits_value);
    RUN_TEST(test_buildIRHexData_missing_repeat_value);
    RUN_TEST(test_buildIRHexData_missing_repeat);

    RUN_TEST(test_buildIRHexData_invalid_protocol_value);
    RUN_TEST(test_buildIRHexData_invalid_command_value);
    RUN_TEST(test_buildIRHexData_invalid_bits_value);
    RUN_TEST(test_buildIRHexData_invalid_repeat_value);

    RUN_TEST(test_buildIRHexData_repeat_too_high);

    RUN_TEST(test_countValuesInCStr_null_input);
    RUN_TEST(test_countValuesInCStr_empty_input);
    RUN_TEST(test_countValuesInCStr_without_separator);
    RUN_TEST(test_countValuesInCStr);

    RUN_TEST(test_prontoBufferToArray_emptyInput);
    RUN_TEST(test_prontoBufferToArray_notEnoughInput);
    RUN_TEST(test_prontoBufferToArray_inputTooShort);
    RUN_TEST(test_prontoBufferToArray_minLength);
    RUN_TEST(test_prontoBufferToArray);

    RUN_TEST(test_globalCacheBufferToArray_emptyInput);
    RUN_TEST(test_globalCacheBufferToArray_short);
    RUN_TEST(test_globalCacheBufferToArray_full);

    UNITY_END();

    return 0;
}
