/* Copyright (c) 2023, Google Inc.
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

#include <optional>
#include <string>
#include <string_view>

#include <openssl/pki/certificate.h>
#include <gmock/gmock.h>

#include "string_util.h"
#include "test_helpers.h"

TEST(CertificateTest, FromPEM) {
  std::optional<std::unique_ptr<bssl::Certificate>> cert(
      bssl::Certificate::FromPEM("nonsense"));
  EXPECT_FALSE(cert.has_value());

  cert = bssl::Certificate::FromPEM(
      bssl::ReadTestFileToString("testdata/certificate_test/self-issued.pem"));
  EXPECT_TRUE(cert);
}

TEST(CertificateTest, IsSelfIssued) {
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/certificate_test/google-leaf.der");
  std::optional<std::unique_ptr<bssl::Certificate>> leaf_cert(
      bssl::Certificate::FromDER(leaf));
  EXPECT_TRUE(leaf_cert);
  EXPECT_FALSE(leaf_cert.value()->IsSelfIssued());

  const std::string self_issued =
      bssl::ReadTestFileToString("testdata/certificate_test/self-issued.pem");
  std::optional<std::unique_ptr<bssl::Certificate>> self_issued_cert(
      bssl::Certificate::FromPEM(self_issued));
  EXPECT_TRUE(self_issued_cert);
  EXPECT_TRUE(self_issued_cert.value()->IsSelfIssued());
}

TEST(CertificateTest, Validity) {
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/certificate_test/google-leaf.der");
  std::optional<std::unique_ptr<bssl::Certificate>> cert(
      bssl::Certificate::FromDER(leaf));
  EXPECT_TRUE(cert);

  bssl::Certificate::Validity validity = cert.value()->GetValidity();
  EXPECT_EQ(validity.not_before, 1498644466);
  EXPECT_EQ(validity.not_after, 1505899620);
}

TEST(CertificateTest, SerialNumber) {
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/certificate_test/google-leaf.der");
  std::optional<std::unique_ptr<bssl::Certificate>> cert(
      bssl::Certificate::FromDER(leaf));
  EXPECT_TRUE(cert);
  EXPECT_STREQ(
      (bssl::string_util::HexEncode(cert.value()->GetSerialNumber().data(),
                                    cert.value()->GetSerialNumber().size()))
          .c_str(),
      "0118F044A8F31892");
}
