/* Originally written by Bodo Moeller and Nils Larsch for the OpenSSL project.
 * ====================================================================
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems
 * Laboratories. */

#include <openssl/ec.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "internal.h"


const EC_METHOD *EC_GFp_mont_method(void) {
  static const EC_METHOD ret = {ec_GFp_mont_group_extra_finish,
                                ec_GFp_mont_group_extra_copy,
                                ec_GFp_mont_group_set_curve,
                                ec_GFp_simple_point_get_affine_coordinates,
                                ec_wNAF_mul, /* XXX TODO: not constant-time. */
                                ec_wNAF_precompute_mult,
                                ec_GFp_mont_field_mul,
                                ec_GFp_mont_field_sqr,
                                ec_GFp_mont_field_encode,
                                ec_GFp_mont_field_decode,
                                ec_GFp_mont_field_set_to_one};

  return &ret;
}

void ec_GFp_mont_group_extra_finish(EC_GROUP *group) {
  if (group->mont != NULL) {
    BN_MONT_CTX_free(group->mont);
    group->mont = NULL;
  }
  if (group->one != NULL) {
    BN_free(group->one);
    group->one = NULL;
  }
}

int ec_GFp_mont_group_extra_copy(EC_GROUP *dest, const EC_GROUP *src) {
  if (src->mont != NULL) {
    dest->mont = BN_MONT_CTX_new();
    if (dest->mont == NULL) {
      return 0;
    }
    if (!BN_MONT_CTX_copy(dest->mont, src->mont)) {
      goto err;
    }
  }
  if (src->one != NULL) {
    dest->one = BN_dup(src->one);
    if (dest->one == NULL) {
      goto err;
    }
  }

  return 1;

err:
  if (dest->mont != NULL) {
    BN_MONT_CTX_free(dest->mont);
    dest->mont = NULL;
  }
  return 0;
}

int ec_GFp_mont_group_set_curve(EC_GROUP *group, const BIGNUM *p,
                                const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx) {
  BN_CTX *new_ctx = NULL;
  BN_MONT_CTX *mont = NULL;
  BIGNUM *one = NULL;
  int ret = 0;

  if (group->mont != NULL) {
    BN_MONT_CTX_free(group->mont);
    group->mont = NULL;
  }
  if (group->one != NULL) {
    BN_free(group->one);
    group->one = NULL;
  }

  if (ctx == NULL) {
    ctx = new_ctx = BN_CTX_new();
    if (ctx == NULL) {
      return 0;
    }
  }

  mont = BN_MONT_CTX_new();
  if (mont == NULL) {
    goto err;
  }
  if (!BN_MONT_CTX_set(mont, p, ctx)) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_group_set_curve, ERR_R_BN_LIB);
    goto err;
  }
  one = BN_new();
  if (one == NULL || !BN_to_montgomery(one, BN_value_one(), mont, ctx)) {
    goto err;
  }

  group->mont = mont;
  mont = NULL;
  group->one = one;
  one = NULL;

  ret = ec_GFp_simple_group_set_curve(group, p, a, b, ctx);

  if (!ret) {
    BN_MONT_CTX_free(group->mont);
    group->mont = NULL;
    BN_free(group->one);
    group->one = NULL;
  }

err:
  if (new_ctx != NULL) {
    BN_CTX_free(new_ctx);
  }
  if (mont != NULL) {
    BN_MONT_CTX_free(mont);
  }
  if (one != NULL) {
    BN_free(one);
  }
  return ret;
}

int ec_GFp_mont_field_mul(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a,
                          const BIGNUM *b, BN_CTX *ctx) {
  if (group->mont == NULL) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_field_mul, EC_R_NOT_INITIALIZED);
    return 0;
  }

  return BN_mod_mul_montgomery(r, a, b, group->mont, ctx);
}

int ec_GFp_mont_field_sqr(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a,
                          BN_CTX *ctx) {
  if (group->mont == NULL) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_field_sqr, EC_R_NOT_INITIALIZED);
    return 0;
  }

  return BN_mod_mul_montgomery(r, a, a, group->mont, ctx);
}

int ec_GFp_mont_field_encode(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a,
                             BN_CTX *ctx) {
  if (group->mont == NULL) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_field_encode, EC_R_NOT_INITIALIZED);
    return 0;
  }

  return BN_to_montgomery(r, a, group->mont, ctx);
}

int ec_GFp_mont_field_decode(const EC_GROUP *group, BIGNUM *r, const BIGNUM *a,
                             BN_CTX *ctx) {
  if (group->mont == NULL) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_field_decode, EC_R_NOT_INITIALIZED);
    return 0;
  }

  return BN_from_montgomery(r, a, group->mont, ctx);
}

int ec_GFp_mont_field_set_to_one(const EC_GROUP *group, BIGNUM *r,
                                 BN_CTX *ctx) {
  if (group->one == NULL) {
    OPENSSL_PUT_ERROR(EC, ec_GFp_mont_field_set_to_one, EC_R_NOT_INITIALIZED);
    return 0;
  }

  if (!BN_copy(r, group->one)) {
    return 0;
  }
  return 1;
}
