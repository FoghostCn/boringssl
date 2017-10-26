/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>
#include <openssl/err.h>
#include <openssl/nid.h>

#include "../test/file_test.h"
#include "../test/test_util.h"


static bssl::UniquePtr<EC_GROUP> GetCurve(FileTest *t, const char *key) {
  std::string curve_name;
  if (!t->GetAttribute(&curve_name, key)) {
    return nullptr;
  }

  if (curve_name == "P-224") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp224r1));
  }
  if (curve_name == "P-256") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(
        NID_X9_62_prime256v1));
  }
  if (curve_name == "P-384") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp384r1));
  }
  if (curve_name == "P-521") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp521r1));
  }

  t->PrintLine("Unknown curve '%s'", curve_name.c_str());
  return nullptr;
}

static bssl::UniquePtr<BIGNUM> GetBIGNUM(FileTest *t, const char *key) {
  std::vector<uint8_t> bytes;
  if (!t->GetBytes(&bytes, key)) {
    return nullptr;
  }

  return bssl::UniquePtr<BIGNUM>(BN_bin2bn(bytes.data(), bytes.size(), nullptr));
}

TEST(ECDHTest, TestVectors) {
  FileTestGTest("crypto/ecdh/ecdh_tests.txt", [](FileTest *t) {
    bssl::UniquePtr<EC_GROUP> group = GetCurve(t, "Curve");
    ASSERT_TRUE(group);
    bssl::UniquePtr<BIGNUM> priv_key = GetBIGNUM(t, "Private");
    ASSERT_TRUE(priv_key);
    bssl::UniquePtr<BIGNUM> x = GetBIGNUM(t, "X");
    ASSERT_TRUE(x);
    bssl::UniquePtr<BIGNUM> y = GetBIGNUM(t, "Y");
    ASSERT_TRUE(y);
    bssl::UniquePtr<BIGNUM> peer_x = GetBIGNUM(t, "PeerX");
    ASSERT_TRUE(peer_x);
    bssl::UniquePtr<BIGNUM> peer_y = GetBIGNUM(t, "PeerY");
    ASSERT_TRUE(peer_y);
    std::vector<uint8_t> z;
    ASSERT_TRUE(t->GetBytes(&z, "Z"));

    bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
    ASSERT_TRUE(key);
    bssl::UniquePtr<EC_POINT> pub_key(EC_POINT_new(group.get()));
    ASSERT_TRUE(pub_key);
    bssl::UniquePtr<EC_POINT> peer_pub_key(EC_POINT_new(group.get()));
    ASSERT_TRUE(peer_pub_key);
    ASSERT_TRUE(EC_KEY_set_group(key.get(), group.get()));
    ASSERT_TRUE(EC_KEY_set_private_key(key.get(), priv_key.get()));
    ASSERT_TRUE(EC_POINT_set_affine_coordinates_GFp(group.get(), pub_key.get(),
                                                    x.get(), y.get(), nullptr));
    ASSERT_TRUE(EC_POINT_set_affine_coordinates_GFp(
        group.get(), peer_pub_key.get(), peer_x.get(), peer_y.get(), nullptr));
    ASSERT_TRUE(EC_KEY_set_public_key(key.get(), pub_key.get()));
    ASSERT_TRUE(EC_KEY_check_key(key.get()));

    std::vector<uint8_t> actual_z;
    // Make |actual_z| larger than expected to ensure |ECDH_compute_key| returns
    // the right amount of data.
    actual_z.resize(z.size() + 1);
    int ret = ECDH_compute_key(actual_z.data(), actual_z.size(),
                               peer_pub_key.get(), key.get(), nullptr);
    ASSERT_GE(ret, 0);
    EXPECT_EQ(Bytes(z), Bytes(actual_z.data(), static_cast<size_t>(ret)));

    // Test |ECDH_compute_key| truncates.
    actual_z.resize(z.size() - 1);
    ret = ECDH_compute_key(actual_z.data(), actual_z.size(), peer_pub_key.get(),
                           key.get(), nullptr);
    ASSERT_GE(ret, 0);
    EXPECT_EQ(Bytes(z.data(), z.size() - 1),
              Bytes(actual_z.data(), static_cast<size_t>(ret)));
  });
}

// MakeCustomGroup returns an |EC_GROUP| containing a non-standard group. (P-256
// with the wrong generator.)
static bssl::UniquePtr<EC_GROUP> MakeCustomGroup() {
  static const uint8_t kP[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  static const uint8_t kA[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
  };
  static const uint8_t kB[] = {
      0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd,
      0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53,
      0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b,
  };
  static const uint8_t kX[] = {
      0xe6, 0x2b, 0x69, 0xe2, 0xbf, 0x65, 0x9f, 0x97, 0xbe, 0x2f, 0x1e,
      0x0d, 0x94, 0x8a, 0x4c, 0xd5, 0x97, 0x6b, 0xb7, 0xa9, 0x1e, 0x0d,
      0x46, 0xfb, 0xdd, 0xa9, 0xa9, 0x1e, 0x9d, 0xdc, 0xba, 0x5a,
  };
  static const uint8_t kY[] = {
      0x01, 0xe7, 0xd6, 0x97, 0xa8, 0x0a, 0x18, 0xf9, 0xc3, 0xc4, 0xa3,
      0x1e, 0x56, 0xe2, 0x7c, 0x83, 0x48, 0xdb, 0x16, 0x1a, 0x1c, 0xf5,
      0x1d, 0x7e, 0xf1, 0x94, 0x2d, 0x4b, 0xcf, 0x72, 0x22, 0xc1,
  };
  static const uint8_t kOrder[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
      0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51,
  };
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  bssl::UniquePtr<BIGNUM> p(BN_bin2bn(kP, sizeof(kP), nullptr));
  bssl::UniquePtr<BIGNUM> a(BN_bin2bn(kA, sizeof(kA), nullptr));
  bssl::UniquePtr<BIGNUM> b(BN_bin2bn(kB, sizeof(kB), nullptr));
  bssl::UniquePtr<BIGNUM> x(BN_bin2bn(kX, sizeof(kX), nullptr));
  bssl::UniquePtr<BIGNUM> y(BN_bin2bn(kY, sizeof(kY), nullptr));
  bssl::UniquePtr<BIGNUM> order(BN_bin2bn(kOrder, sizeof(kOrder), nullptr));
  if (!ctx || !p || !a || !b || !x || !y || !order) {
    return nullptr;
  }
  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_curve_GFp(p.get(), a.get(), b.get(), ctx.get()));
  if (!group) {
    return nullptr;
  }
  bssl::UniquePtr<EC_POINT> generator(EC_POINT_new(group.get()));
  if (!generator ||
      !EC_POINT_set_affine_coordinates_GFp(group.get(), generator.get(),
                                           x.get(), y.get(), ctx.get()) ||
      !EC_GROUP_set_generator(group.get(), generator.get(), order.get(),
                              BN_value_one())) {
    return nullptr;
  }
  return group;
}

TEST(ECDHTest, GroupMismatch) {
  const size_t num_curves = EC_get_builtin_curves(nullptr, 0);
  std::vector<EC_builtin_curve> curves(num_curves);
  EC_get_builtin_curves(curves.data(), num_curves);

  // Instantiate all the built-in curves.
  std::vector<bssl::UniquePtr<EC_GROUP>> groups;
  for (const auto &curve : curves) {
    groups.emplace_back(EC_GROUP_new_by_curve_name(curve.nid));
    ASSERT_TRUE(groups.back());
  }

  // Also create some arbitrary group. (This is P-256 with the wrong generator.)
  groups.push_back(MakeCustomGroup());
  ASSERT_TRUE(groups.back());

  for (const auto &a : groups) {
    for (const auto &b : groups) {
      if (a.get() == b.get()) {
        continue;
      }

      bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
      ASSERT_TRUE(EC_KEY_set_group(key.get(), a.get()));
      ASSERT_TRUE(EC_KEY_generate_key(key.get()));

      // ECDH across the groups should not work.
      char out[64];
      const EC_POINT *peer = EC_GROUP_get0_generator(b.get());
      EXPECT_EQ(-1,
                ECDH_compute_key(out, sizeof(out), peer, key.get(), nullptr));
      ERR_clear_error();
    }
  }
}
