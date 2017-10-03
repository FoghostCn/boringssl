/* Copyright (c) 2017, Google Inc.
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

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/span.h>

// Basic implementation of mod_exp using square and multiple method.
int mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
            BN_CTX *ctx) {
  if (BN_is_one(m)) {
    BN_zero(r);
    return 1;
  }

  bssl::UniquePtr<BIGNUM> exp(BN_dup(p));
  bssl::UniquePtr<BIGNUM> base(BN_new());
  if (!exp || !base) {
    return 0;
  }
  if (!BN_one(r) || !BN_nnmod(base.get(), a, m, ctx)) {
    return 0;
  }

  while (!BN_is_zero(exp.get())) {
    if (BN_is_odd(exp.get())) {
      if (!BN_mul(r, r, base.get(), ctx) || !BN_nnmod(r, r, m, ctx)) {
        return 0;
      }
    }
    if (!BN_rshift1(exp.get(), exp.get()) ||
        !BN_mul(base.get(), base.get(), base.get(), ctx) ||
        !BN_nnmod(base.get(), base.get(), m, ctx)) {
      return 0;
    }
  }

  return 1;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
  CBS cbs, child0, child1, child2;
  uint8_t sign0, sign1, sign2;
  CBS_init(&cbs, buf, len);
  if (!CBS_get_u8_length_prefixed(&cbs, &child0) ||
      !CBS_get_u8(&child0, &sign0) ||
      CBS_len(&child0) == 0 ||
      !CBS_get_u8_length_prefixed(&cbs, &child1) ||
      !CBS_get_u8(&child1, &sign1) ||
      CBS_len(&child1) == 0 ||
      !CBS_get_u8_length_prefixed(&cbs, &child2) ||
      !CBS_get_u8(&child2, &sign2) ||
      CBS_len(&child2) == 0) {
    return 0;
  }

  bssl::UniquePtr<BIGNUM> bn0(
      BN_bin2bn(CBS_data(&child0), CBS_len(&child0), nullptr));
  BN_set_negative(bn0.get(), sign0 % 2);
  bssl::UniquePtr<BIGNUM> bn1(
      BN_bin2bn(CBS_data(&child1), CBS_len(&child1), nullptr));
  BN_set_negative(bn1.get(), sign1 % 2);
  bssl::UniquePtr<BIGNUM> bn2(
      BN_bin2bn(CBS_data(&child2), CBS_len(&child2), nullptr));
  BN_set_negative(bn2.get(), sign2 % 2);

  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  bssl::UniquePtr<BIGNUM> bnr(BN_new());
  bssl::UniquePtr<BIGNUM> bnq(BN_new());
  bssl::UniquePtr<BN_MONT_CTX> mont(BN_MONT_CTX_new());
  if (!ctx || !bnr || !bnq || !mont) {
    return -1;
  }

  if (!BN_is_zero(bn2.get()) &&
      !BN_is_negative(bn1.get()) &&
      !BN_is_negative(bn2.get())) {
    if (!mod_exp(bnq.get(), bn0.get(), bn1.get(), bn2.get(), ctx.get())) {
      return -1;
    }

    if (!BN_mod_exp(bnr.get(), bn0.get(), bn1.get(), bn2.get(), ctx.get()) ||
        BN_cmp(bnr.get(), bnq.get()) != 0) {
      return -1;
    }

    if (BN_MONT_CTX_set(mont.get(), bn2.get(), ctx.get())) {
      if (!BN_mod_exp_mont(bnr.get(), bn0.get(), bn1.get(), bn2.get(),
                           ctx.get(), mont.get()) ||
          BN_cmp(bnr.get(), bnq.get()) != 0) {
        return -1;
      }

      if (!BN_mod_exp_mont_consttime(bnr.get(), bn0.get(), bn1.get(), bn2.get(),
                                     ctx.get(), mont.get()) ||
          BN_cmp(bnr.get(), bnq.get()) != 0) {
        return -1;
      }
    }
  }

  uint8_t *data = (uint8_t *)OPENSSL_malloc(BN_num_bytes(bnr.get()));
  BN_bn2bin(bnr.get(), data);
  OPENSSL_free(data);

  return 0;
}
