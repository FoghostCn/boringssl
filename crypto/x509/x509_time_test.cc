/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Tests for X509 time functions */

#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <gtest/gtest.h>

typedef struct {
    const char *data;
    int type;
    time_t cmp_time;
    /* -1 if asn1_time <= cmp_time, 1 if asn1_time > cmp_time, 0 if error. */
    int expected;
} TESTDATA;

static TESTDATA x509_cmp_tests[] = {
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* Same as UTC time. */
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* UTCTime from the 20th century. */
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        919274514, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        919274515, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        919274513, 1,
    },
    /* Various invalid formats. */
    {
        /* No trailing Z. */
        "20170217180154", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No trailing Z, UTCTime. */
        "170217180154", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* No seconds. */
        "201702171801Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No seconds, UTCTime. */
        "1702171801Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Fractional seconds. */
        "20170217180154.001Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Fractional seconds, UTCTime. */
        "170217180154.001Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Timezone offset. */
        "20170217180154+0100", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Timezone offset, UTCTime. */
        "170217180154+0100", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Extra digits. */
        "2017021718015400Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Extra digits, UTCTime. */
        "17021718015400Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Non-digits. */
        "2017021718015aZ", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Non-digits, UTCTime. */
        "17021718015aZ", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Trailing garbage. */
        "20170217180154Zlongtrailinggarbage", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Trailing garbage, UTCTime. */
        "170217180154Zlongtrailinggarbage", V_ASN1_UTCTIME, 0, 0,
    },
    {
         /* Swapped type. */
        "20170217180154Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Swapped type. */
        "170217180154Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Bad type. */
        "20170217180154Z", V_ASN1_OCTET_STRING, 0, 0,
    },
};

TEST(X509TimeTest, TestCmpTime) {
  for (size_t idx = 0; idx < sizeof(x509_cmp_tests)/sizeof(TESTDATA); ++idx) {
    ASN1_TIME t;
    int result;

    memset(&t, 0, sizeof(t));
    t.type = x509_cmp_tests[idx].type;
    t.data = (unsigned char*)(x509_cmp_tests[idx].data);
    t.length = strlen(x509_cmp_tests[idx].data);

    result = X509_cmp_time(&t, &x509_cmp_tests[idx].cmp_time);
    ASSERT_EQ(x509_cmp_tests[idx].expected, result);
  }
}

TEST(X509TimeTest, TestCmpTimeCurrent) {
    time_t now = time(NULL);
    /* Pick a day earlier and later, relative to any system clock. */
    ASN1_TIME *asn1_before = NULL, *asn1_after = NULL;
    int cmp_result = 0;

    asn1_before = ASN1_TIME_adj(NULL, now, -1, 0);
    asn1_after = ASN1_TIME_adj(NULL, now, 1, 0);

    cmp_result  = X509_cmp_time(asn1_before, NULL);
    ASSERT_EQ(-1, cmp_result);

    cmp_result = X509_cmp_time(asn1_after, NULL);
    ASSERT_EQ(1, cmp_result);

    ASN1_TIME_free(asn1_before);
    ASN1_TIME_free(asn1_after);
}
