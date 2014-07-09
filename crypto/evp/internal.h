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

#ifndef OPENSSL_HEADER_EVP_INTERNAL_H
#define OPENSSL_HEADER_EVP_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* These values are flags for EVP_PKEY_ASN1_METHOD.flags. */
#define ASN1_PKEY_ALIAS 0x1
#define ASN1_PKEY_DYNAMIC 0x2
#define ASN1_PKEY_SIGPARAM_NULL 0x4

struct evp_pkey_asn1_method_st {
  int pkey_id;
  int pkey_base_id;
  unsigned long pkey_flags;

  char *pem_str;
  char *info;

  int (*pub_decode)(EVP_PKEY *pk, X509_PUBKEY *pub);
  int (*pub_encode)(X509_PUBKEY *pub, const EVP_PKEY *pk);
  int (*pub_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);
  int (*pub_print)(BIO *out, const EVP_PKEY *pkey, int indent, ASN1_PCTX *pctx);

  int (*priv_decode)(EVP_PKEY *pk, PKCS8_PRIV_KEY_INFO *p8inf);
  int (*priv_encode)(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pk);
  int (*priv_print)(BIO *out, const EVP_PKEY *pkey, int indent,
                    ASN1_PCTX *pctx);

  int (*pkey_size)(const EVP_PKEY *pk);
  int (*pkey_bits)(const EVP_PKEY *pk);

  int (*param_decode)(EVP_PKEY *pkey, const unsigned char **pder, int derlen);
  int (*param_encode)(const EVP_PKEY *pkey, unsigned char **pder);
  int (*param_missing)(const EVP_PKEY *pk);
  int (*param_copy)(EVP_PKEY *to, const EVP_PKEY *from);
  int (*param_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);
  int (*param_print)(BIO *out, const EVP_PKEY *pkey, int indent,
                     ASN1_PCTX *pctx);
  int (*sig_print)(BIO *out, const X509_ALGOR *sigalg, const ASN1_STRING *sig,
                   int indent, ASN1_PCTX *pctx);


  void (*pkey_free)(EVP_PKEY *pkey);
  int (*pkey_ctrl)(EVP_PKEY *pkey, int op, long arg1, void *arg2);

  /* Legacy functions for old PEM */

  int (*old_priv_decode)(EVP_PKEY *pkey, const unsigned char **pder,
                         int derlen);
  int (*old_priv_encode)(const EVP_PKEY *pkey, unsigned char **pder);
  /* Custom ASN1 signature verification */
  int (*item_verify)(EVP_MD_CTX *ctx, const ASN1_ITEM *it, void *asn,
                     X509_ALGOR *a, ASN1_BIT_STRING *sig, EVP_PKEY *pkey);
  int (*item_sign)(EVP_MD_CTX *ctx, const ASN1_ITEM *it, void *asn,
                   X509_ALGOR *alg1, X509_ALGOR *alg2, ASN1_BIT_STRING *sig);

} /* EVP_PKEY_ASN1_METHOD */;


typedef int EVP_PKEY_gen_cb(EVP_PKEY_CTX *ctx);

#define EVP_PKEY_OP_UNDEFINED 0
#define EVP_PKEY_OP_PARAMGEN (1 << 1)
#define EVP_PKEY_OP_KEYGEN (1 << 2)
#define EVP_PKEY_OP_SIGN (1 << 3)
#define EVP_PKEY_OP_VERIFY (1 << 4)
#define EVP_PKEY_OP_VERIFYRECOVER (1 << 5)
#define EVP_PKEY_OP_SIGNCTX (1 << 6)
#define EVP_PKEY_OP_VERIFYCTX (1 << 7)
#define EVP_PKEY_OP_ENCRYPT (1 << 8)
#define EVP_PKEY_OP_DECRYPT (1 << 9)
#define EVP_PKEY_OP_DERIVE (1 << 10)

#define EVP_PKEY_OP_TYPE_SIG                                           \
  (EVP_PKEY_OP_SIGN | EVP_PKEY_OP_VERIFY | EVP_PKEY_OP_VERIFYRECOVER | \
   EVP_PKEY_OP_SIGNCTX | EVP_PKEY_OP_VERIFYCTX)

#define EVP_PKEY_OP_TYPE_CRYPT (EVP_PKEY_OP_ENCRYPT | EVP_PKEY_OP_DECRYPT)

#define EVP_PKEY_OP_TYPE_NOGEN \
  (EVP_PKEY_OP_SIG | EVP_PKEY_OP_CRYPT | EVP_PKEY_OP_DERIVE)

#define EVP_PKEY_OP_TYPE_GEN (EVP_PKEY_OP_PARAMGEN | EVP_PKEY_OP_KEYGEN)

#define EVP_PKEY_CTRL_MD 1
#define EVP_PKEY_CTRL_GET_MD 2
#define EVP_PKEY_CTRL_RSA_PADDING (EVP_PKEY_ALG_CTRL + 1)
#define EVP_PKEY_CTRL_GET_RSA_PADDING (EVP_PKEY_ALG_CTRL + 2)
#define EVP_PKEY_CTRL_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 3)
#define EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 4)
#define EVP_PKEY_CTRL_RSA_KEYGEN_BITS (EVP_PKEY_ALG_CTRL + 5)
#define EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP	(EVP_PKEY_ALG_CTRL + 6)
#define EVP_PKEY_CTRL_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 7)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 8)
#define EVP_PKEY_CTRL_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 9)
#define EVP_PKEY_CTRL_GET_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 10)
#define EVP_PKEY_CTRL_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 11)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 12)

struct evp_pkey_ctx_st {
  /* Method associated with this operation */
  const EVP_PKEY_METHOD *pmeth;
  /* Engine that implements this method or NULL if builtin */
  ENGINE *engine;
  /* Key: may be NULL */
  EVP_PKEY *pkey;
  /* Peer key for key agreement, may be NULL */
  EVP_PKEY *peerkey;
  /* operation contains one of the |EVP_PKEY_OP_*| values. */
  int operation;
  /* Algorithm specific data */
  void *data;
  /* Application specific data */
  void *app_data;
} /* EVP_PKEY_CTX */;

struct evp_pkey_method_st {
  int pkey_id;
  int flags;

  int (*init)(EVP_PKEY_CTX *ctx);
  int (*copy)(EVP_PKEY_CTX *dst, EVP_PKEY_CTX *src);
  void (*cleanup)(EVP_PKEY_CTX *ctx);

  int (*paramgen_init)(EVP_PKEY_CTX *ctx);
  int (*paramgen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

  int (*keygen_init)(EVP_PKEY_CTX *ctx);
  int (*keygen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

  int (*sign_init)(EVP_PKEY_CTX *ctx);
  int (*sign)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
              const unsigned char *tbs, size_t tbslen);

  int (*verify_init)(EVP_PKEY_CTX *ctx);
  int (*verify)(EVP_PKEY_CTX *ctx, const unsigned char *sig, size_t siglen,
                const unsigned char *tbs, size_t tbslen);

  int (*signctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
  int (*signctx)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
                 EVP_MD_CTX *mctx);

  int (*verifyctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
  int (*verifyctx)(EVP_PKEY_CTX *ctx, const unsigned char *sig, int siglen,
                   EVP_MD_CTX *mctx);

  int (*encrypt_init)(EVP_PKEY_CTX *ctx);
  int (*encrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
                 const unsigned char *in, size_t inlen);

  int (*decrypt_init)(EVP_PKEY_CTX *ctx);
  int (*decrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
                 const unsigned char *in, size_t inlen);

  int (*derive_init)(EVP_PKEY_CTX *ctx);
  int (*derive)(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);

  int (*ctrl)(EVP_PKEY_CTX *ctx, int type, int p1, void *p2);
  int (*ctrl_str)(EVP_PKEY_CTX *ctx, const char *type, const char *value);
} /* EVP_PKEY_METHOD */;


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_EVP_INTERNAL_H */
