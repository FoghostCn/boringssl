/* Copyright (c) 2014, Google Inc.
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

/* This implementation of poly1305 is by Andrew Moon
 * (https://github.com/floodyberry/poly1305-donna) and released as public
 * domain. */

#include <openssl/poly1305.h>

#include <string.h>

#include <openssl/cpu.h>
#include <openssl/type_check.h>

#if defined(OPENSSL_ARM) && !defined(OPENSSL_NO_ASM)
#define POLY1305_HAS_NEON
void CRYPTO_poly1305_init_neon(poly1305_state *state, const uint8_t key[32]);

void CRYPTO_poly1305_update_neon(poly1305_state *state, const uint8_t *in,
                                 size_t in_len);

void CRYPTO_poly1305_finish_neon(poly1305_state *state, uint8_t mac[16]);
#endif

#if !defined(OPENSSL_WINDOWS) && defined(OPENSSL_X86_64)
#define POLY1305_HAS_MMX
void CRYPTO_poly1305_init_mmx(poly1305_state *state, const uint8_t key[32]);

void CRYPTO_poly1305_update_mmx(poly1305_state *state, const uint8_t *in,
                                 size_t in_len);

void CRYPTO_poly1305_finish_mmx(poly1305_state *state, uint8_t mac[16]);
#endif

#if defined(OPENSSL_X86_64) && !defined(OPENSSL_NO_ASM)
#define POLY1305_HAS_AVX
void poly1305_init_avx(poly1305_state* state, const uint8_t key[32]);
void poly1305_update_avx(poly1305_state* state, const uint8_t *in, size_t in_len);
void poly1305_finish_avx(poly1305_state* state, uint8_t mac[16]);

void poly1305_init_avx2(poly1305_state* state, const uint8_t key[32]);
void poly1305_update_avx2(poly1305_state* state, const uint8_t *in, size_t in_len);
void poly1305_finish_avx2(poly1305_state* state, uint8_t mac[16]);
#endif

struct poly1305_state_st {
  uint32_t r0, r1, r2, r3, r4;
  uint32_t s1, s2, s3, s4;
  uint32_t h0, h1, h2, h3, h4;
  uint8_t buf[16];
  unsigned int buf_used;
  uint8_t key[16];
};
OPENSSL_COMPILE_ASSERT(sizeof(struct poly1305_state_st)
                       <= sizeof(poly1305_state),
                       poly1305_state_too_big);

/* The internal state of the x86 ASM implementation of poly1305 is ~324 bytes
 * long.  However, since the implementation accepts input only in 128-byte sized
 * chunks, this structure also has an internal buffer to accumulate bytes before
 * feeding them into the */
struct poly1305_state_avx_st {
  /* The assembley code is vague on how much space does it exactly need, so the
   * largest size which would fit into poly1305_state is used here.
   * Experimentally, it is 324 bytes. */
  uint8_t state[488];
  uint8_t buf[16];
  unsigned int buf_used;
};
OPENSSL_COMPILE_ASSERT(sizeof(struct poly1305_state_avx_st)
                       <= sizeof(poly1305_state),
                       poly1305_asm_state_too_big);

#ifndef POLY1305_HAS_MMX
/* poly1305_blocks updates |state| given some amount of input data. This
 * function may only be called with a |len| that is not a multiple of 16 at the
 * end of the data. Otherwise the input must be buffered into 16 byte blocks. */
static void poly1305_update(poly1305_state *statep, const uint8_t *in,
                            size_t len) {
  uint32_t t0, t1, t2, t3;
  uint64_t t[5];
  uint32_t b;
  uint64_t c;
  size_t j;
  uint8_t mp[16];
  struct poly1305_state_st *state = (struct poly1305_state_st*)statep;

  if (len < 16) {
    goto poly1305_donna_atmost15bytes;
  }

poly1305_donna_16bytes:
  t0 = U8TO32_LE(in);
  t1 = U8TO32_LE(in + 4);
  t2 = U8TO32_LE(in + 8);
  t3 = U8TO32_LE(in + 12);

  in += 16;
  len -= 16;

  state->h0 += t0 & 0x3ffffff;
  state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
  state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
  state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
  state->h4 += (t3 >> 8) | (1 << 24);

poly1305_donna_mul:
  t[0] = mul32x32_64(state->h0, state->r0) + mul32x32_64(state->h1, state->s4) +
         mul32x32_64(state->h2, state->s3) + mul32x32_64(state->h3, state->s2) +
         mul32x32_64(state->h4, state->s1);
  t[1] = mul32x32_64(state->h0, state->r1) + mul32x32_64(state->h1, state->r0) +
         mul32x32_64(state->h2, state->s4) + mul32x32_64(state->h3, state->s3) +
         mul32x32_64(state->h4, state->s2);
  t[2] = mul32x32_64(state->h0, state->r2) + mul32x32_64(state->h1, state->r1) +
         mul32x32_64(state->h2, state->r0) + mul32x32_64(state->h3, state->s4) +
         mul32x32_64(state->h4, state->s3);
  t[3] = mul32x32_64(state->h0, state->r3) + mul32x32_64(state->h1, state->r2) +
         mul32x32_64(state->h2, state->r1) + mul32x32_64(state->h3, state->r0) +
         mul32x32_64(state->h4, state->s4);
  t[4] = mul32x32_64(state->h0, state->r4) + mul32x32_64(state->h1, state->r3) +
         mul32x32_64(state->h2, state->r2) + mul32x32_64(state->h3, state->r1) +
         mul32x32_64(state->h4, state->r0);

  state->h0 = (uint32_t)t[0] & 0x3ffffff;
  c = (t[0] >> 26);
  t[1] += c;
  state->h1 = (uint32_t)t[1] & 0x3ffffff;
  b = (uint32_t)(t[1] >> 26);
  t[2] += b;
  state->h2 = (uint32_t)t[2] & 0x3ffffff;
  b = (uint32_t)(t[2] >> 26);
  t[3] += b;
  state->h3 = (uint32_t)t[3] & 0x3ffffff;
  b = (uint32_t)(t[3] >> 26);
  t[4] += b;
  state->h4 = (uint32_t)t[4] & 0x3ffffff;
  b = (uint32_t)(t[4] >> 26);
  state->h0 += b * 5;

  if (len >= 16) {
    goto poly1305_donna_16bytes;
  }

/* final bytes */
poly1305_donna_atmost15bytes:
  if (!len) {
    return;
  }

  for (j = 0; j < len; j++) {
    mp[j] = in[j];
  }
  mp[j++] = 1;
  for (; j < 16; j++) {
    mp[j] = 0;
  }
  len = 0;

  t0 = U8TO32_LE(mp + 0);
  t1 = U8TO32_LE(mp + 4);
  t2 = U8TO32_LE(mp + 8);
  t3 = U8TO32_LE(mp + 12);

  state->h0 += t0 & 0x3ffffff;
  state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
  state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
  state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
  state->h4 += (t3 >> 8);

  goto poly1305_donna_mul;
}

static uint64_t mul32x32_64(uint32_t a, uint32_t b) { return (uint64_t)a * b; }

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64) || defined(OPENSSL_ARM)
/* We can assume little-endian. */
static uint32_t U8TO32_LE(const uint8_t *m) {
  uint32_t r;
  memcpy(&r, m, sizeof(r));
  return r;
}

static void U32TO8_LE(uint8_t *m, uint32_t v) { memcpy(m, &v, sizeof(v)); }
#else
static uint32_t U8TO32_LE(const uint8_t *m) {
  return (uint32_t)m[0] | (uint32_t)m[1] << 8 | (uint32_t)m[2] << 16 |
         (uint32_t)m[3] << 24;
}

static void U32TO8_LE(uint8_t *m, uint32_t v) {
  m[0] = v;
  m[1] = v >> 8;
  m[2] = v >> 16;
  m[3] = v >> 24;
}
#endif  /* OPENSSL_X86 || OPENSSL_X86_64 || OPENSSL_ARM */
#endif  /* !POLY1305_HAS_MMX */

typedef void (*poly1305_update_t)(poly1305_state*, const uint8_t*, size_t);

void CRYPTO_poly1305_init(poly1305_state *statep, const uint8_t key[32]) {
#ifdef POLY1305_HAS_NEON
  if (CRYPTO_is_NEON_functional()) {
    CRYPTO_poly1305_init_neon(statep, key);
    return;
  }
#endif  /* POLY1305_HAS_NEON */

#ifdef POLY1305_HAS_AVX
  if (CRYPTO_has_AVX()) {
    struct poly1305_state_avx_st *state_avx
        = (struct poly1305_state_avx_st*) statep;
    state_avx->buf_used = 0;
    if (CRYPTO_has_AVX2()) {
      poly1305_init_avx2(statep, key);
    } else {
      poly1305_init_avx(statep, key);
    }
    return;
  }
#endif  /* POLY1305_HAS_AVX */

#ifdef POLY1305_HAS_MMX
  CRYPTO_poly1305_init_mmx(statep, key);
#else
  {
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;
    uint32_t t0, t1, t2, t3;

    t0 = U8TO32_LE(key + 0);
    t1 = U8TO32_LE(key + 4);
    t2 = U8TO32_LE(key + 8);
    t3 = U8TO32_LE(key + 12);

    /* precompute multipliers */
    state->r0 = t0 & 0x3ffffff;
    t0 >>= 26;
    t0 |= t1 << 6;
    state->r1 = t0 & 0x3ffff03;
    t1 >>= 20;
    t1 |= t2 << 12;
    state->r2 = t1 & 0x3ffc0ff;
    t2 >>= 14;
    t2 |= t3 << 18;
    state->r3 = t2 & 0x3f03fff;
    t3 >>= 8;
    state->r4 = t3 & 0x00fffff;

    state->s1 = state->r1 * 5;
    state->s2 = state->r2 * 5;
    state->s3 = state->r3 * 5;
    state->s4 = state->r4 * 5;

    /* init state */
    state->h0 = 0;
    state->h1 = 0;
    state->h2 = 0;
    state->h3 = 0;
    state->h4 = 0;

    state->buf_used = 0;
    memcpy(state->key, key + 16, sizeof(state->key));
  }
#endif  /* POLY1305_HAS_MMX */
}

void CRYPTO_poly1305_update(poly1305_state *statep, const uint8_t *in,
                            size_t in_len) {
  unsigned int i;
  uint8_t *buf;
  unsigned int *buf_used;
  poly1305_update_t update_function;

#if defined(POLY1305_HAS_MMX) && !defined(POLY1305_HAS_AVX)
  CRYPTO_poly1305_update_mmx(statep, in, in_len);
#else

#if defined(OPENSSL_ARM) && !defined(OPENSSL_NO_ASM)
  if (CRYPTO_is_NEON_functional()) {
    CRYPTO_poly1305_update_neon(statep, in, in_len);
    return;
  }
#endif

#ifdef POLY1305_HAS_AVX
  if (CRYPTO_has_AVX()) {
    struct poly1305_state_avx_st *state_avx
        = (struct poly1305_state_avx_st*) statep;

    buf = state_avx->buf;
    buf_used = &state_avx->buf_used;
    if (CRYPTO_has_AVX2()) {
      update_function = poly1305_update_avx2;
    } else {
      update_function = poly1305_update_avx;
    }
  } else {
    CRYPTO_poly1305_update_mmx(statep, in, in_len);
    return;
  }
#else
  {
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;

    buf = state->buf;
    buf_used = &state->buf_used;
    update_function = poly1305_update;
  }
#endif  /* POLY1305_HAS_AVX */

  if (*buf_used) {
    unsigned int todo = 16 - *buf_used;
    if (todo > in_len) {
      todo = in_len;
    }
    for (i = 0; i < todo; i++) {
      buf[*buf_used + i] = in[i];
    }
    *buf_used += todo;
    in_len -= todo;
    in += todo;

    if (*buf_used == 16) {
      update_function(statep, buf, 16);
      *buf_used = 0;
    }
  }

  if (in_len >= 16) {
    size_t todo = in_len & ~0xf;
    update_function(statep, in, todo);
    in += todo;
    in_len &= 0xf;
  }

  if (in_len) {
    for (i = 0; i < in_len; i++) {
      buf[i] = in[i];
    }
    *buf_used = in_len;
  }
#endif  /* POLY1305_HAS_MMX && !POLY1305_HAS_AVX */
}

void CRYPTO_poly1305_finish(poly1305_state *statep, uint8_t mac[16]) {
#if defined(OPENSSL_ARM) && !defined(OPENSSL_NO_ASM)
  if (CRYPTO_is_NEON_functional()) {
    CRYPTO_poly1305_finish_neon(statep, mac);
    return;
  }
#endif

#ifdef POLY1305_HAS_AVX
  if (CRYPTO_has_AVX()) {
    struct poly1305_state_avx_st *state_avx
        = (struct poly1305_state_avx_st*) statep;

    if (CRYPTO_has_AVX2()) {
      if (state_avx->buf_used > 0) {
        poly1305_update_avx2(statep,
                             state_avx->buf,
                             state_avx->buf_used);
      }
      poly1305_finish_avx2(statep, mac);
    } else {
      if (state_avx->buf_used > 0) {
        poly1305_update_avx(statep,
                             state_avx->buf,
                             state_avx->buf_used);
      }
      poly1305_finish_avx(statep, mac);
    }
    return;
  }
#endif  /* POLY1305_HAS_AVX */

#ifdef POLY1305_HAS_MMX
  CRYPTO_poly1305_finish_mmx(statep, mac);
#else
  {
    struct poly1305_state_st *state = (struct poly1305_state_st *)statep;
    uint64_t f0, f1, f2, f3;
    uint32_t g0, g1, g2, g3, g4;
    uint32_t b, nb;

    if (state->buf_used) {
      poly1305_update(statep, state->buf, state->buf_used);
    }

    b = state->h0 >> 26;
    state->h0 = state->h0 & 0x3ffffff;
    state->h1 += b;
    b = state->h1 >> 26;
    state->h1 = state->h1 & 0x3ffffff;
    state->h2 += b;
    b = state->h2 >> 26;
    state->h2 = state->h2 & 0x3ffffff;
    state->h3 += b;
    b = state->h3 >> 26;
    state->h3 = state->h3 & 0x3ffffff;
    state->h4 += b;
    b = state->h4 >> 26;
    state->h4 = state->h4 & 0x3ffffff;
    state->h0 += b * 5;

    g0 = state->h0 + 5;
    b = g0 >> 26;
    g0 &= 0x3ffffff;
    g1 = state->h1 + b;
    b = g1 >> 26;
    g1 &= 0x3ffffff;
    g2 = state->h2 + b;
    b = g2 >> 26;
    g2 &= 0x3ffffff;
    g3 = state->h3 + b;
    b = g3 >> 26;
    g3 &= 0x3ffffff;
    g4 = state->h4 + b - (1 << 26);

    b = (g4 >> 31) - 1;
    nb = ~b;
    state->h0 = (state->h0 & nb) | (g0 & b);
    state->h1 = (state->h1 & nb) | (g1 & b);
    state->h2 = (state->h2 & nb) | (g2 & b);
    state->h3 = (state->h3 & nb) | (g3 & b);
    state->h4 = (state->h4 & nb) | (g4 & b);

    f0 = ((state->h0) | (state->h1 << 26)) + (uint64_t)U8TO32_LE(&state->key[0]);
    f1 = ((state->h1 >> 6) | (state->h2 << 20)) +
        (uint64_t)U8TO32_LE(&state->key[4]);
    f2 = ((state->h2 >> 12) | (state->h3 << 14)) +
        (uint64_t)U8TO32_LE(&state->key[8]);
    f3 = ((state->h3 >> 18) | (state->h4 << 8)) +
        (uint64_t)U8TO32_LE(&state->key[12]);

    U32TO8_LE(&mac[0], f0);
    f1 += (f0 >> 32);
    U32TO8_LE(&mac[4], f1);
    f2 += (f1 >> 32);
    U32TO8_LE(&mac[8], f2);
    f3 += (f2 >> 32);
    U32TO8_LE(&mac[12], f3);
  }
#endif  /* POLY1305_HAS_MMX */
}
