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

#include <openssl/digest.h>

#include <assert.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/sha.h>

#include "internal.h"
#include "../../internal.h"

#if defined(NDEBUG)
#define CHECK(x) (void) (x)
#else
#define CHECK(x) assert(x)
#endif

#if 0
static void md4_init(EVP_MD_CTX *ctx) {
  CHECK(MD4_Init(ctx->md_data));
}

static void md4_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(MD4_Update(ctx->md_data, data, count));
}

static void md4_final(EVP_MD_CTX *ctx, uint8_t *out) {
  CHECK(MD4_Final(out, ctx->md_data));
}

static const EVP_MD md4_md = {
    NID_md4,    MD4_DIGEST_LENGTH, 0 /* flags */,       md4_init,
    md4_update, md4_final,         64 /* block size */, sizeof(MD4_CTX),
};

const EVP_MD *EVP_md4(void) { return &md4_md; }


static void md5_init(EVP_MD_CTX *ctx) {
  CHECK(MD5_Init(ctx->md_data));
}

static void md5_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(MD5_Update(ctx->md_data, data, count));
}

static void md5_final(EVP_MD_CTX *ctx, uint8_t *out) {
  CHECK(MD5_Final(out, ctx->md_data));
}

static const EVP_MD md5_md = {
    NID_md5,    MD5_DIGEST_LENGTH, 0 /* flags */,       md5_init,
    md5_update, md5_final,         64 /* block size */, sizeof(MD5_CTX),
};

const EVP_MD *EVP_md5(void) { return &md5_md; }


static void sha1_init(EVP_MD_CTX *ctx) {
  CHECK(SHA1_Init(ctx->md_data));
}

static void sha1_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(SHA1_Update(ctx->md_data, data, count));
}

static void sha1_final(EVP_MD_CTX *ctx, uint8_t *md) {
  CHECK(SHA1_Final(md, ctx->md_data));
}

static const EVP_MD sha1_md = {
    NID_sha1,    SHA_DIGEST_LENGTH, 0 /* flags */,       sha1_init,
    sha1_update, sha1_final,        64 /* block size */, sizeof(SHA_CTX),
};

const EVP_MD *EVP_sha1(void) { return &sha1_md; }
#endif

static void sha224_init(EVP_MD_CTX *ctx) {
  CHECK(SHA224_Init(ctx->md_data));
}

static void sha224_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(SHA224_Update(ctx->md_data, data, count));
}

static void sha224_final(EVP_MD_CTX *ctx, uint8_t *md) {
  CHECK(SHA224_Final(md, ctx->md_data));
}

static EVP_MD sha224_md __attribute__((used));
static CRYPTO_once_t sha224_md_once __attribute__((used)) = CRYPTO_ONCE_INIT;

EVP_MD *sha224_md_bss_get(void);
CRYPTO_once_t *sha224_md_once_bss_get(void);

static void sha224_md_init(void) {
  EVP_MD *md = sha224_md_bss_get();

  md->type = NID_sha224;
  md->md_size = SHA224_DIGEST_LENGTH;
  md->flags = 0;
  md->init = sha224_init;
  md->update = sha224_update;
  md->final = sha224_final;
  md->block_size = 64;
  md->ctx_size = sizeof(SHA256_CTX);
}

const EVP_MD *EVP_sha224(void) {
  CRYPTO_once(sha224_md_once_bss_get(), sha224_md_init);
  return sha224_md_bss_get();
}

static void sha256_init(EVP_MD_CTX *ctx) {
  CHECK(SHA256_Init(ctx->md_data));
}

static void sha256_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(SHA256_Update(ctx->md_data, data, count));
}

static void sha256_final(EVP_MD_CTX *ctx, uint8_t *md) {
  CHECK(SHA256_Final(md, ctx->md_data));
}

static EVP_MD sha256_md __attribute__((used));
static CRYPTO_once_t sha256_md_once __attribute__((used)) = CRYPTO_ONCE_INIT;

EVP_MD *sha256_md_bss_get(void);
CRYPTO_once_t *sha256_md_once_bss_get(void);

static void sha256_md_init(void) {
  EVP_MD *md = sha256_md_bss_get();

  md->type = NID_sha256;
  md->md_size = SHA256_DIGEST_LENGTH;
  md->flags = 0;
  md->init = sha256_init;
  md->update = sha256_update;
  md->final = sha256_final;
  md->block_size = 64;
  md->ctx_size = sizeof(SHA256_CTX);
}

const EVP_MD *EVP_sha256(void) {
  CRYPTO_once(sha256_md_once_bss_get(), sha256_md_init);
  return sha256_md_bss_get();
}

#if 0
static void sha384_init(EVP_MD_CTX *ctx) {
  CHECK(SHA384_Init(ctx->md_data));
}

static void sha384_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(SHA384_Update(ctx->md_data, data, count));
}

static void sha384_final(EVP_MD_CTX *ctx, uint8_t *md) {
  CHECK(SHA384_Final(md, ctx->md_data));
}

static const EVP_MD sha384_md = {
    NID_sha384,           SHA384_DIGEST_LENGTH, 0 /* flags */,
    sha384_init,          sha384_update,        sha384_final,
    128 /* block size */, sizeof(SHA512_CTX),
};

const EVP_MD *EVP_sha384(void) { return &sha384_md; }


static void sha512_init(EVP_MD_CTX *ctx) {
  CHECK(SHA512_Init(ctx->md_data));
}

static void sha512_update(EVP_MD_CTX *ctx, const void *data, size_t count) {
  CHECK(SHA512_Update(ctx->md_data, data, count));
}

static void sha512_final(EVP_MD_CTX *ctx, uint8_t *md) {
  CHECK(SHA512_Final(md, ctx->md_data));
}

static const EVP_MD sha512_md = {
    NID_sha512,           SHA512_DIGEST_LENGTH, 0 /* flags */,
    sha512_init,          sha512_update,        sha512_final,
    128 /* block size */, sizeof(SHA512_CTX),
};

const EVP_MD *EVP_sha512(void) { return &sha512_md; }


typedef struct {
  MD5_CTX md5;
  SHA_CTX sha1;
} MD5_SHA1_CTX;

static void md5_sha1_init(EVP_MD_CTX *md_ctx) {
  MD5_SHA1_CTX *ctx = md_ctx->md_data;
  CHECK(MD5_Init(&ctx->md5) && SHA1_Init(&ctx->sha1));
}

static void md5_sha1_update(EVP_MD_CTX *md_ctx, const void *data,
                            size_t count) {
  MD5_SHA1_CTX *ctx = md_ctx->md_data;
  CHECK(MD5_Update(&ctx->md5, data, count) &&
        SHA1_Update(&ctx->sha1, data, count));
}

static void md5_sha1_final(EVP_MD_CTX *md_ctx, uint8_t *out) {
  MD5_SHA1_CTX *ctx = md_ctx->md_data;
  CHECK(MD5_Final(out, &ctx->md5) &&
        SHA1_Final(out + MD5_DIGEST_LENGTH, &ctx->sha1));
}

static const EVP_MD md5_sha1_md = {
    NID_md5_sha1,
    MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH,
    0 /* flags */,
    md5_sha1_init,
    md5_sha1_update,
    md5_sha1_final,
    64 /* block size */,
    sizeof(MD5_SHA1_CTX),
};

const EVP_MD *EVP_md5_sha1(void) { return &md5_sha1_md; }


struct nid_to_digest {
  int nid;
  const EVP_MD* (*md_func)(void);
  const char *short_name;
  const char *long_name;
};

static const struct nid_to_digest nid_to_digest_mapping[] = {
    {NID_md4, EVP_md4, SN_md4, LN_md4},
    {NID_md5, EVP_md5, SN_md5, LN_md5},
    {NID_sha1, EVP_sha1, SN_sha1, LN_sha1},
    {NID_sha224, EVP_sha224, SN_sha224, LN_sha224},
    {NID_sha256, EVP_sha256, SN_sha256, LN_sha256},
    {NID_sha384, EVP_sha384, SN_sha384, LN_sha384},
    {NID_sha512, EVP_sha512, SN_sha512, LN_sha512},
    {NID_md5_sha1, EVP_md5_sha1, SN_md5_sha1, LN_md5_sha1},
    /* As a remnant of signing |EVP_MD|s, OpenSSL returned the corresponding
     * hash function when given a signature OID. To avoid unintended lax parsing
     * of hash OIDs, this is no longer supported for lookup by OID or NID.
     * Node.js, however, exposes |EVP_get_digestbyname|'s full behavior to
     * consumers so we retain it there. */
    {NID_undef, EVP_sha1, SN_dsaWithSHA, LN_dsaWithSHA},
    {NID_undef, EVP_sha1, SN_dsaWithSHA1, LN_dsaWithSHA1},
    {NID_undef, EVP_sha1, SN_ecdsa_with_SHA1, NULL},
    {NID_undef, EVP_md5, SN_md5WithRSAEncryption, LN_md5WithRSAEncryption},
    {NID_undef, EVP_sha1, SN_sha1WithRSAEncryption, LN_sha1WithRSAEncryption},
    {NID_undef, EVP_sha224, SN_sha224WithRSAEncryption,
     LN_sha224WithRSAEncryption},
    {NID_undef, EVP_sha256, SN_sha256WithRSAEncryption,
     LN_sha256WithRSAEncryption},
    {NID_undef, EVP_sha384, SN_sha384WithRSAEncryption,
     LN_sha384WithRSAEncryption},
    {NID_undef, EVP_sha512, SN_sha512WithRSAEncryption,
     LN_sha512WithRSAEncryption},
};

const EVP_MD* EVP_get_digestbynid(int nid) {
  if (nid == NID_undef) {
    /* Skip the |NID_undef| entries in |nid_to_digest_mapping|. */
    return NULL;
  }

  for (unsigned i = 0; i < OPENSSL_ARRAY_SIZE(nid_to_digest_mapping); i++) {
    if (nid_to_digest_mapping[i].nid == nid) {
      return nid_to_digest_mapping[i].md_func();
    }
  }

  return NULL;
}

static const struct {
  uint8_t oid[9];
  uint8_t oid_len;
  const EVP_MD *(*md_func) (void);
} kMDOIDs[] = {
  /* 1.2.840.113549.2.4 */
  { {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x04}, 8, EVP_md4 },
  /* 1.2.840.113549.2.5 */
  { {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05}, 8, EVP_md5 },
  /* 1.3.14.3.2.26 */
  { {0x2b, 0x0e, 0x03, 0x02, 0x1a}, 5, EVP_sha1 },
  /* 2.16.840.1.101.3.4.2.1 */
  { {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01}, 9, EVP_sha256 },
  /* 2.16.840.1.101.3.4.2.2 */
  { {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02}, 9, EVP_sha384 },
  /* 2.16.840.1.101.3.4.2.3 */
  { {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03}, 9, EVP_sha512 },
  /* 2.16.840.1.101.3.4.2.4 */
  { {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04}, 9, EVP_sha224 },
};

static const EVP_MD *cbs_to_md(const CBS *cbs) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMDOIDs); i++) {
    if (CBS_len(cbs) == kMDOIDs[i].oid_len &&
        OPENSSL_memcmp(CBS_data(cbs), kMDOIDs[i].oid, kMDOIDs[i].oid_len) ==
            0) {
      return kMDOIDs[i].md_func();
    }
  }

  return NULL;
}

const EVP_MD *EVP_get_digestbyobj(const ASN1_OBJECT *obj) {
  /* Handle objects with no corresponding OID. */
  if (obj->nid != NID_undef) {
    return EVP_get_digestbynid(obj->nid);
  }

  CBS cbs;
  CBS_init(&cbs, obj->data, obj->length);
  return cbs_to_md(&cbs);
}

const EVP_MD *EVP_parse_digest_algorithm(CBS *cbs) {
  CBS algorithm, oid;
  if (!CBS_get_asn1(cbs, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&algorithm, &oid, CBS_ASN1_OBJECT)) {
    OPENSSL_PUT_ERROR(DIGEST, DIGEST_R_DECODE_ERROR);
    return NULL;
  }

  const EVP_MD *ret = cbs_to_md(&oid);
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(DIGEST, DIGEST_R_UNKNOWN_HASH);
    return NULL;
  }

  /* The parameters, if present, must be NULL. Historically, whether the NULL
   * was included or omitted was not well-specified. When parsing an
   * AlgorithmIdentifier, we allow both. (Note this code is not used when
   * verifying RSASSA-PKCS1-v1_5 signatures.) */
  if (CBS_len(&algorithm) > 0) {
    CBS param;
    if (!CBS_get_asn1(&algorithm, &param, CBS_ASN1_NULL) ||
        CBS_len(&param) != 0 ||
        CBS_len(&algorithm) != 0) {
      OPENSSL_PUT_ERROR(DIGEST, DIGEST_R_DECODE_ERROR);
      return NULL;
    }
  }

  return ret;
}

const EVP_MD *EVP_get_digestbyname(const char *name) {
  for (unsigned i = 0; i < OPENSSL_ARRAY_SIZE(nid_to_digest_mapping); i++) {
    const char *short_name = nid_to_digest_mapping[i].short_name;
    const char *long_name = nid_to_digest_mapping[i].long_name;
    if ((short_name && strcmp(short_name, name) == 0) ||
        (long_name && strcmp(long_name, name) == 0)) {
      return nid_to_digest_mapping[i].md_func();
    }
  }

  return NULL;
}

#endif
