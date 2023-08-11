/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <openssl/dh.h>

#include <stdio.h>
#include <string.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../fipsmodule/dh/internal.h"
#include "../test/test_util.h"


TEST(DHTest, Basic) {
  bssl::UniquePtr<DH> a(DH_new());
  ASSERT_TRUE(a);
  ASSERT_TRUE(DH_generate_parameters_ex(a.get(), 64, DH_GENERATOR_5, nullptr));

  int check_result;
  ASSERT_TRUE(DH_check(a.get(), &check_result));
  EXPECT_FALSE(check_result & DH_CHECK_P_NOT_PRIME);
  EXPECT_FALSE(check_result & DH_CHECK_P_NOT_SAFE_PRIME);
  EXPECT_FALSE(check_result & DH_CHECK_UNABLE_TO_CHECK_GENERATOR);
  EXPECT_FALSE(check_result & DH_CHECK_NOT_SUITABLE_GENERATOR);

  bssl::UniquePtr<DH> b(DHparams_dup(a.get()));
  ASSERT_TRUE(b);

  ASSERT_TRUE(DH_generate_key(a.get()));
  ASSERT_TRUE(DH_generate_key(b.get()));

  std::vector<uint8_t> key1(DH_size(a.get()));
  int ret = DH_compute_key(key1.data(), DH_get0_pub_key(b.get()), a.get());
  ASSERT_GE(ret, 0);
  key1.resize(ret);

  std::vector<uint8_t> key2(DH_size(b.get()));
  ret = DH_compute_key(key2.data(), DH_get0_pub_key(a.get()), b.get());
  ASSERT_GE(ret, 0);
  key2.resize(ret);

  EXPECT_EQ(Bytes(key1), Bytes(key2));

  // |DH_compute_key|, unlike |DH_compute_key_padded|, removes leading zeros
  // from the output, so the key will not have a fixed length. This test uses a
  // small, 64-bit prime, so check for at least 32 bits of output after removing
  // leading zeros.
  EXPECT_GE(key1.size(), 4u);
}

// The following parameters are taken from RFC 5114, section 2.2. This is not a
// safe prime. Do not use these parameters.
static const uint8_t kRFC5114_2048_224P[] = {
    0xad, 0x10, 0x7e, 0x1e, 0x91, 0x23, 0xa9, 0xd0, 0xd6, 0x60, 0xfa, 0xa7,
    0x95, 0x59, 0xc5, 0x1f, 0xa2, 0x0d, 0x64, 0xe5, 0x68, 0x3b, 0x9f, 0xd1,
    0xb5, 0x4b, 0x15, 0x97, 0xb6, 0x1d, 0x0a, 0x75, 0xe6, 0xfa, 0x14, 0x1d,
    0xf9, 0x5a, 0x56, 0xdb, 0xaf, 0x9a, 0x3c, 0x40, 0x7b, 0xa1, 0xdf, 0x15,
    0xeb, 0x3d, 0x68, 0x8a, 0x30, 0x9c, 0x18, 0x0e, 0x1d, 0xe6, 0xb8, 0x5a,
    0x12, 0x74, 0xa0, 0xa6, 0x6d, 0x3f, 0x81, 0x52, 0xad, 0x6a, 0xc2, 0x12,
    0x90, 0x37, 0xc9, 0xed, 0xef, 0xda, 0x4d, 0xf8, 0xd9, 0x1e, 0x8f, 0xef,
    0x55, 0xb7, 0x39, 0x4b, 0x7a, 0xd5, 0xb7, 0xd0, 0xb6, 0xc1, 0x22, 0x07,
    0xc9, 0xf9, 0x8d, 0x11, 0xed, 0x34, 0xdb, 0xf6, 0xc6, 0xba, 0x0b, 0x2c,
    0x8b, 0xbc, 0x27, 0xbe, 0x6a, 0x00, 0xe0, 0xa0, 0xb9, 0xc4, 0x97, 0x08,
    0xb3, 0xbf, 0x8a, 0x31, 0x70, 0x91, 0x88, 0x36, 0x81, 0x28, 0x61, 0x30,
    0xbc, 0x89, 0x85, 0xdb, 0x16, 0x02, 0xe7, 0x14, 0x41, 0x5d, 0x93, 0x30,
    0x27, 0x82, 0x73, 0xc7, 0xde, 0x31, 0xef, 0xdc, 0x73, 0x10, 0xf7, 0x12,
    0x1f, 0xd5, 0xa0, 0x74, 0x15, 0x98, 0x7d, 0x9a, 0xdc, 0x0a, 0x48, 0x6d,
    0xcd, 0xf9, 0x3a, 0xcc, 0x44, 0x32, 0x83, 0x87, 0x31, 0x5d, 0x75, 0xe1,
    0x98, 0xc6, 0x41, 0xa4, 0x80, 0xcd, 0x86, 0xa1, 0xb9, 0xe5, 0x87, 0xe8,
    0xbe, 0x60, 0xe6, 0x9c, 0xc9, 0x28, 0xb2, 0xb9, 0xc5, 0x21, 0x72, 0xe4,
    0x13, 0x04, 0x2e, 0x9b, 0x23, 0xf1, 0x0b, 0x0e, 0x16, 0xe7, 0x97, 0x63,
    0xc9, 0xb5, 0x3d, 0xcf, 0x4b, 0xa8, 0x0a, 0x29, 0xe3, 0xfb, 0x73, 0xc1,
    0x6b, 0x8e, 0x75, 0xb9, 0x7e, 0xf3, 0x63, 0xe2, 0xff, 0xa3, 0x1f, 0x71,
    0xcf, 0x9d, 0xe5, 0x38, 0x4e, 0x71, 0xb8, 0x1c, 0x0a, 0xc4, 0xdf, 0xfe,
    0x0c, 0x10, 0xe6, 0x4f,
};
static const uint8_t kRFC5114_2048_224G[] = {
    0xac, 0x40, 0x32, 0xef, 0x4f, 0x2d, 0x9a, 0xe3, 0x9d, 0xf3, 0x0b, 0x5c,
    0x8f, 0xfd, 0xac, 0x50, 0x6c, 0xde, 0xbe, 0x7b, 0x89, 0x99, 0x8c, 0xaf,
    0x74, 0x86, 0x6a, 0x08, 0xcf, 0xe4, 0xff, 0xe3, 0xa6, 0x82, 0x4a, 0x4e,
    0x10, 0xb9, 0xa6, 0xf0, 0xdd, 0x92, 0x1f, 0x01, 0xa7, 0x0c, 0x4a, 0xfa,
    0xab, 0x73, 0x9d, 0x77, 0x00, 0xc2, 0x9f, 0x52, 0xc5, 0x7d, 0xb1, 0x7c,
    0x62, 0x0a, 0x86, 0x52, 0xbe, 0x5e, 0x90, 0x01, 0xa8, 0xd6, 0x6a, 0xd7,
    0xc1, 0x76, 0x69, 0x10, 0x19, 0x99, 0x02, 0x4a, 0xf4, 0xd0, 0x27, 0x27,
    0x5a, 0xc1, 0x34, 0x8b, 0xb8, 0xa7, 0x62, 0xd0, 0x52, 0x1b, 0xc9, 0x8a,
    0xe2, 0x47, 0x15, 0x04, 0x22, 0xea, 0x1e, 0xd4, 0x09, 0x93, 0x9d, 0x54,
    0xda, 0x74, 0x60, 0xcd, 0xb5, 0xf6, 0xc6, 0xb2, 0x50, 0x71, 0x7c, 0xbe,
    0xf1, 0x80, 0xeb, 0x34, 0x11, 0x8e, 0x98, 0xd1, 0x19, 0x52, 0x9a, 0x45,
    0xd6, 0xf8, 0x34, 0x56, 0x6e, 0x30, 0x25, 0xe3, 0x16, 0xa3, 0x30, 0xef,
    0xbb, 0x77, 0xa8, 0x6f, 0x0c, 0x1a, 0xb1, 0x5b, 0x05, 0x1a, 0xe3, 0xd4,
    0x28, 0xc8, 0xf8, 0xac, 0xb7, 0x0a, 0x81, 0x37, 0x15, 0x0b, 0x8e, 0xeb,
    0x10, 0xe1, 0x83, 0xed, 0xd1, 0x99, 0x63, 0xdd, 0xd9, 0xe2, 0x63, 0xe4,
    0x77, 0x05, 0x89, 0xef, 0x6a, 0xa2, 0x1e, 0x7f, 0x5f, 0x2f, 0xf3, 0x81,
    0xb5, 0x39, 0xcc, 0xe3, 0x40, 0x9d, 0x13, 0xcd, 0x56, 0x6a, 0xfb, 0xb4,
    0x8d, 0x6c, 0x01, 0x91, 0x81, 0xe1, 0xbc, 0xfe, 0x94, 0xb3, 0x02, 0x69,
    0xed, 0xfe, 0x72, 0xfe, 0x9b, 0x6a, 0xa4, 0xbd, 0x7b, 0x5a, 0x0f, 0x1c,
    0x71, 0xcf, 0xff, 0x4c, 0x19, 0xc4, 0x18, 0xe1, 0xf6, 0xec, 0x01, 0x79,
    0x81, 0xbc, 0x08, 0x7f, 0x2a, 0x70, 0x65, 0xb3, 0x84, 0xb8, 0x90, 0xd3,
    0x19, 0x1f, 0x2b, 0xfa,
};
static const uint8_t kRFC5114_2048_224Q[] = {
    0x80, 0x1c, 0x0d, 0x34, 0xc5, 0x8d, 0x93, 0xfe, 0x99, 0x71,
    0x77, 0x10, 0x1f, 0x80, 0x53, 0x5a, 0x47, 0x38, 0xce, 0xbc,
    0xbf, 0x38, 0x9a, 0x99, 0xb3, 0x63, 0x71, 0xeb,
};

// kRFC5114_2048_224BadY is a bad y-coordinate for RFC 5114's 2048-bit MODP
// Group with 224-bit Prime Order Subgroup (section 2.2).
static const uint8_t kRFC5114_2048_224BadY[] = {
    0x45, 0x32, 0x5f, 0x51, 0x07, 0xe5, 0xdf, 0x1c, 0xd6, 0x02, 0x82, 0xb3,
    0x32, 0x8f, 0xa4, 0x0f, 0x87, 0xb8, 0x41, 0xfe, 0xb9, 0x35, 0xde, 0xad,
    0xc6, 0x26, 0x85, 0xb4, 0xff, 0x94, 0x8c, 0x12, 0x4c, 0xbf, 0x5b, 0x20,
    0xc4, 0x46, 0xa3, 0x26, 0xeb, 0xa4, 0x25, 0xb7, 0x68, 0x8e, 0xcc, 0x67,
    0xba, 0xea, 0x58, 0xd0, 0xf2, 0xe9, 0xd2, 0x24, 0x72, 0x60, 0xda, 0x88,
    0x18, 0x9c, 0xe0, 0x31, 0x6a, 0xad, 0x50, 0x6d, 0x94, 0x35, 0x8b, 0x83,
    0x4a, 0x6e, 0xfa, 0x48, 0x73, 0x0f, 0x83, 0x87, 0xff, 0x6b, 0x66, 0x1f,
    0xa8, 0x82, 0xc6, 0x01, 0xe5, 0x80, 0xb5, 0xb0, 0x52, 0xd0, 0xe9, 0xd8,
    0x72, 0xf9, 0x7d, 0x5b, 0x8b, 0xa5, 0x4c, 0xa5, 0x25, 0x95, 0x74, 0xe2,
    0x7a, 0x61, 0x4e, 0xa7, 0x8f, 0x12, 0xe2, 0xd2, 0x9d, 0x8c, 0x02, 0x70,
    0x34, 0x44, 0x32, 0xc7, 0xb2, 0xf3, 0xb9, 0xfe, 0x17, 0x2b, 0xd6, 0x1f,
    0x8b, 0x7e, 0x4a, 0xfa, 0xa3, 0xb5, 0x3e, 0x7a, 0x81, 0x9a, 0x33, 0x66,
    0x62, 0xa4, 0x50, 0x18, 0x3e, 0xa2, 0x5f, 0x00, 0x07, 0xd8, 0x9b, 0x22,
    0xe4, 0xec, 0x84, 0xd5, 0xeb, 0x5a, 0xf3, 0x2a, 0x31, 0x23, 0xd8, 0x44,
    0x22, 0x2a, 0x8b, 0x37, 0x44, 0xcc, 0xc6, 0x87, 0x4b, 0xbe, 0x50, 0x9d,
    0x4a, 0xc4, 0x8e, 0x45, 0xcf, 0x72, 0x4d, 0xc0, 0x89, 0xb3, 0x72, 0xed,
    0x33, 0x2c, 0xbc, 0x7f, 0x16, 0x39, 0x3b, 0xeb, 0xd2, 0xdd, 0xa8, 0x01,
    0x73, 0x84, 0x62, 0xb9, 0x29, 0xd2, 0xc9, 0x51, 0x32, 0x9e, 0x7a, 0x6a,
    0xcf, 0xc1, 0x0a, 0xdb, 0x0e, 0xe0, 0x62, 0x77, 0x6f, 0x59, 0x62, 0x72,
    0x5a, 0x69, 0xa6, 0x5b, 0x70, 0xca, 0x65, 0xc4, 0x95, 0x6f, 0x9a, 0xc2,
    0xdf, 0x72, 0x6d, 0xb1, 0x1e, 0x54, 0x7b, 0x51, 0xb4, 0xef, 0x7f, 0x89,
    0x93, 0x74, 0x89, 0x59,
};

static bssl::UniquePtr<DH> NewDHGroup(const BIGNUM *p, const BIGNUM *q,
                                      const BIGNUM *g) {
  bssl::UniquePtr<BIGNUM> p_copy(BN_dup(p));
  bssl::UniquePtr<BIGNUM> q_copy(q != nullptr ? BN_dup(q) : nullptr);
  bssl::UniquePtr<BIGNUM> g_copy(BN_dup(g));
  bssl::UniquePtr<DH> dh(DH_new());
  if (p_copy == nullptr || (q != nullptr && q_copy == nullptr) ||
      g_copy == nullptr || dh == nullptr ||
      !DH_set0_pqg(dh.get(), p_copy.get(), q_copy.get(), g_copy.get())) {
    return nullptr;
  }
  p_copy.release();
  q_copy.release();
  g_copy.release();
  return dh;
}

TEST(DHTest, BadY) {
  bssl::UniquePtr<BIGNUM> p(
      BN_bin2bn(kRFC5114_2048_224P, sizeof(kRFC5114_2048_224P), nullptr));
  bssl::UniquePtr<BIGNUM> q(
      BN_bin2bn(kRFC5114_2048_224Q, sizeof(kRFC5114_2048_224Q), nullptr));
  bssl::UniquePtr<BIGNUM> g(
      BN_bin2bn(kRFC5114_2048_224G, sizeof(kRFC5114_2048_224G), nullptr));
  ASSERT_TRUE(p);
  ASSERT_TRUE(q);
  ASSERT_TRUE(g);
  bssl::UniquePtr<DH> dh = NewDHGroup(p.get(), q.get(), g.get());
  ASSERT_TRUE(dh);

  bssl::UniquePtr<BIGNUM> pub_key(
      BN_bin2bn(kRFC5114_2048_224BadY, sizeof(kRFC5114_2048_224BadY), nullptr));
  ASSERT_TRUE(pub_key);
  ASSERT_TRUE(DH_generate_key(dh.get()));

  int flags;
  ASSERT_TRUE(DH_check_pub_key(dh.get(), pub_key.get(), &flags));
  EXPECT_TRUE(flags & DH_CHECK_PUBKEY_INVALID)
      << "DH_check_pub_key did not reject the key";

  std::vector<uint8_t> result(DH_size(dh.get()));
  EXPECT_LT(DH_compute_key(result.data(), pub_key.get(), dh.get()), 0)
      << "DH_compute_key unexpectedly succeeded";
  ERR_clear_error();
}

static bool BIGNUMEqualsHex(const BIGNUM *bn, const char *hex) {
  BIGNUM *hex_bn = NULL;
  if (!BN_hex2bn(&hex_bn, hex)) {
    return false;
  }
  bssl::UniquePtr<BIGNUM> free_hex_bn(hex_bn);
  return BN_cmp(bn, hex_bn) == 0;
}

TEST(DHTest, ASN1) {
  // kParams are a set of Diffie-Hellman parameters generated with
  // openssl dhparam 256
  static const uint8_t kParams[] = {
      0x30, 0x26, 0x02, 0x21, 0x00, 0xd7, 0x20, 0x34, 0xa3, 0x27,
      0x4f, 0xdf, 0xbf, 0x04, 0xfd, 0x24, 0x68, 0x25, 0xb6, 0x56,
      0xd8, 0xab, 0x2a, 0x41, 0x2d, 0x74, 0x0a, 0x52, 0x08, 0x7c,
      0x40, 0x71, 0x4e, 0xd2, 0x57, 0x93, 0x13, 0x02, 0x01, 0x02,
  };

  CBS cbs;
  CBS_init(&cbs, kParams, sizeof(kParams));
  bssl::UniquePtr<DH> dh(DH_parse_parameters(&cbs));
  ASSERT_TRUE(dh);
  EXPECT_EQ(CBS_len(&cbs), 0u);
  EXPECT_TRUE(BIGNUMEqualsHex(
      DH_get0_p(dh.get()),
      "d72034a3274fdfbf04fd246825b656d8ab2a412d740a52087c40714ed2579313"));
  EXPECT_TRUE(BIGNUMEqualsHex(DH_get0_g(dh.get()), "2"));
  EXPECT_EQ(dh->priv_length, 0u);

  bssl::ScopedCBB cbb;
  uint8_t *der;
  size_t der_len;
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(DH_marshal_parameters(cbb.get(), dh.get()));
  ASSERT_TRUE(CBB_finish(cbb.get(), &der, &der_len));
  bssl::UniquePtr<uint8_t> free_der(der);
  EXPECT_EQ(Bytes(kParams), Bytes(der, der_len));

  // kParamsDSA are a set of Diffie-Hellman parameters generated with
  // openssl dhparam 256 -dsaparam
  static const uint8_t kParamsDSA[] = {
      0x30, 0x81, 0x89, 0x02, 0x41, 0x00, 0x93, 0xf3, 0xc1, 0x18, 0x01, 0xe6,
      0x62, 0xb6, 0xd1, 0x46, 0x9a, 0x2c, 0x72, 0xea, 0x31, 0xd9, 0x18, 0x10,
      0x30, 0x28, 0x63, 0xe2, 0x34, 0x7d, 0x80, 0xca, 0xee, 0x82, 0x2b, 0x19,
      0x3c, 0x19, 0xbb, 0x42, 0x83, 0x02, 0x70, 0xdd, 0xdb, 0x8c, 0x03, 0xab,
      0xe9, 0x9c, 0xc4, 0x00, 0x4d, 0x70, 0x5f, 0x52, 0x03, 0x31, 0x2c, 0xa4,
      0x67, 0x34, 0x51, 0x95, 0x2a, 0xac, 0x11, 0xe2, 0x6a, 0x55, 0x02, 0x40,
      0x44, 0xc8, 0x10, 0x53, 0x44, 0x32, 0x31, 0x63, 0xd8, 0xd1, 0x8c, 0x75,
      0xc8, 0x98, 0x53, 0x3b, 0x5b, 0x4a, 0x2a, 0x0a, 0x09, 0xe7, 0xd0, 0x3c,
      0x53, 0x72, 0xa8, 0x6b, 0x70, 0x41, 0x9c, 0x26, 0x71, 0x44, 0xfc, 0x7f,
      0x08, 0x75, 0xe1, 0x02, 0xab, 0x74, 0x41, 0xe8, 0x2a, 0x3d, 0x3c, 0x26,
      0x33, 0x09, 0xe4, 0x8b, 0xb4, 0x41, 0xec, 0xa6, 0xa8, 0xba, 0x1a, 0x07,
      0x8a, 0x77, 0xf5, 0x5f, 0x02, 0x02, 0x00, 0xa0,
  };

  CBS_init(&cbs, kParamsDSA, sizeof(kParamsDSA));
  dh.reset(DH_parse_parameters(&cbs));
  ASSERT_TRUE(dh);
  EXPECT_EQ(CBS_len(&cbs), 0u);
  EXPECT_TRUE(
      BIGNUMEqualsHex(DH_get0_p(dh.get()),
                      "93f3c11801e662b6d1469a2c72ea31d91810302863e2347d80caee8"
                      "22b193c19bb42830270dddb8c03abe99cc4004d705f5203312ca467"
                      "3451952aac11e26a55"));
  EXPECT_TRUE(
      BIGNUMEqualsHex(DH_get0_g(dh.get()),
                      "44c8105344323163d8d18c75c898533b5b4a2a0a09e7d03c5372a86"
                      "b70419c267144fc7f0875e102ab7441e82a3d3c263309e48bb441ec"
                      "a6a8ba1a078a77f55f"));
  EXPECT_EQ(dh->priv_length, 160u);

  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(DH_marshal_parameters(cbb.get(), dh.get()));
  ASSERT_TRUE(CBB_finish(cbb.get(), &der, &der_len));
  bssl::UniquePtr<uint8_t> free_der2(der);
  EXPECT_EQ(Bytes(kParamsDSA), Bytes(der, der_len));
}

TEST(DHTest, RFC3526) {
  bssl::UniquePtr<BIGNUM> bn(BN_get_rfc3526_prime_1536(nullptr));
  ASSERT_TRUE(bn);

  static const uint8_t kPrime1536[] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9, 0x0f, 0xda, 0xa2,
      0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6, 0x62, 0x8b, 0x80, 0xdc, 0x1c, 0xd1,
      0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67, 0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6,
      0x3b, 0x13, 0x9b, 0x22, 0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd,
      0xef, 0x95, 0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d,
      0xf2, 0x5f, 0x14, 0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51, 0xc2, 0x45,
      0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6, 0xf4, 0x4c, 0x42, 0xe9,
      0xa6, 0x37, 0xed, 0x6b, 0x0b, 0xff, 0x5c, 0xb6, 0xf4, 0x06, 0xb7, 0xed,
      0xee, 0x38, 0x6b, 0xfb, 0x5a, 0x89, 0x9f, 0xa5, 0xae, 0x9f, 0x24, 0x11,
      0x7c, 0x4b, 0x1f, 0xe6, 0x49, 0x28, 0x66, 0x51, 0xec, 0xe4, 0x5b, 0x3d,
      0xc2, 0x00, 0x7c, 0xb8, 0xa1, 0x63, 0xbf, 0x05, 0x98, 0xda, 0x48, 0x36,
      0x1c, 0x55, 0xd3, 0x9a, 0x69, 0x16, 0x3f, 0xa8, 0xfd, 0x24, 0xcf, 0x5f,
      0x83, 0x65, 0x5d, 0x23, 0xdc, 0xa3, 0xad, 0x96, 0x1c, 0x62, 0xf3, 0x56,
      0x20, 0x85, 0x52, 0xbb, 0x9e, 0xd5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6d,
      0x67, 0x0c, 0x35, 0x4e, 0x4a, 0xbc, 0x98, 0x04, 0xf1, 0x74, 0x6c, 0x08,
      0xca, 0x23, 0x73, 0x27, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  uint8_t buffer[sizeof(kPrime1536)];
  ASSERT_EQ(BN_num_bytes(bn.get()), sizeof(kPrime1536));
  ASSERT_EQ(BN_bn2bin(bn.get(), buffer), sizeof(kPrime1536));
  EXPECT_EQ(Bytes(buffer), Bytes(kPrime1536));
}

TEST(DHTest, LeadingZeros) {
  bssl::UniquePtr<BIGNUM> p(BN_get_rfc3526_prime_1536(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<BIGNUM> g(BN_new());
  ASSERT_TRUE(g);
  ASSERT_TRUE(BN_set_word(g.get(), 2));

  bssl::UniquePtr<DH> dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(dh);

  // These values are far too small to be reasonable Diffie-Hellman keys, but
  // they are an easy way to get a shared secret with leading zeros.
  bssl::UniquePtr<BIGNUM> priv_key(BN_new()), peer_key(BN_new());
  ASSERT_TRUE(priv_key);
  ASSERT_TRUE(BN_set_word(priv_key.get(), 2));
  ASSERT_TRUE(peer_key);
  ASSERT_TRUE(BN_set_word(peer_key.get(), 3));
  ASSERT_TRUE(DH_set0_key(dh.get(), /*pub_key=*/nullptr, priv_key.get()));
  priv_key.release();

  uint8_t padded[192] = {0};
  padded[191] = 9;
  static const uint8_t kTruncated[] = {9};
  EXPECT_EQ(int(sizeof(padded)), DH_size(dh.get()));

  std::vector<uint8_t> buf(DH_size(dh.get()));
  int len = DH_compute_key(buf.data(), peer_key.get(), dh.get());
  ASSERT_GT(len, 0);
  EXPECT_EQ(Bytes(buf.data(), len), Bytes(kTruncated));

  len = DH_compute_key_padded(buf.data(), peer_key.get(), dh.get());
  ASSERT_GT(len, 0);
  EXPECT_EQ(Bytes(buf.data(), len), Bytes(padded));
}

TEST(DHTest, Overwrite) {
  // Generate a DH key with the 1536-bit MODP group.
  bssl::UniquePtr<BIGNUM> p(BN_get_rfc3526_prime_1536(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<BIGNUM> g(BN_new());
  ASSERT_TRUE(g);
  ASSERT_TRUE(BN_set_word(g.get(), 2));

  bssl::UniquePtr<DH> key1 = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(key1);
  ASSERT_TRUE(DH_generate_key(key1.get()));

  bssl::UniquePtr<BIGNUM> peer_key(BN_new());
  ASSERT_TRUE(peer_key);
  ASSERT_TRUE(BN_set_word(peer_key.get(), 42));

  // Use the key to fill in cached values.
  std::vector<uint8_t> buf1(DH_size(key1.get()));
  ASSERT_GT(DH_compute_key_padded(buf1.data(), peer_key.get(), key1.get()), 0);

  // Generate a different key with a different group.
  p.reset(BN_get_rfc3526_prime_2048(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<DH> key2 = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(key2);
  ASSERT_TRUE(DH_generate_key(key2.get()));

  // Overwrite |key1|'s contents with |key2|.
  p.reset(BN_dup(DH_get0_p(key2.get())));
  ASSERT_TRUE(p);
  g.reset(BN_dup(DH_get0_g(key2.get())));
  ASSERT_TRUE(g);
  bssl::UniquePtr<BIGNUM> pub(BN_dup(DH_get0_pub_key(key2.get())));
  ASSERT_TRUE(pub);
  bssl::UniquePtr<BIGNUM> priv(BN_dup(DH_get0_priv_key(key2.get())));
  ASSERT_TRUE(priv);
  ASSERT_TRUE(DH_set0_pqg(key1.get(), p.get(), /*q=*/nullptr, g.get()));
  p.release();
  g.release();
  ASSERT_TRUE(DH_set0_key(key1.get(), pub.get(), priv.get()));
  pub.release();
  priv.release();

  // Verify that |key1| and |key2| behave equivalently.
  buf1.resize(DH_size(key1.get()));
  ASSERT_GT(DH_compute_key_padded(buf1.data(), peer_key.get(), key1.get()), 0);
  std::vector<uint8_t> buf2(DH_size(key2.get()));
  ASSERT_GT(DH_compute_key_padded(buf2.data(), peer_key.get(), key2.get()), 0);
  EXPECT_EQ(Bytes(buf1), Bytes(buf2));
}

TEST(DHTest, GenerateKeyTwice) {
  bssl::UniquePtr<BIGNUM> p(BN_get_rfc3526_prime_2048(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<BIGNUM> g(BN_new());
  ASSERT_TRUE(g);
  ASSERT_TRUE(BN_set_word(g.get(), 2));
  bssl::UniquePtr<DH> key1 = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(key1);
  ASSERT_TRUE(DH_generate_key(key1.get()));

  // Copy the parameters and private key to a new DH object.
  bssl::UniquePtr<DH> key2(DHparams_dup(key1.get()));
  ASSERT_TRUE(key2);
  bssl::UniquePtr<BIGNUM> priv_key(BN_dup(DH_get0_priv_key(key1.get())));
  ASSERT_TRUE(DH_set0_key(key2.get(), /*pub_key=*/NULL, priv_key.get()));
  priv_key.release();

  // This time, calling |DH_generate_key| preserves the old key and recomputes
  // the public key.
  ASSERT_TRUE(DH_generate_key(key2.get()));
  EXPECT_EQ(BN_cmp(DH_get0_priv_key(key1.get()), DH_get0_priv_key(key2.get())),
            0);
  EXPECT_EQ(BN_cmp(DH_get0_pub_key(key1.get()), DH_get0_pub_key(key2.get())),
            0);
}

// Bad parameters should be rejected, rather than cause a DoS risk in the
// event that an application uses Diffie-Hellman incorrectly, with untrusted
// domain parameters.
TEST(DHTest, InvalidParameters) {
  auto check_invalid_group = [](DH *dh) {
    // All operations on egregiously invalid groups should fail.
    EXPECT_FALSE(DH_generate_key(dh));
    int check_result;
    EXPECT_FALSE(DH_check(dh, &check_result));
    bssl::UniquePtr<BIGNUM> pub_key(BN_new());
    ASSERT_TRUE(pub_key);
    ASSERT_TRUE(BN_set_u64(pub_key.get(), 42));
    EXPECT_FALSE(DH_check_pub_key(dh, pub_key.get(), &check_result));
    uint8_t buf[1024];
    EXPECT_EQ(DH_compute_key(buf, pub_key.get(), dh), -1);
    EXPECT_EQ(DH_compute_key_padded(buf, pub_key.get(), dh), -1);
  };

  bssl::UniquePtr<BIGNUM> p(BN_get_rfc3526_prime_2048(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<BIGNUM> g(BN_new());
  ASSERT_TRUE(g);
  ASSERT_TRUE(BN_set_word(g.get(), 2));

  // p is negative.
  BN_set_negative(p.get(), 1);
  bssl::UniquePtr<DH> dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(dh);
  BN_set_negative(p.get(), 0);
  check_invalid_group(dh.get());

  // g is negative.
  BN_set_negative(g.get(), 1);
  dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(dh);
  BN_set_negative(g.get(), 0);
  check_invalid_group(dh.get());

  // g is not reduced mod p.
  dh = NewDHGroup(p.get(), /*q=*/nullptr, p.get());
  ASSERT_TRUE(dh);
  BN_set_negative(g.get(), 0);
  check_invalid_group(dh.get());

  // p is too large.
  bssl::UniquePtr<BIGNUM> large(BN_new());
  ASSERT_TRUE(BN_set_bit(large.get(), 0));
  ASSERT_TRUE(BN_set_bit(large.get(), 10000000));
  dh = NewDHGroup(large.get(), /*q=*/nullptr, g.get());
  ASSERT_TRUE(dh);
  check_invalid_group(dh.get());

  // q is too large.
  dh = NewDHGroup(p.get(), large.get(), g.get());
  ASSERT_TRUE(dh);
  check_invalid_group(dh.get());

  // Attempting to generate too large of a Diffie-Hellman group should fail.
  EXPECT_FALSE(
      DH_generate_parameters_ex(dh.get(), 20000, DH_GENERATOR_5, nullptr));
}

TEST(DHTest, PrivateKeyLength) {
  bssl::UniquePtr<BIGNUM> p(BN_get_rfc3526_prime_2048(nullptr));
  ASSERT_TRUE(p);
  bssl::UniquePtr<BIGNUM> g(BN_new());
  bssl::UniquePtr<BIGNUM> q(BN_new());
  ASSERT_TRUE(q);
  ASSERT_TRUE(BN_rshift1(q.get(), p.get()));  // (p-1)/2
  ASSERT_TRUE(g);
  ASSERT_TRUE(BN_set_word(g.get(), 2));

  EXPECT_EQ(BN_num_bits(p.get()), 2048u);
  EXPECT_EQ(BN_num_bits(q.get()), 2047u);

  // This test will only probabilistically notice some kinds of failures, so we
  // repeat it for several iterations.
  constexpr unsigned kIterations = 100;

  // If the private key was chosen from the range [1, M), num_bits(priv_key)
  // should be very close to num_bits(M), but may be a few bits short. Allow 128
  // leading zeros, which should fail with negligible probability.
  constexpr unsigned kMaxLeadingZeros = 128;

  for (unsigned i = 0; i < kIterations; i++) {
    // If unspecified, the private key is bounded by q = (p-1)/2.
    bssl::UniquePtr<DH> dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
    ASSERT_TRUE(dh);
    ASSERT_TRUE(DH_generate_key(dh.get()));
    EXPECT_LT(BN_cmp(DH_get0_priv_key(dh.get()), q.get()), 0);
    EXPECT_LE(BN_num_bits(q.get()) - kMaxLeadingZeros,
              BN_num_bits(DH_get0_priv_key(dh.get())));

    // Setting too large of a private key length should not be a DoS vector. The
    // key is clamped to q = (p-1)/2.
    dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
    ASSERT_TRUE(dh);
    DH_set_length(dh.get(), 10000000);
    ASSERT_TRUE(DH_generate_key(dh.get()));
    EXPECT_LT(BN_cmp(DH_get0_priv_key(dh.get()), q.get()), 0);
    EXPECT_LE(BN_num_bits(q.get()) - kMaxLeadingZeros,
              BN_num_bits(DH_get0_priv_key(dh.get())));

    // A small private key size should bound the private key.
    dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
    ASSERT_TRUE(dh);
    unsigned bits = 1024;
    DH_set_length(dh.get(), bits);
    ASSERT_TRUE(DH_generate_key(dh.get()));
    EXPECT_LE(BN_num_bits(DH_get0_priv_key(dh.get())), bits);
    EXPECT_LE(bits - kMaxLeadingZeros, BN_num_bits(DH_get0_priv_key(dh.get())));

    // If the private key length is num_bits(q) - 1, the length should be the
    // limiting factor.
    dh = NewDHGroup(p.get(), /*q=*/nullptr, g.get());
    ASSERT_TRUE(dh);
    bits = BN_num_bits(q.get()) - 1;
    DH_set_length(dh.get(), bits);
    ASSERT_TRUE(DH_generate_key(dh.get()));
    EXPECT_LE(BN_num_bits(DH_get0_priv_key(dh.get())), bits);
    EXPECT_LE(bits - kMaxLeadingZeros, BN_num_bits(DH_get0_priv_key(dh.get())));
  }
}
