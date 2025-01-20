#include <unity.h>

#include "event_bits.hpp"

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_bit_not_set(void) {
    int mask = 4;
    TEST_ASSERT_EQUAL(false, bitIsSetAndNoHigherPrioTask(mask, 1));
    TEST_ASSERT_EQUAL(false, bitIsSetAndNoHigherPrioTask(mask, 3));
    TEST_ASSERT_EQUAL(false, bitIsSetAndNoHigherPrioTask(mask, 8));
    TEST_ASSERT_EQUAL(false, bitIsSetAndNoHigherPrioTask(mask, 1+8));
    TEST_ASSERT_EQUAL(false, bitIsSetAndNoHigherPrioTask(mask, 16));
}

// edge case with highest prio task
void test_bitset_highest_priotask_only(void) {
    int mask = 1;
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 1));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 3));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 5));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 0xFF));
}

void test_bitset_only_lower_priotasks(void) {
    int mask = 4;
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 4));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 4+8));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 4+8+16));
    TEST_ASSERT_EQUAL(true, bitIsSetAndNoHigherPrioTask(mask, 4+8+16+32));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_bit_not_set);
    RUN_TEST(test_bitset_highest_priotask_only);
    RUN_TEST(test_bitset_only_lower_priotasks);
    UNITY_END();

    return 0;
}
