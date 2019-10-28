/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdint.h>
#include <string.h>
#include <luid.h>

#include "tests-luid.h"

static void test_luid_uniqe(void)
{
    uint8_t a[8];
    uint8_t b[8];

    luid_get(a, sizeof(a));
    luid_get(b, sizeof(b));
    TEST_ASSERT_EQUAL_INT(0, !memcmp(a, b, sizeof(a)));
}

static void test_luid_uniqe_mac(void)
{
    uint8_t a[8];
    uint8_t b[8];

    luid_get(a, sizeof(a));
    luid_get(b, sizeof(b));

    a[0] &= ~(0x01);
    a[0] |=  (0x02);
    b[0] &= ~(0x01);
    b[0] |=  (0x02);

    TEST_ASSERT_EQUAL_INT(0, !memcmp(a, b, sizeof(a)));
}

static void test_luid_custom(void)
{
    uint8_t a[2][8];
    uint8_t b[2][8];

    luid_custom(a[0], sizeof(a[0]), 0xfefe);
    luid_custom(a[1], sizeof(a[1]), 0xfefe);
    luid_custom(b[0], sizeof(b[0]), 0xbeef);
    luid_custom(b[1], sizeof(b[1]), 0xbeef);

    TEST_ASSERT_EQUAL_INT(0, !memcmp(a[0], b[0], sizeof(a[0])));
    TEST_ASSERT_EQUAL_INT(0, !memcmp(a[1], b[1], sizeof(a[1])));
    TEST_ASSERT_EQUAL_INT(0, memcmp(a[0], a[0], sizeof(a[0])));
    TEST_ASSERT_EQUAL_INT(0, memcmp(b[1], b[1], sizeof(b[0])));
}

Test *tests_luid_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_luid_uniqe),
        new_TestFixture(test_luid_uniqe_mac),
        new_TestFixture(test_luid_custom),
    };

    EMB_UNIT_TESTCALLER(luid_tests, NULL, NULL, fixtures);

    return (Test *)&luid_tests;
}

void tests_luid(void)
{
    TESTS_RUN(tests_luid_tests());
}
