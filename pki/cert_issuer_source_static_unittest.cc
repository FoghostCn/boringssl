// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cert_issuer_source_static.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "cert_issuer_source_sync_unittest.h"
#include "parsed_certificate.h"

namespace bssl {

namespace {

class CertIssuerSourceStaticTestDelegate {
 public:
  void AddCert(std::shared_ptr<const ParsedCertificate> cert) {
    source_.AddCert(std::move(cert));
  }

  CertIssuerSource &source() { return source_; }

 protected:
  CertIssuerSourceStatic source_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(CertIssuerSourceStaticSyncTest,
                               CertIssuerSourceSyncTest,
                               CertIssuerSourceStaticTestDelegate);

INSTANTIATE_TYPED_TEST_SUITE_P(CertIssuerSourceStaticNormalizationTest,
                               CertIssuerSourceSyncNormalizationTest,
                               CertIssuerSourceStaticTestDelegate);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CertIssuerSourceSyncNotNormalizedTest);

TEST(CertIssuerSourceStaticTest, AddCertsGetCertsAndClear) {
  std::string test_dir = "testdata/cert_issuer_source_static_unittest/";
  std::shared_ptr<const ParsedCertificate> cert1 =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(cert1);
  std::shared_ptr<const ParsedCertificate> cert2 =
      ReadCertFromFile(test_dir + "i1_1.pem");
  ASSERT_TRUE(cert2);
  std::shared_ptr<const ParsedCertificate> cert3 =
      ReadCertFromFile(test_dir + "i1_2.pem");
  ASSERT_TRUE(cert3);

  CertIssuerSourceStatic source;
  EXPECT_TRUE(source.Certs().empty());
  EXPECT_EQ(source.size(), 0u);

  source.AddCert(cert1);
  EXPECT_THAT(source.Certs(), testing::UnorderedElementsAre(cert1));
  EXPECT_EQ(source.size(), 1u);

  source.AddCert(cert2);
  EXPECT_THAT(source.Certs(), testing::UnorderedElementsAre(cert1, cert2));
  EXPECT_EQ(source.size(), 2u);

  source.AddCert(cert3);
  EXPECT_THAT(source.Certs(),
              testing::UnorderedElementsAre(cert1, cert2, cert3));
  EXPECT_EQ(source.size(), 3u);

  source.Clear();
  EXPECT_TRUE(source.Certs().empty());
  EXPECT_EQ(source.size(), 0u);
}

}  // namespace

}  // namespace bssl
