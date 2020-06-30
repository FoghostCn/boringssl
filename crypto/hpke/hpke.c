/* Copyright (c) 2020, Google Inc.
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

#include <assert.h>
#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <string.h>

#include "../internal.h"
#include "internal.h"


#define KEM_CONTEXT_LEN (2 * X25519_PUBLIC_VALUE_LEN)

// HPKE KEM scheme IDs.
#define EVP_HPKE_DHKEM_X25519_HKDF_SHA256 0x0020

// TODO(dmcardle): Delete this function and calls when ready to commit.
static void PrintActual(const char *debug_label, const uint8_t *data,
                        size_t data_len) {
  printf("actual [%zu bytes] %s:\n", data_len, debug_label);
  for (size_t i = 0; i < data_len; i++) {
    printf("%.2x", data[i]);
  }
  printf("\n");
}

static const char kHpkeRfcId[] = "RFCXXXX ";

static int add_label_string(CBB *cbb, const char *label) {
  return CBB_add_bytes(cbb, (const uint8_t *)label, strlen(label));
}

typedef enum evp_hpke_mode {
  EVP_HPKE_MODE_BASE = 0,  // We only support |HPKE_MODE_BASE|.
  EVP_HPKE_MODE_PSK = 1,
  EVP_HPKE_MODE_AUTH = 2,
  EVP_HPKE_MODE_AUTH_PSK = 3,
} evp_hpke_mode;

static int hpke_labeled_extract(const EVP_MD *hkdf_md, uint8_t *out_key,
                                size_t *out_len, const uint8_t *salt,
                                size_t salt_len, const uint8_t *label,
                                size_t label_len, const uint8_t *ikm,
                                size_t ikm_len) {
  CBB labeled_ikm;
  int ok = (CBB_init(&labeled_ikm, strlen(kHpkeRfcId) + label_len + ikm_len) &&
            add_label_string(&labeled_ikm, kHpkeRfcId) &&
            CBB_add_bytes(&labeled_ikm, label, label_len) &&
            CBB_add_bytes(&labeled_ikm, ikm, ikm_len) &&
            HKDF_extract(out_key, out_len, hkdf_md, CBB_data(&labeled_ikm),
                         CBB_len(&labeled_ikm), salt, salt_len));
  CBB_cleanup(&labeled_ikm);
  return ok;
}

static int hpke_labeled_expand(const EVP_MD *hkdf_md, uint8_t *out_key,
                               size_t out_len, const uint8_t *prk,
                               size_t prk_len, const uint8_t *label,
                               size_t label_len, const uint8_t *info,
                               size_t info_len) {
  CBB labeled_info;
  int ok = (CBB_init(&labeled_info, sizeof(uint16_t) + strlen(kHpkeRfcId) +
                                        label_len + info_len) &&
            CBB_add_u16(&labeled_info, out_len) &&
            add_label_string(&labeled_info, kHpkeRfcId) &&
            CBB_add_bytes(&labeled_info, label, label_len) &&
            CBB_add_bytes(&labeled_info, info, info_len) &&
            HKDF_expand(out_key, out_len, hkdf_md, prk, prk_len,
                        CBB_data(&labeled_info), CBB_len(&labeled_info)));
  CBB_cleanup(&labeled_info);
  return ok;
}

static int hpke_extract_and_expand(const EVP_MD *hkdf_md, uint8_t *out_key,
                                   size_t out_len,
                                   const uint8_t dh[X25519_PUBLIC_VALUE_LEN],
                                   const uint8_t kem_context[KEM_CONTEXT_LEN]) {
  int ret = 0;
  CBB prk_label_cbb;
  if (!CBB_init(&prk_label_cbb, 0) ||
      !CBB_add_u16(&prk_label_cbb, EVP_HPKE_DHKEM_X25519_HKDF_SHA256) ||
      !add_label_string(&prk_label_cbb, "eae_prk")) {
    goto err;
  }

  uint8_t prk[EVP_MAX_MD_SIZE];
  size_t prk_len;
  if (!hpke_labeled_extract(hkdf_md, prk, &prk_len, NULL, 0,
                            CBB_data(&prk_label_cbb), CBB_len(&prk_label_cbb),
                            dh, X25519_PUBLIC_VALUE_LEN)) {
    goto err;
  }
  const char kPRKExpandLabel[] = "zz";
  if (!hpke_labeled_expand(hkdf_md, out_key, out_len, prk, prk_len,
                           (const uint8_t *)kPRKExpandLabel,
                           strlen(kPRKExpandLabel), kem_context,
                           KEM_CONTEXT_LEN)) {
    goto err;
  }

  ret = 1;
err:
  CBB_cleanup(&prk_label_cbb);
  return ret;
}

int EVP_HPKE_derive_x25519_keypair(uint8_t out_private[X25519_PRIVATE_KEY_LEN],
                                   uint8_t out_public[X25519_PUBLIC_VALUE_LEN],
                                   const uint8_t *ikm, size_t ikm_len) {
  int ret = 0;

  // Build the label for extracting dkp_prk.
  CBB label_cbb;
  if (!CBB_init(&label_cbb, 0) ||
      !CBB_add_u16(&label_cbb, EVP_HPKE_DHKEM_X25519_HKDF_SHA256) ||
      !add_label_string(&label_cbb, "dkp_prk")) {
    goto err;
  }

  const char *kSkExpandLabel = "sk";
  uint8_t dkp_prk[EVP_MAX_MD_SIZE];
  size_t dkp_prk_len;
  if (!hpke_labeled_extract(EVP_sha256(), dkp_prk, &dkp_prk_len, NULL, 0,
                            CBB_data(&label_cbb), CBB_len(&label_cbb), ikm,
                            ikm_len) ||
      !hpke_labeled_expand(EVP_sha256(), out_private, X25519_PRIVATE_KEY_LEN,
                           dkp_prk, dkp_prk_len,
                           (const uint8_t *)kSkExpandLabel,
                           strlen(kSkExpandLabel), NULL, 0)) {
    goto err;
  }

  X25519_public_from_private(out_public, out_private);
  ret = 1;
err:
  CBB_cleanup(&label_cbb);
  return ret;
}

static const EVP_AEAD *hpke_get_aead(uint16_t aead_id) {
  switch (aead_id) {
    case HPKE_AEAD_AES_GCM_128:
      return EVP_aead_aes_128_gcm();
    case HPKE_AEAD_AES_GCM_256:
      return EVP_aead_aes_256_gcm();
    case HPKE_AEAD_CHACHA20POLY1305:
      return EVP_aead_chacha20_poly1305();
  }
  return NULL;
}

static int hpke_get_kdf(uint16_t kdf_id, const EVP_MD **out_md) {
  switch (kdf_id) {
    case HPKE_HKDF_SHA256:
      *out_md = EVP_sha256();
      return 1;
    case HPKE_HKDF_SHA384:
      *out_md = EVP_sha384();
      return 1;
    case HPKE_HKDF_SHA512:
      *out_md = EVP_sha512();
      return 1;
    default:
      return 0;
  }
}

static int hpke_key_schedule(EVP_HPKE_CTX *hpke, uint16_t kdf_id,
                             uint16_t aead_id,
                             const uint8_t zz[EVP_MAX_MD_SIZE], size_t zz_len,
                             const uint8_t *info, size_t info_len) {
  // Attempt to get an EVP_AEAD*.
  const EVP_AEAD *aead = hpke_get_aead(aead_id);
  if (aead == NULL) {
    return 0;
  }

  // pskID_hash = LabeledExtract(zero(Nh), "pskID_hash", pskID)
  static const char kPskIdHashLabel[] = "pskID_hash";
  uint8_t psk_id_hash[EVP_MAX_MD_SIZE];
  size_t psk_id_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, psk_id_hash, &psk_id_hash_len, NULL,
                            0, (const uint8_t *)kPskIdHashLabel,
                            strlen(kPskIdHashLabel), NULL, 0)) {
    return 0;
  }

  // info_hash = LabeledExtract(zero(Nh), "info", info)

  // TODO(dmcardle) draft-draft-irtf-cfrg-hpke-04 says "info", but reference
  // implementation uses "info_hash"
  static const char kInfoHashLabel[] = "info_hash";
  uint8_t info_hash[EVP_MAX_MD_SIZE];
  size_t info_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, info_hash, &info_hash_len, NULL, 0,
                            (const uint8_t *)kInfoHashLabel,
                            strlen(kInfoHashLabel), info, info_len)) {
    return 0;
  }

  // context = concat(ciphersuite, mode, pskID_hash, info_hash)
  uint8_t context[3 * sizeof(uint16_t) + sizeof(uint8_t) + 2 * EVP_MAX_MD_SIZE];
  CBB context_cbb;

  PrintActual("psk_id_hash", psk_id_hash, psk_id_hash_len);
  PrintActual("info_hash", info_hash, info_hash_len);

  int ret = 0;
  if (!CBB_init_fixed(&context_cbb, context, sizeof(context)) ||
      !CBB_add_u16(&context_cbb, EVP_HPKE_DHKEM_X25519_HKDF_SHA256) ||
      !CBB_add_u16(&context_cbb, kdf_id) ||
      !CBB_add_u16(&context_cbb, aead_id) ||
      !CBB_add_u8(&context_cbb, EVP_HPKE_MODE_BASE) ||
      !CBB_add_bytes(&context_cbb, psk_id_hash, psk_id_hash_len) ||
      !CBB_add_bytes(&context_cbb, info_hash, info_hash_len)) {
    goto err;
  }
  PrintActual("context", CBB_data(&context_cbb), CBB_len(&context_cbb));

  // psk = LabeledExtract(zero(Nh), "psk_hash", psk)
  //
  // Confusingly, the HPKE draft shadows the `psk` parameter here. For our
  // purposes, that parameter is just the default empty PSK.
  static const char kPskHashLabel[] = "psk_hash";
  uint8_t psk_hash[EVP_MAX_MD_SIZE];
  size_t psk_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, psk_hash, &psk_hash_len, NULL, 0,
                            (const uint8_t *)kPskHashLabel,
                            strlen(kPskHashLabel), NULL, 0)) {
    goto err;
  }

  PrintActual("psk_hash", psk_hash, psk_hash_len);
  PrintActual("zz", zz, zz_len);

  // secret = LabeledExtract(psk, "secret", zz)
  static const char kSecretExtractLabel[] = "secret";
  uint8_t secret[EVP_MAX_MD_SIZE];
  size_t secret_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, secret, &secret_len, psk_hash,
                            psk_hash_len, (const uint8_t *)kSecretExtractLabel,
                            strlen(kSecretExtractLabel), zz, zz_len)) {
    goto err;
  }

  PrintActual("secret", secret, secret_len);
  // key = LabeledExpand(secret, "key", context, Nk)
  static const char kKeyExpandLabel[] = "key";
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  const size_t kKeyLen = EVP_AEAD_key_length(aead);
  if (!hpke_labeled_expand(hpke->hkdf_md, key, kKeyLen, secret, secret_len,
                           (const uint8_t *)kKeyExpandLabel,
                           strlen(kKeyExpandLabel), CBB_data(&context_cbb),
                           CBB_len(&context_cbb))) {
    goto err;
  }
  PrintActual("key", key, kKeyLen);

  // Initialize the HPKE context's AEAD context, storing a copy of |key|.
  if (!EVP_AEAD_CTX_init(&hpke->aead_ctx, aead, key, kKeyLen, 0, NULL)) {
    goto err;
  }

  // nonce = LabeledExpand(secret, "nonce", context, Nn)
  static const char kNonceExpandLabel[] = "nonce";
  if (!hpke_labeled_expand(hpke->hkdf_md, hpke->nonce,
                           EVP_AEAD_nonce_length(aead), secret, secret_len,
                           (const uint8_t *)kNonceExpandLabel,
                           strlen(kNonceExpandLabel), CBB_data(&context_cbb),
                           CBB_len(&context_cbb))) {
    goto err;
  }

  PrintActual("nonce", hpke->nonce, EVP_AEAD_nonce_length(aead));

  // exporter_secret = LabeledExpand(secret, "exp", context, Nh)
  static const char kExporterSecretExpandLabel[] = "exp";
  if (!hpke_labeled_expand(hpke->hkdf_md, hpke->exporter_secret,
                           EVP_MD_size(hpke->hkdf_md), secret, secret_len,
                           (const uint8_t *)kExporterSecretExpandLabel,
                           strlen(kExporterSecretExpandLabel),
                           CBB_data(&context_cbb), CBB_len(&context_cbb))) {
    goto err;
  }

  ret = 1;
err:
  CBB_cleanup(&context_cbb);
  return ret;
}

static int hpke_increment_seq(EVP_HPKE_CTX *hpke) {
  hpke->seq++;
  return hpke->seq != 0;
}

static void hpke_ephemeral_keypair_set(
    EVP_HPKE_CTX *hpke, const uint8_t private[X25519_PRIVATE_KEY_LEN]) {
  OPENSSL_memcpy(hpke->secret_key_ephemeral, private, X25519_PRIVATE_KEY_LEN);
  hpke->secret_key_ephemeral_len = X25519_PRIVATE_KEY_LEN;
}

static int hpke_ephemeral_keypair_get(EVP_HPKE_CTX *hpke, uint8_t *out_public,
                                      uint8_t *out_private) {
  if (hpke->secret_key_ephemeral_len > 0) {
    // Recover the public key from |hpke|.
    X25519_public_from_private(out_public, hpke->secret_key_ephemeral);
    OPENSSL_memcpy(out_private, hpke->secret_key_ephemeral,
                   X25519_PRIVATE_KEY_LEN);
    return 1;
  }

  uint8_t public_value_ignored[X25519_PUBLIC_VALUE_LEN];
  uint8_t private_key_ikm[X25519_PRIVATE_KEY_LEN];
  X25519_keypair(public_value_ignored, private_key_ikm);

  if (!EVP_HPKE_derive_x25519_keypair(out_private, out_public, private_key_ikm,
                                      sizeof(private_key_ikm))) {
    return 0;
  }
  // Save a copy of the secret key in |hpke|.
  hpke_ephemeral_keypair_set(hpke, out_private);
  return 1;
}

// The number of bytes written to |out_zz| is the size of the KEM's KDF
// (currently we only support SHA256).
static int hpke_encap(EVP_HPKE_CTX *hpke, uint8_t out_zz[EVP_MAX_MD_SIZE],
                      uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
                      const uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN]) {
  uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN];
  if (!hpke_ephemeral_keypair_get(hpke, out_enc, ephemeral_private)) {
    return 0;
  }

  uint8_t dh[X25519_PUBLIC_VALUE_LEN];
  if (!X25519(dh, ephemeral_private, public_key_r)) {
    return 0;
  }

  uint8_t kem_context[KEM_CONTEXT_LEN];
  OPENSSL_memcpy(kem_context, out_enc, X25519_PUBLIC_VALUE_LEN);
  OPENSSL_memcpy(kem_context + X25519_PUBLIC_VALUE_LEN, public_key_r,
                 X25519_PUBLIC_VALUE_LEN);
  if (!hpke_extract_and_expand(EVP_sha256(), out_zz, EVP_MD_size(EVP_sha256()),
                               dh, kem_context)) {
    return 0;
  }
  return 1;
}

static int hpke_decap(const EVP_HPKE_CTX *hpke, uint8_t out_zz[EVP_MAX_MD_SIZE],
                      const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
                      const uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN]) {
  uint8_t dh[X25519_PUBLIC_VALUE_LEN];
  if (!X25519(dh, secret_key_r, enc)) {
    return 0;
  }
  uint8_t public_key[X25519_PUBLIC_VALUE_LEN];
  X25519_public_from_private(public_key, secret_key_r);
  uint8_t kem_context[KEM_CONTEXT_LEN];
  OPENSSL_memcpy(kem_context, enc, X25519_PUBLIC_VALUE_LEN);
  OPENSSL_memcpy(kem_context + X25519_PUBLIC_VALUE_LEN, public_key,
                 sizeof(public_key));
  if (!hpke_extract_and_expand(EVP_sha256(), out_zz, EVP_MD_size(EVP_sha256()),
                               dh, kem_context)) {
    return 0;
  }
  return 1;
}

void EVP_HPKE_CTX_init(EVP_HPKE_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(EVP_HPKE_CTX));
  EVP_AEAD_CTX_zero(&ctx->aead_ctx);
}

void EVP_HPKE_CTX_cleanup(EVP_HPKE_CTX *ctx) {
  EVP_AEAD_CTX_cleanup(&ctx->aead_ctx);
}

void EVP_HPKE_CTX_free(EVP_HPKE_CTX *ctx) {
  EVP_HPKE_CTX_cleanup(ctx);
  OPENSSL_free(ctx);
}

int EVP_HPKE_CTX_setup_base_x25519_s(
    EVP_HPKE_CTX *hpke, uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
    uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len) {
  if (!hpke_get_kdf(kdf_id, &hpke->hkdf_md)) {
    return 0;
  }
  uint8_t zz[EVP_MAX_MD_SIZE];
  if (!hpke_encap(hpke, zz, out_enc, peer_public_value) ||
      !hpke_key_schedule(hpke, kdf_id, aead_id, zz, EVP_MD_size(EVP_sha256()),
                         info, info_len)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_setup_base_x25519_r(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
    const uint8_t private_key[X25519_PRIVATE_KEY_LEN], const uint8_t *info,
    size_t info_len) {
  if (!hpke_get_kdf(kdf_id, &hpke->hkdf_md)) {
    return 0;
  }
  uint8_t zz[EVP_MAX_MD_SIZE];
  if (!hpke_decap(hpke, zz, enc, private_key) ||
      !hpke_key_schedule(hpke, kdf_id, aead_id, zz, EVP_MD_size(EVP_sha256()),
                         info, info_len)) {
    return 0;
  }
  return 1;
}

static void hpke_nonce(EVP_HPKE_CTX *hpke, uint8_t *out_nonce,
                       size_t nonce_len) {
  assert(nonce_len >= 8);

  // Write padded big-endian bytes of |hpke->seq| to |out_nonce|.
  OPENSSL_memset(out_nonce, 0, nonce_len);
  uint64_t seq_copy = hpke->seq;
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    out_nonce[nonce_len - i - 1] = seq_copy & 0xff;
    seq_copy >>= 8;
  }

  // XOR the encoded sequence with the |hpke->nonce|.
  for (size_t i = 0; i < nonce_len; i++) {
    out_nonce[i] ^= hpke->nonce[i];
  }
}

int EVP_HPKE_CTX_open(EVP_HPKE_CTX *hpke, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(hpke->aead_ctx.aead);
  hpke_nonce(hpke, nonce, nonce_len);

  if (!EVP_AEAD_CTX_open(&hpke->aead_ctx, out, out_len, max_out_len, nonce,
                         nonce_len, in, in_len, ad, ad_len) ||
      !hpke_increment_seq(hpke)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_seal(EVP_HPKE_CTX *hpke, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(hpke->aead_ctx.aead);
  hpke_nonce(hpke, nonce, nonce_len);

  if (!EVP_AEAD_CTX_seal(&hpke->aead_ctx, out, out_len, max_out_len, nonce,
                         EVP_AEAD_nonce_length(hpke->aead_ctx.aead), in, in_len,
                         ad, ad_len) ||
      !hpke_increment_seq(hpke)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_export(const EVP_HPKE_CTX *hpke, uint8_t *secret_out,
                        size_t secret_len, const uint8_t *context,
                        size_t context_len) {
  static const char kExportExpandLabel[] = "sec";
  if (!hpke_labeled_expand(hpke->hkdf_md, secret_out, secret_len,
                           hpke->exporter_secret, EVP_MD_size(hpke->hkdf_md),
                           (const uint8_t *)kExportExpandLabel,
                           strlen(kExportExpandLabel), context, context_len)) {
    return 0;
  }
  return 1;
}
