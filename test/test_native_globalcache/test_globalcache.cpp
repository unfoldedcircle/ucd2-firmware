#include <unity.h>

#include "globalcache.hpp"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}


void test_parseGcRequest_nullInput(void) {
    const char *request = "blink";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, nullptr));
    TEST_ASSERT_EQUAL(0, parseGcRequest(nullptr, &msg));
    TEST_ASSERT_EQUAL(0, parseGcRequest(nullptr, nullptr));
}

void test_parseGcRequest_emptyInput(void) {
    const char *request = "";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, &msg));
}

void test_parseGcRequest_commandOnlyTooLong(void) {
    const char *request = "01234567890123456789";
    GCMsg msg;
    TEST_ASSERT_EQUAL(1, parseGcRequest(request, &msg));
}

void test_parseGcRequest_commandWithParamTooLong(void) {
    const char *request = "01234567890123456789,foobar";
    GCMsg msg;
    TEST_ASSERT_EQUAL(1, parseGcRequest(request, &msg));
}

void test_parseGcRequest_commandTooLong(void) {
    const char *request = "01234567890123456789,1:1,foo,bar";
    GCMsg msg;
    TEST_ASSERT_EQUAL(1, parseGcRequest(request, &msg));
}

void test_parseGcRequest_commandOnly(void) {
    const char *request = "blink";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, &msg));
    TEST_ASSERT_EQUAL_STRING("blink", msg.command);
    TEST_ASSERT_EQUAL(0, msg.module);
    TEST_ASSERT_EQUAL(0, msg.port);
}

void test_parseGcRequest_commandAndModule(void) {
    const char *request = "stopir,1:3";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, &msg));
    TEST_ASSERT_EQUAL_STRING("stopir", msg.command);
    TEST_ASSERT_EQUAL(1, msg.module);
    TEST_ASSERT_EQUAL(3, msg.port);
    TEST_ASSERT_EQUAL(nullptr, msg.param);
}

void test_parseGcRequest_commandAndParam(void) {
    const char *request = "blink,1";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, &msg));
    TEST_ASSERT_EQUAL_STRING("blink", msg.command);
    TEST_ASSERT_EQUAL(0, msg.module);
    TEST_ASSERT_EQUAL(0, msg.port);
    TEST_ASSERT_EQUAL_STRING("1", msg.param);
}

void test_parseGcRequest_full(void) {
    const char *request = "sendir,1:1,1,37000,1,1,128,64,16,16,16,16,16,48,16,16,16,48,16,16,16,48,16,16,16,16,16,48,16,16,16,16,16,48,16,48,16,16,16,16,16,16,16,16,16,16,16,16,16,48,16,16,16,48,16,16,16,48,16,16,16,16,16,16,16,48,16,48,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,48,16,16,16,48,16,16,16,16,16,16,16,16,16,48,16,16,16,16,16,2765";
    GCMsg msg;
    TEST_ASSERT_EQUAL(0, parseGcRequest(request, &msg));
    TEST_ASSERT_EQUAL_STRING("sendir", msg.command);
    TEST_ASSERT_EQUAL(1, msg.module);
    TEST_ASSERT_EQUAL(1, msg.port);
    TEST_ASSERT_EQUAL_STRING("1,37000,1,1,128,64,16,16,16,16,16,48,16,16,16,48,16,16,16,48,16,16,16,16,16,48,16,16,16,16,16,48,16,48,16,16,16,16,16,16,16,16,16,16,16,16,16,48,16,16,16,48,16,16,16,48,16,16,16,16,16,16,16,48,16,48,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,48,16,16,16,48,16,16,16,16,16,16,16,16,16,48,16,16,16,16,16,2765",
                             msg.param);
}

void test_parseGcRequest_outOfRangeModule(void) {
    GCMsg msg;
    // only module 1 is valid
    TEST_ASSERT_EQUAL(0, parseGcRequest("stopir,1:3", &msg));
    // out of range
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,0:3", &msg));
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,2:3", &msg));
}

void test_parseGcRequest_invalidModule(void) {
    GCMsg msg;
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,:3", &msg));
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,a:3", &msg));
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,:3,1", &msg));
    TEST_ASSERT_EQUAL(2, parseGcRequest("stopir,a:3,1", &msg));
}

void test_parseGcRequest_outOfRangePort(void) {
    GCMsg msg;
    // valid range
    TEST_ASSERT_EQUAL(0, parseGcRequest("stopir,1:1", &msg));
    TEST_ASSERT_EQUAL(0, parseGcRequest("stopir,1:15", &msg));
    // out of range
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:0", &msg));
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:16", &msg));
}

void test_parseGcRequest_invalidPort(void) {
    GCMsg msg;
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:", &msg));
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:,2", &msg));
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:a", &msg));
    TEST_ASSERT_EQUAL(3, parseGcRequest("stopir,1:a,2", &msg));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_parseGcRequest_nullInput);
    RUN_TEST(test_parseGcRequest_emptyInput);
    RUN_TEST(test_parseGcRequest_commandOnlyTooLong);
    RUN_TEST(test_parseGcRequest_commandWithParamTooLong);
    RUN_TEST(test_parseGcRequest_commandTooLong);
    RUN_TEST(test_parseGcRequest_commandOnly);
    RUN_TEST(test_parseGcRequest_commandAndParam);
    RUN_TEST(test_parseGcRequest_commandAndModule);
    RUN_TEST(test_parseGcRequest_full);
    RUN_TEST(test_parseGcRequest_outOfRangeModule);
    RUN_TEST(test_parseGcRequest_invalidModule);
    RUN_TEST(test_parseGcRequest_outOfRangePort);
    RUN_TEST(test_parseGcRequest_invalidPort);

    UNITY_END();

    return 0;
}
