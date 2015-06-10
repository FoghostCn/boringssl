/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/ecdsa.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/ec_key.h>
#include <openssl/mem.h>

#include "../ec/internal.h"


DECLARE_ASN1_FUNCTIONS_const(ECDSA_SIG);
DECLARE_ASN1_ENCODE_FUNCTIONS_const(ECDSA_SIG, ECDSA_SIG);

ASN1_SEQUENCE(ECDSA_SIG) = {
    ASN1_SIMPLE(ECDSA_SIG, r, CBIGNUM),
    ASN1_SIMPLE(ECDSA_SIG, s, CBIGNUM),
} ASN1_SEQUENCE_END(ECDSA_SIG);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(ECDSA_SIG, ECDSA_SIG, ECDSA_SIG);

size_t ECDSA_size(const EC_KEY *key) {
  if (key == NULL) {
    return 0;
  }

  size_t group_order_size;
  if (key->ecdsa_meth && key->ecdsa_meth->group_order_size) {
    group_order_size = key->ecdsa_meth->group_order_size(key);
  } else {
    const EC_GROUP *group = EC_KEY_get0_group(key);
    if (group == NULL) {
      return 0;
    }

    BIGNUM *order = BN_new();
    if (order == NULL) {
      return 0;
    }
    if (!EC_GROUP_get_order(group, order, NULL)) {
      BN_clear_free(order);
      return 0;
    }

    group_order_size = BN_num_bytes(order);
    BN_clear_free(order);
  }

  return ECDSA_SIG_max_len(group_order_size);
}

ECDSA_SIG *ECDSA_SIG_new(void) {
  ECDSA_SIG *sig = OPENSSL_malloc(sizeof(ECDSA_SIG));
  if (sig == NULL) {
    return NULL;
  }
  sig->r = BN_new();
  sig->s = BN_new();
  if (sig->r == NULL || sig->s == NULL) {
    ECDSA_SIG_free(sig);
    return NULL;
  }
  return sig;
}

void ECDSA_SIG_free(ECDSA_SIG *sig) {
  if (sig == NULL) {
    return;
  }

  BN_free(sig->r);
  BN_free(sig->s);
  OPENSSL_free(sig);
}

/* der_len_len returns the number of bytes needed to represent a length of |len|
 * in DER. */
static size_t der_len_len(size_t len) {
  if (len < 0x80) {
    return 1;
  }
  size_t ret = 1;
  while (len > 0) {
    ret++;
    len >>= 8;
  }
  return ret;
}

size_t ECDSA_SIG_max_len(size_t order_len) {
  /* Compute the maximum length of an |order_len| byte integer. Defensively
   * assume that the leading 0x00 is included. */
  size_t integer_len = 1 /* tag */ + der_len_len(order_len + 1) + order_len + 1;
  if (integer_len < order_len) {
    return 0;
  }
  /* An ECDSA signature is two INTEGERs. */
  size_t value_len = 2 * integer_len;
  if (value_len < integer_len) {
    return 0;
  }
  /* Add the header. */
  size_t ret = 1 /* tag */ + der_len_len(value_len) + value_len;
  if (ret < value_len) {
    return 0;
  }
  return ret;
}
