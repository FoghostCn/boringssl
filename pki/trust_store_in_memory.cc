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

#include "trust_store_in_memory.h"

namespace bssl {

TrustStoreInMemory::TrustStoreInMemory() = default;
TrustStoreInMemory::~TrustStoreInMemory() = default;

bool TrustStoreInMemory::IsEmpty() const { return entries_.empty(); }

void TrustStoreInMemory::Clear() { entries_.clear(); }

void TrustStoreInMemory::AddTrustAnchor(
    std::shared_ptr<const ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForTrustAnchor());
}

void TrustStoreInMemory::AddTrustAnchorWithExpiration(
    std::shared_ptr<const ParsedCertificate> cert) {
  AddCertificate(std::move(cert),
                 CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry());
}

void TrustStoreInMemory::AddTrustAnchorWithConstraints(
    std::shared_ptr<const ParsedCertificate> cert) {
  AddCertificate(
      std::move(cert),
      CertificateTrust::ForTrustAnchor().WithEnforceAnchorConstraints());
}

void TrustStoreInMemory::AddDistrustedCertificateForTest(
    std::shared_ptr<const ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForDistrusted());
}

void TrustStoreInMemory::AddDistrustedCertificateBySPKI(std::string spki) {
  distrusted_spkis_.insert(std::move(spki));
}

void TrustStoreInMemory::AddCertificateWithUnspecifiedTrust(
    std::shared_ptr<const ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForUnspecified());
}

void TrustStoreInMemory::SyncGetIssuersOf(const ParsedCertificate *cert,
                                          ParsedCertificateList *issuers) {
  auto range = entries_.equal_range(cert->normalized_issuer().AsStringView());
  for (auto it = range.first; it != range.second; ++it) {
    issuers->push_back(it->second.cert);
  }
}

CertificateTrust TrustStoreInMemory::GetTrust(const ParsedCertificate *cert) {
  // Check SPKI distrust first.
  if (distrusted_spkis_.find(cert->tbs().spki_tlv.AsString()) !=
      distrusted_spkis_.end()) {
    return CertificateTrust::ForDistrusted();
  }

  const Entry *entry = GetEntry(cert);
  return entry ? entry->trust : CertificateTrust::ForUnspecified();
}

bool TrustStoreInMemory::Contains(const ParsedCertificate *cert) const {
  return GetEntry(cert) != nullptr;
}

TrustStoreInMemory::Entry::Entry() = default;
TrustStoreInMemory::Entry::Entry(const Entry &other) = default;
TrustStoreInMemory::Entry::~Entry() = default;

void TrustStoreInMemory::AddCertificate(
    std::shared_ptr<const ParsedCertificate> cert,
    const CertificateTrust &trust) {
  Entry entry;
  entry.cert = std::move(cert);
  entry.trust = trust;

  // TODO(mattm): should this check for duplicate certificates?
  entries_.insert(
      std::make_pair(entry.cert->normalized_subject().AsStringView(), entry));
}

const TrustStoreInMemory::Entry *TrustStoreInMemory::GetEntry(
    const ParsedCertificate *cert) const {
  auto range = entries_.equal_range(cert->normalized_subject().AsStringView());
  for (auto it = range.first; it != range.second; ++it) {
    if (cert == it->second.cert.get() ||
        cert->der_cert() == it->second.cert->der_cert()) {
      // NOTE: ambiguity when there are duplicate entries.
      return &it->second;
    }
  }
  return nullptr;
}

}  // namespace bssl
