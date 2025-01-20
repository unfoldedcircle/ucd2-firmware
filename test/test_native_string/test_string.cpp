#include <stdio.h>
#include <unity.h>

#include "string_util.hpp"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_replacechar_nullInput(void) {
    TEST_ASSERT_EQUAL(0, replacechar(nullptr, 'a', 'b'));
}

void test_replacechar_emptyInput(void) {
    char buf[1];
    buf[0] = 0;
    TEST_ASSERT_EQUAL(0, replacechar(buf, 'a', 'b'));
}

void test_replacechar_noMatch(void) {
    char buf[20];
    snprintf(buf, sizeof(buf), "foobar");
    TEST_ASSERT_EQUAL(0, replacechar(buf, 'c', 'd'));
    TEST_ASSERT_EQUAL_STRING("foobar", buf);
}

void test_replacechar_singleMatch(void) {
    char buf[20];
    snprintf(buf, sizeof(buf), "foobar");
    TEST_ASSERT_EQUAL(1, replacechar(buf, 'r', 's'));
    TEST_ASSERT_EQUAL_STRING("foobas", buf);

    TEST_ASSERT_EQUAL(1, replacechar(buf, 'f', 'r'));
    TEST_ASSERT_EQUAL_STRING("roobas", buf);

    TEST_ASSERT_EQUAL(1, replacechar(buf, 'b', 'r'));
    TEST_ASSERT_EQUAL_STRING("rooras", buf);
}

void test_replacechar_multiMatch(void) {
    char buf[20];
    snprintf(buf, sizeof(buf), "foobar");
    TEST_ASSERT_EQUAL(2, replacechar(buf, 'o', 'u'));
    TEST_ASSERT_EQUAL_STRING("fuubar", buf);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_replacechar_nullInput);
    RUN_TEST(test_replacechar_emptyInput);
    RUN_TEST(test_replacechar_noMatch);
    RUN_TEST(test_replacechar_singleMatch);
    RUN_TEST(test_replacechar_multiMatch);

    UNITY_END();

    return 0;
}
