/* Copyright (c) 2015, Google Inc.
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

// This code is mostly taken from the ref10 version of Ed25519 in SUPERCOP
// 20141124 (http://bench.cr.yp.to/supercop.html). That code is released as
// public domain but this file has the ISC license just to keep licencing
// simple.
//
// The field functions are shared by Ed25519 and X25519 where possible.

#include <openssl/curve25519.h>

#include <assert.h>
#include <string.h>

#include <openssl/cpu.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "internal.h"
#include "../internal.h"


static const int64_t kBottom25Bits = INT64_C(0x1ffffff);
static const int64_t kBottom26Bits = INT64_C(0x3ffffff);

static uint64_t load_3(const uint8_t *in) {
  uint64_t result;
  result = (uint64_t)in[0];
  result |= ((uint64_t)in[1]) << 8;
  result |= ((uint64_t)in[2]) << 16;
  return result;
}

static uint64_t load_4(const uint8_t *in) {
  uint64_t result;
  result = (uint64_t)in[0];
  result |= ((uint64_t)in[1]) << 8;
  result |= ((uint64_t)in[2]) << 16;
  result |= ((uint64_t)in[3]) << 24;
  return result;
}

#define assert_fe(f) do { \
  for (unsigned _assert_fe_i = 0; _assert_fe_i< 10; _assert_fe_i++) { \
    assert(f[_assert_fe_i] < 1.125*(1<<(26-(_assert_fe_i&1)))); \
  } \
} while (0)

#define assert_fe_loose(f) do { \
  for (unsigned _assert_fe_i = 0; _assert_fe_i< 10; _assert_fe_i++) { \
    assert(f[_assert_fe_i] < 3.375*(1<<(26-(_assert_fe_i&1)))); \
  } \
} while (0)

static void fe_frombytes_impl(uint32_t h[10], const uint8_t *s) {
  // Ignores top bit of s.
  uint32_t a0 = load_4(s);
  uint32_t a1 = load_4(s+4);
  uint32_t a2 = load_4(s+8);
  uint32_t a3 = load_4(s+12);
  uint32_t a4 = load_4(s+16);
  uint32_t a5 = load_4(s+20);
  uint32_t a6 = load_4(s+24);
  uint32_t a7 = load_4(s+28);
  h[0] = a0&((1<<26)-1);                    // 26 used, 32-26 left.   26
  h[1] = (a0>>26) | ((a1&((1<<19)-1))<< 6); // (32-26) + 19 =  6+19 = 25
  h[2] = (a1>>19) | ((a2&((1<<13)-1))<<13); // (32-19) + 13 = 13+13 = 26
  h[3] = (a2>>13) | ((a3&((1<< 6)-1))<<19); // (32-13) +  6 = 19+ 6 = 25
  h[4] = (a3>> 6);                          // (32- 6)              = 26
  h[5] = a4&((1<<25)-1);                    //                        25
  h[6] = (a4>>25) | ((a5&((1<<19)-1))<< 7); // (32-25) + 19 =  7+19 = 26
  h[7] = (a5>>19) | ((a6&((1<<12)-1))<<13); // (32-19) + 12 = 13+12 = 25
  h[8] = (a6>>12) | ((a7&((1<< 6)-1))<<20); // (32-12) +  6 = 20+ 6 = 26
  h[9] = (a7>> 6)&((1<<25)-1); //                                     25
  assert_fe(h);
}

static void fe_frombytes(fe *h, const uint8_t *s) {
  fe_frombytes_impl(h->v, s);
}

// Preconditions:
//  |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
//
// Write p=2^255-19; q=floor(h/p).
// Basic claim: q = floor(2^(-255)(h + 19 2^(-25)h9 + 2^(-1))).
//
// Proof:
//   Have |h|<=p so |q|<=1 so |19^2 2^(-255) q|<1/4.
//   Also have |h-2^230 h9|<2^231 so |19 2^(-255)(h-2^230 h9)|<1/4.
//
//   Write y=2^(-1)-19^2 2^(-255)q-19 2^(-255)(h-2^230 h9).
//   Then 0<y<1.
//
//   Write r=h-pq.
//   Have 0<=r<=p-1=2^255-20.
//   Thus 0<=r+19(2^-255)r<r+19(2^-255)2^255<=2^255-1.
//
//   Write x=r+19(2^-255)r+y.
//   Then 0<x<2^255 so floor(2^(-255)x) = 0 so floor(q+2^(-255)x) = q.
//
//   Have q+2^(-255)x = 2^(-255)(h + 19 2^(-25) h9 + 2^(-1))
//   so floor(2^(-255)(h + 19 2^(-25) h9 + 2^(-1))) = q.
static void fe_tobytes_impl(uint8_t *s, const uint32_t h[10]) {
  assert_fe_loose(h);
  int32_t h0 = h[0];
  int32_t h1 = h[1];
  int32_t h2 = h[2];
  int32_t h3 = h[3];
  int32_t h4 = h[4];
  int32_t h5 = h[5];
  int32_t h6 = h[6];
  int32_t h7 = h[7];
  int32_t h8 = h[8];
  int32_t h9 = h[9];
  int32_t q;

  q = (19 * h9 + (((int32_t) 1) << 24)) >> 25;
  q = (h0 + q) >> 26;
  q = (h1 + q) >> 25;
  q = (h2 + q) >> 26;
  q = (h3 + q) >> 25;
  q = (h4 + q) >> 26;
  q = (h5 + q) >> 25;
  q = (h6 + q) >> 26;
  q = (h7 + q) >> 25;
  q = (h8 + q) >> 26;
  q = (h9 + q) >> 25;

  // Goal: Output h-(2^255-19)q, which is between 0 and 2^255-20.
  h0 += 19 * q;
  // Goal: Output h-2^255 q, which is between 0 and 2^255-20.

  h1 += h0 >> 26; h0 &= kBottom26Bits;
  h2 += h1 >> 25; h1 &= kBottom25Bits;
  h3 += h2 >> 26; h2 &= kBottom26Bits;
  h4 += h3 >> 25; h3 &= kBottom25Bits;
  h5 += h4 >> 26; h4 &= kBottom26Bits;
  h6 += h5 >> 25; h5 &= kBottom25Bits;
  h7 += h6 >> 26; h6 &= kBottom26Bits;
  h8 += h7 >> 25; h7 &= kBottom25Bits;
  h9 += h8 >> 26; h8 &= kBottom26Bits;
                  h9 &= kBottom25Bits;
                  // h10 = carry9

  // Goal: Output h0+...+2^255 h10-2^255 q, which is between 0 and 2^255-20.
  // Have h0+...+2^230 h9 between 0 and 2^255-1;
  // evidently 2^255 h10-2^255 q = 0.
  // Goal: Output h0+...+2^230 h9.

  s[0] = h0 >> 0;
  s[1] = h0 >> 8;
  s[2] = h0 >> 16;
  s[3] = (h0 >> 24) | ((uint32_t)(h1) << 2);
  s[4] = h1 >> 6;
  s[5] = h1 >> 14;
  s[6] = (h1 >> 22) | ((uint32_t)(h2) << 3);
  s[7] = h2 >> 5;
  s[8] = h2 >> 13;
  s[9] = (h2 >> 21) | ((uint32_t)(h3) << 5);
  s[10] = h3 >> 3;
  s[11] = h3 >> 11;
  s[12] = (h3 >> 19) | ((uint32_t)(h4) << 6);
  s[13] = h4 >> 2;
  s[14] = h4 >> 10;
  s[15] = h4 >> 18;
  s[16] = h5 >> 0;
  s[17] = h5 >> 8;
  s[18] = h5 >> 16;
  s[19] = (h5 >> 24) | ((uint32_t)(h6) << 1);
  s[20] = h6 >> 7;
  s[21] = h6 >> 15;
  s[22] = (h6 >> 23) | ((uint32_t)(h7) << 3);
  s[23] = h7 >> 5;
  s[24] = h7 >> 13;
  s[25] = (h7 >> 21) | ((uint32_t)(h8) << 4);
  s[26] = h8 >> 4;
  s[27] = h8 >> 12;
  s[28] = (h8 >> 20) | ((uint32_t)(h9) << 6);
  s[29] = h9 >> 2;
  s[30] = h9 >> 10;
  s[31] = h9 >> 18;
}

static void fe_tobytes(uint8_t *s, const fe *h) {
  fe_tobytes_impl(s, h->v);
}

static void fe_loose_tobytes(uint8_t *s, const fe_loose *h) {
  fe_tobytes_impl(s, h->v);
}

// h = f
static void fe_copy(fe *h, const fe *f) {
  OPENSSL_memmove(h, f, sizeof(uint32_t) * 10);
}
static void fe_copy_lt(fe_loose *h, const fe *f) {
  OPENSSL_memmove(h, f, sizeof(uint32_t) * 10);
}
#if !defined(OPENSSL_SMALL)
static void fe_copy_ll(fe_loose *h, const fe_loose *f) {
  OPENSSL_memmove(h, f, sizeof(uint32_t) * 10);
}
#endif // !defined(OPENSSL_SMALL)

// h = 0
static void fe_0(fe *h) {
  OPENSSL_memset(h, 0, sizeof(uint32_t) * 10);
}
static void fe_loose_0(fe_loose *h) {
  OPENSSL_memset(h, 0, sizeof(uint32_t) * 10);
}

// h = 1
static void fe_1(fe *h) {
  OPENSSL_memset(h, 0, sizeof(uint32_t) * 10);
  h->v[0] = 1;
}
static void fe_loose_1(fe_loose *h) {
  OPENSSL_memset(h, 0, sizeof(uint32_t) * 10);
  h->v[0] = 1;
}

static void fe_add_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10]) {
  { const uint32_t x20 = in1[9];
  { const uint32_t x21 = in1[8];
  { const uint32_t x19 = in1[7];
  { const uint32_t x17 = in1[6];
  { const uint32_t x15 = in1[5];
  { const uint32_t x13 = in1[4];
  { const uint32_t x11 = in1[3];
  { const uint32_t x9 = in1[2];
  { const uint32_t x7 = in1[1];
  { const uint32_t x5 = in1[0];
  { const uint32_t x38 = in2[9];
  { const uint32_t x39 = in2[8];
  { const uint32_t x37 = in2[7];
  { const uint32_t x35 = in2[6];
  { const uint32_t x33 = in2[5];
  { const uint32_t x31 = in2[4];
  { const uint32_t x29 = in2[3];
  { const uint32_t x27 = in2[2];
  { const uint32_t x25 = in2[1];
  { const uint32_t x23 = in2[0];
  out[0] = (x5 + x23);
  out[1] = (x7 + x25);
  out[2] = (x9 + x27);
  out[3] = (x11 + x29);
  out[4] = (x13 + x31);
  out[5] = (x15 + x33);
  out[6] = (x17 + x35);
  out[7] = (x19 + x37);
  out[8] = (x21 + x39);
  out[9] = (x20 + x38);
  }}}}}}}}}}}}}}}}}}}}
}

// h = f + g
// Can overlap h with f or g.
//
// Preconditions:
//    |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
//    |g| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
//
// Postconditions:
//    |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
static void fe_add(fe_loose *h, const fe *f, const fe *g) {
  assert_fe(f->v);
  assert_fe(g->v);
  fe_add_impl(h->v, f->v, g->v);
  assert_fe_loose(h->v);
}

static void fe_sub_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10]) {
  { const uint32_t x20 = in1[9];
  { const uint32_t x21 = in1[8];
  { const uint32_t x19 = in1[7];
  { const uint32_t x17 = in1[6];
  { const uint32_t x15 = in1[5];
  { const uint32_t x13 = in1[4];
  { const uint32_t x11 = in1[3];
  { const uint32_t x9 = in1[2];
  { const uint32_t x7 = in1[1];
  { const uint32_t x5 = in1[0];
  { const uint32_t x38 = in2[9];
  { const uint32_t x39 = in2[8];
  { const uint32_t x37 = in2[7];
  { const uint32_t x35 = in2[6];
  { const uint32_t x33 = in2[5];
  { const uint32_t x31 = in2[4];
  { const uint32_t x29 = in2[3];
  { const uint32_t x27 = in2[2];
  { const uint32_t x25 = in2[1];
  { const uint32_t x23 = in2[0];
  out[0] = ((0x7ffffda + x5) - x23);
  out[1] = ((0x3fffffe + x7) - x25);
  out[2] = ((0x7fffffe + x9) - x27);
  out[3] = ((0x3fffffe + x11) - x29);
  out[4] = ((0x7fffffe + x13) - x31);
  out[5] = ((0x3fffffe + x15) - x33);
  out[6] = ((0x7fffffe + x17) - x35);
  out[7] = ((0x3fffffe + x19) - x37);
  out[8] = ((0x7fffffe + x21) - x39);
  out[9] = ((0x3fffffe + x20) - x38);
  }}}}}}}}}}}}}}}}}}}}
}

// h = f - g
// Can overlap h with f or g.
//
// Preconditions:
//    |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
//    |g| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
//
// Postconditions:
//    |h| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
static void fe_sub(fe_loose *h, const fe *f, const fe *g) {
  assert_fe(f->v);
  assert_fe(g->v);
  fe_sub_impl(h->v, f->v, g->v);
  assert_fe_loose(h->v);
}

static void fe_carry_impl(uint32_t out[10], const uint32_t in1[10]) {
  { const uint32_t x17 = in1[9];
  { const uint32_t x18 = in1[8];
  { const uint32_t x16 = in1[7];
  { const uint32_t x14 = in1[6];
  { const uint32_t x12 = in1[5];
  { const uint32_t x10 = in1[4];
  { const uint32_t x8 = in1[3];
  { const uint32_t x6 = in1[2];
  { const uint32_t x4 = in1[1];
  { const uint32_t x2 = in1[0];
  { uint32_t x19 = (x2 >> 0x1a);
  { uint32_t x20 = (x2 & 0x3ffffff);
  { uint32_t x21 = (x19 + x4);
  { uint32_t x22 = (x21 >> 0x19);
  { uint32_t x23 = (x21 & 0x1ffffff);
  { uint32_t x24 = (x22 + x6);
  { uint32_t x25 = (x24 >> 0x1a);
  { uint32_t x26 = (x24 & 0x3ffffff);
  { uint32_t x27 = (x25 + x8);
  { uint32_t x28 = (x27 >> 0x19);
  { uint32_t x29 = (x27 & 0x1ffffff);
  { uint32_t x30 = (x28 + x10);
  { uint32_t x31 = (x30 >> 0x1a);
  { uint32_t x32 = (x30 & 0x3ffffff);
  { uint32_t x33 = (x31 + x12);
  { uint32_t x34 = (x33 >> 0x19);
  { uint32_t x35 = (x33 & 0x1ffffff);
  { uint32_t x36 = (x34 + x14);
  { uint32_t x37 = (x36 >> 0x1a);
  { uint32_t x38 = (x36 & 0x3ffffff);
  { uint32_t x39 = (x37 + x16);
  { uint32_t x40 = (x39 >> 0x19);
  { uint32_t x41 = (x39 & 0x1ffffff);
  { uint32_t x42 = (x40 + x18);
  { uint32_t x43 = (x42 >> 0x1a);
  { uint32_t x44 = (x42 & 0x3ffffff);
  { uint32_t x45 = (x43 + x17);
  { uint32_t x46 = (x45 >> 0x19);
  { uint32_t x47 = (x45 & 0x1ffffff);
  { uint32_t x48 = (x20 + (0x13 * x46));
  { uint32_t x49 = (x48 >> 0x1a);
  { uint32_t x50 = (x48 & 0x3ffffff);
  { uint32_t x51 = (x49 + x23);
  { uint32_t x52 = (x51 >> 0x19);
  { uint32_t x53 = (x51 & 0x1ffffff);
  out[0] = x50;
  out[1] = x53;
  out[2] = (x52 + x26);
  out[3] = x29;
  out[4] = x32;
  out[5] = x35;
  out[6] = x38;
  out[7] = x41;
  out[8] = x44;
  out[9] = x47;
  }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
}

static void fe_carry(fe *h, const fe_loose* f) {
  assert_fe_loose(f->v);
  fe_carry_impl(h->v, f->v);
  assert_fe(h->v);
}

static void fe_mul_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10]) {
  assert_fe_loose(in1);
  assert_fe_loose(in2);
  { const uint32_t x20 = in1[9];
  { const uint32_t x21 = in1[8];
  { const uint32_t x19 = in1[7];
  { const uint32_t x17 = in1[6];
  { const uint32_t x15 = in1[5];
  { const uint32_t x13 = in1[4];
  { const uint32_t x11 = in1[3];
  { const uint32_t x9 = in1[2];
  { const uint32_t x7 = in1[1];
  { const uint32_t x5 = in1[0];
  { const uint32_t x38 = in2[9];
  { const uint32_t x39 = in2[8];
  { const uint32_t x37 = in2[7];
  { const uint32_t x35 = in2[6];
  { const uint32_t x33 = in2[5];
  { const uint32_t x31 = in2[4];
  { const uint32_t x29 = in2[3];
  { const uint32_t x27 = in2[2];
  { const uint32_t x25 = in2[1];
  { const uint32_t x23 = in2[0];
  { uint64_t x40 = ((uint64_t)x23 * x5);
  { uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
  { uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
  { uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
  { uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
  { uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
  { uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
  { uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
  { uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
  { uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
  { uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
  { uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
  { uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
  { uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
  { uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
  { uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
  { uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
  { uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
  { uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
  { uint64_t x59 = (x48 + (x58 << 0x4));
  { uint64_t x60 = (x59 + (x58 << 0x1));
  { uint64_t x61 = (x60 + x58);
  { uint64_t x62 = (x47 + (x57 << 0x4));
  { uint64_t x63 = (x62 + (x57 << 0x1));
  { uint64_t x64 = (x63 + x57);
  { uint64_t x65 = (x46 + (x56 << 0x4));
  { uint64_t x66 = (x65 + (x56 << 0x1));
  { uint64_t x67 = (x66 + x56);
  { uint64_t x68 = (x45 + (x55 << 0x4));
  { uint64_t x69 = (x68 + (x55 << 0x1));
  { uint64_t x70 = (x69 + x55);
  { uint64_t x71 = (x44 + (x54 << 0x4));
  { uint64_t x72 = (x71 + (x54 << 0x1));
  { uint64_t x73 = (x72 + x54);
  { uint64_t x74 = (x43 + (x53 << 0x4));
  { uint64_t x75 = (x74 + (x53 << 0x1));
  { uint64_t x76 = (x75 + x53);
  { uint64_t x77 = (x42 + (x52 << 0x4));
  { uint64_t x78 = (x77 + (x52 << 0x1));
  { uint64_t x79 = (x78 + x52);
  { uint64_t x80 = (x41 + (x51 << 0x4));
  { uint64_t x81 = (x80 + (x51 << 0x1));
  { uint64_t x82 = (x81 + x51);
  { uint64_t x83 = (x40 + (x50 << 0x4));
  { uint64_t x84 = (x83 + (x50 << 0x1));
  { uint64_t x85 = (x84 + x50);
  { uint64_t x86 = (x85 >> 0x1a);
  { uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
  { uint64_t x88 = (x86 + x82);
  { uint64_t x89 = (x88 >> 0x19);
  { uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
  { uint64_t x91 = (x89 + x79);
  { uint64_t x92 = (x91 >> 0x1a);
  { uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
  { uint64_t x94 = (x92 + x76);
  { uint64_t x95 = (x94 >> 0x19);
  { uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
  { uint64_t x97 = (x95 + x73);
  { uint64_t x98 = (x97 >> 0x1a);
  { uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
  { uint64_t x100 = (x98 + x70);
  { uint64_t x101 = (x100 >> 0x19);
  { uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
  { uint64_t x103 = (x101 + x67);
  { uint64_t x104 = (x103 >> 0x1a);
  { uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
  { uint64_t x106 = (x104 + x64);
  { uint64_t x107 = (x106 >> 0x19);
  { uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
  { uint64_t x109 = (x107 + x61);
  { uint64_t x110 = (x109 >> 0x1a);
  { uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
  { uint64_t x112 = (x110 + x49);
  { uint64_t x113 = (x112 >> 0x19);
  { uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
  { uint64_t x115 = (x87 + (0x13 * x113));
  { uint32_t x116 = (uint32_t) (x115 >> 0x1a);
  { uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
  { uint32_t x118 = (x116 + x90);
  { uint32_t x119 = (x118 >> 0x19);
  { uint32_t x120 = (x118 & 0x1ffffff);
  out[0] = x117;
  out[1] = x120;
  out[2] = (x119 + x93);
  out[3] = x96;
  out[4] = x99;
  out[5] = x102;
  out[6] = x105;
  out[7] = x108;
  out[8] = x111;
  out[9] = x114;
  }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
  assert_fe(out);
}

static void fe_mul_ltt(fe_loose *h, const fe *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_llt(fe_loose *h, const fe_loose *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_ttt(fe *h, const fe *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_tlt(fe *h, const fe_loose *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_ttl(fe *h, const fe *f, const fe_loose *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_tll(fe *h, const fe_loose *f, const fe_loose *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_sqr_impl(uint32_t out[10], const uint32_t in1[10]) {
  assert_fe_loose(in1);
  { const uint32_t x17 = in1[9];
  { const uint32_t x18 = in1[8];
  { const uint32_t x16 = in1[7];
  { const uint32_t x14 = in1[6];
  { const uint32_t x12 = in1[5];
  { const uint32_t x10 = in1[4];
  { const uint32_t x8 = in1[3];
  { const uint32_t x6 = in1[2];
  { const uint32_t x4 = in1[1];
  { const uint32_t x2 = in1[0];
  { uint64_t x19 = ((uint64_t)x2 * x2);
  { uint64_t x20 = ((uint64_t)(0x2 * x2) * x4);
  { uint64_t x21 = (0x2 * (((uint64_t)x4 * x4) + ((uint64_t)x2 * x6)));
  { uint64_t x22 = (0x2 * (((uint64_t)x4 * x6) + ((uint64_t)x2 * x8)));
  { uint64_t x23 = ((((uint64_t)x6 * x6) + ((uint64_t)(0x4 * x4) * x8)) + ((uint64_t)(0x2 * x2) * x10));
  { uint64_t x24 = (0x2 * ((((uint64_t)x6 * x8) + ((uint64_t)x4 * x10)) + ((uint64_t)x2 * x12)));
  { uint64_t x25 = (0x2 * (((((uint64_t)x8 * x8) + ((uint64_t)x6 * x10)) + ((uint64_t)x2 * x14)) + ((uint64_t)(0x2 * x4) * x12)));
  { uint64_t x26 = (0x2 * (((((uint64_t)x8 * x10) + ((uint64_t)x6 * x12)) + ((uint64_t)x4 * x14)) + ((uint64_t)x2 * x16)));
  { uint64_t x27 = (((uint64_t)x10 * x10) + (0x2 * ((((uint64_t)x6 * x14) + ((uint64_t)x2 * x18)) + (0x2 * (((uint64_t)x4 * x16) + ((uint64_t)x8 * x12))))));
  { uint64_t x28 = (0x2 * ((((((uint64_t)x10 * x12) + ((uint64_t)x8 * x14)) + ((uint64_t)x6 * x16)) + ((uint64_t)x4 * x18)) + ((uint64_t)x2 * x17)));
  { uint64_t x29 = (0x2 * (((((uint64_t)x12 * x12) + ((uint64_t)x10 * x14)) + ((uint64_t)x6 * x18)) + (0x2 * (((uint64_t)x8 * x16) + ((uint64_t)x4 * x17)))));
  { uint64_t x30 = (0x2 * (((((uint64_t)x12 * x14) + ((uint64_t)x10 * x16)) + ((uint64_t)x8 * x18)) + ((uint64_t)x6 * x17)));
  { uint64_t x31 = (((uint64_t)x14 * x14) + (0x2 * (((uint64_t)x10 * x18) + (0x2 * (((uint64_t)x12 * x16) + ((uint64_t)x8 * x17))))));
  { uint64_t x32 = (0x2 * ((((uint64_t)x14 * x16) + ((uint64_t)x12 * x18)) + ((uint64_t)x10 * x17)));
  { uint64_t x33 = (0x2 * ((((uint64_t)x16 * x16) + ((uint64_t)x14 * x18)) + ((uint64_t)(0x2 * x12) * x17)));
  { uint64_t x34 = (0x2 * (((uint64_t)x16 * x18) + ((uint64_t)x14 * x17)));
  { uint64_t x35 = (((uint64_t)x18 * x18) + ((uint64_t)(0x4 * x16) * x17));
  { uint64_t x36 = ((uint64_t)(0x2 * x18) * x17);
  { uint64_t x37 = ((uint64_t)(0x2 * x17) * x17);
  { uint64_t x38 = (x27 + (x37 << 0x4));
  { uint64_t x39 = (x38 + (x37 << 0x1));
  { uint64_t x40 = (x39 + x37);
  { uint64_t x41 = (x26 + (x36 << 0x4));
  { uint64_t x42 = (x41 + (x36 << 0x1));
  { uint64_t x43 = (x42 + x36);
  { uint64_t x44 = (x25 + (x35 << 0x4));
  { uint64_t x45 = (x44 + (x35 << 0x1));
  { uint64_t x46 = (x45 + x35);
  { uint64_t x47 = (x24 + (x34 << 0x4));
  { uint64_t x48 = (x47 + (x34 << 0x1));
  { uint64_t x49 = (x48 + x34);
  { uint64_t x50 = (x23 + (x33 << 0x4));
  { uint64_t x51 = (x50 + (x33 << 0x1));
  { uint64_t x52 = (x51 + x33);
  { uint64_t x53 = (x22 + (x32 << 0x4));
  { uint64_t x54 = (x53 + (x32 << 0x1));
  { uint64_t x55 = (x54 + x32);
  { uint64_t x56 = (x21 + (x31 << 0x4));
  { uint64_t x57 = (x56 + (x31 << 0x1));
  { uint64_t x58 = (x57 + x31);
  { uint64_t x59 = (x20 + (x30 << 0x4));
  { uint64_t x60 = (x59 + (x30 << 0x1));
  { uint64_t x61 = (x60 + x30);
  { uint64_t x62 = (x19 + (x29 << 0x4));
  { uint64_t x63 = (x62 + (x29 << 0x1));
  { uint64_t x64 = (x63 + x29);
  { uint64_t x65 = (x64 >> 0x1a);
  { uint32_t x66 = ((uint32_t)x64 & 0x3ffffff);
  { uint64_t x67 = (x65 + x61);
  { uint64_t x68 = (x67 >> 0x19);
  { uint32_t x69 = ((uint32_t)x67 & 0x1ffffff);
  { uint64_t x70 = (x68 + x58);
  { uint64_t x71 = (x70 >> 0x1a);
  { uint32_t x72 = ((uint32_t)x70 & 0x3ffffff);
  { uint64_t x73 = (x71 + x55);
  { uint64_t x74 = (x73 >> 0x19);
  { uint32_t x75 = ((uint32_t)x73 & 0x1ffffff);
  { uint64_t x76 = (x74 + x52);
  { uint64_t x77 = (x76 >> 0x1a);
  { uint32_t x78 = ((uint32_t)x76 & 0x3ffffff);
  { uint64_t x79 = (x77 + x49);
  { uint64_t x80 = (x79 >> 0x19);
  { uint32_t x81 = ((uint32_t)x79 & 0x1ffffff);
  { uint64_t x82 = (x80 + x46);
  { uint64_t x83 = (x82 >> 0x1a);
  { uint32_t x84 = ((uint32_t)x82 & 0x3ffffff);
  { uint64_t x85 = (x83 + x43);
  { uint64_t x86 = (x85 >> 0x19);
  { uint32_t x87 = ((uint32_t)x85 & 0x1ffffff);
  { uint64_t x88 = (x86 + x40);
  { uint64_t x89 = (x88 >> 0x1a);
  { uint32_t x90 = ((uint32_t)x88 & 0x3ffffff);
  { uint64_t x91 = (x89 + x28);
  { uint64_t x92 = (x91 >> 0x19);
  { uint32_t x93 = ((uint32_t)x91 & 0x1ffffff);
  { uint64_t x94 = (x66 + (0x13 * x92));
  { uint32_t x95 = (uint32_t) (x94 >> 0x1a);
  { uint32_t x96 = ((uint32_t)x94 & 0x3ffffff);
  { uint32_t x97 = (x95 + x69);
  { uint32_t x98 = (x97 >> 0x19);
  { uint32_t x99 = (x97 & 0x1ffffff);
  out[0] = x96;
  out[1] = x99;
  out[2] = (x98 + x72);
  out[3] = x75;
  out[4] = x78;
  out[5] = x81;
  out[6] = x84;
  out[7] = x87;
  out[8] = x90;
  out[9] = x93;
  }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
  assert_fe(out);
}

static void fe_sq_tl(fe *h, const fe_loose *f) {
  fe_sqr_impl(h->v, f->v);
}

static void fe_sq_tt(fe *h, const fe *f) {
  fe_sqr_impl(h->v, f->v);
}

static void fe_loose_invert(fe *out, const fe_loose *z) {
  fe t0;
  fe t1;
  fe t2;
  fe t3;
  int i;

  fe_sq_tl(&t0, z);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_tlt(&t1, z, &t1);
  fe_mul_ttt(&t0, &t0, &t1);
  fe_sq_tt(&t2, &t0);
  fe_mul_ttt(&t1, &t1, &t2);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t2, &t2, &t1);
  fe_sq_tt(&t3, &t2);
  for (i = 1; i < 20; ++i) {
    fe_sq_tt(&t3, &t3);
  }
  fe_mul_ttt(&t2, &t3, &t2);
  fe_sq_tt(&t2, &t2);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t2, &t2, &t1);
  fe_sq_tt(&t3, &t2);
  for (i = 1; i < 100; ++i) {
    fe_sq_tt(&t3, &t3);
  }
  fe_mul_ttt(&t2, &t3, &t2);
  fe_sq_tt(&t2, &t2);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(out, &t1, &t0);
}

static void fe_invert(fe *out, const fe *z) {
  fe_loose l;
  fe_copy_lt(&l, z);
  fe_loose_invert(out, &l);
}

static void fe_neg_impl(uint32_t out[10], const uint32_t in2[10]) {
  { const uint32_t x20 = 0;
  { const uint32_t x21 = 0;
  { const uint32_t x19 = 0;
  { const uint32_t x17 = 0;
  { const uint32_t x15 = 0;
  { const uint32_t x13 = 0;
  { const uint32_t x11 = 0;
  { const uint32_t x9 = 0;
  { const uint32_t x7 = 0;
  { const uint32_t x5 = 0;
  { const uint32_t x38 = in2[9];
  { const uint32_t x39 = in2[8];
  { const uint32_t x37 = in2[7];
  { const uint32_t x35 = in2[6];
  { const uint32_t x33 = in2[5];
  { const uint32_t x31 = in2[4];
  { const uint32_t x29 = in2[3];
  { const uint32_t x27 = in2[2];
  { const uint32_t x25 = in2[1];
  { const uint32_t x23 = in2[0];
  out[0] = ((0x7ffffda + x5) - x23);
  out[1] = ((0x3fffffe + x7) - x25);
  out[2] = ((0x7fffffe + x9) - x27);
  out[3] = ((0x3fffffe + x11) - x29);
  out[4] = ((0x7fffffe + x13) - x31);
  out[5] = ((0x3fffffe + x15) - x33);
  out[6] = ((0x7fffffe + x17) - x35);
  out[7] = ((0x3fffffe + x19) - x37);
  out[8] = ((0x7fffffe + x21) - x39);
  out[9] = ((0x3fffffe + x20) - x38);
  }}}}}}}}}}}}}}}}}}}}
}

// h = -f
//
// Preconditions:
//    |f| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
//
// Postconditions:
//    |h| bounded by 1.1*2^25,1.1*2^24,1.1*2^25,1.1*2^24,etc.
static void fe_neg(fe_loose *h, const fe *f) {
  assert_fe(f->v);
  fe_neg_impl(h->v, f->v);
  assert_fe_loose(h->v);
}

// Replace (f,g) with (g,g) if b == 1;
// replace (f,g) with (f,g) if b == 0.
//
// Preconditions: b in {0,1}.
static void fe_cmov(fe_loose *f, const fe_loose *g, unsigned b) {
  b = 0-b;
  unsigned i;
  for (i = 0; i < 10; i++) {
    uint32_t x = f->v[i] ^ g->v[i];
    x &= b;
    f->v[i] ^= x;
  }
}

// return 0 if f == 0
// return 1 if f != 0
//
// Preconditions:
//    |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
static int fe_isnonzero(const fe_loose *f) {
  uint8_t s[32];
  fe_loose_tobytes(s, f);

  static const uint8_t zero[32] = {0};
  return CRYPTO_memcmp(s, zero, sizeof(zero)) != 0;
}

// return 1 if f is in {1,3,5,...,q-2}
// return 0 if f is in {0,2,4,...,q-1}
//
// Preconditions:
//    |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
static int fe_isnegative(const fe *f) {
  uint8_t s[32];
  fe_tobytes(s, f);
  return s[0] & 1;
}

// NOTE: based on fiat-crypto fe_mul, edited for in2=2*in1
static void fe_sq2_impl(uint32_t out[10], const uint32_t in1[10]) {
  assert_fe_loose(in1);
  { const uint32_t x20 = in1[9];
  { const uint32_t x21 = in1[8];
  { const uint32_t x19 = in1[7];
  { const uint32_t x17 = in1[6];
  { const uint32_t x15 = in1[5];
  { const uint32_t x13 = in1[4];
  { const uint32_t x11 = in1[3];
  { const uint32_t x9 = in1[2];
  { const uint32_t x7 = in1[1];
  { const uint32_t x5 = in1[0];
  { const uint32_t x38 = 2*in1[9];
  { const uint32_t x39 = 2*in1[8];
  { const uint32_t x37 = 2*in1[7];
  { const uint32_t x35 = 2*in1[6];
  { const uint32_t x33 = 2*in1[5];
  { const uint32_t x31 = 2*in1[4];
  { const uint32_t x29 = 2*in1[3];
  { const uint32_t x27 = 2*in1[2];
  { const uint32_t x25 = 2*in1[1];
  { const uint32_t x23 = 2*in1[0];
  { uint64_t x40 = ((uint64_t)x23 * x5);
  { uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
  { uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
  { uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
  { uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
  { uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
  { uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
  { uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
  { uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
  { uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
  { uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
  { uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
  { uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
  { uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
  { uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
  { uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
  { uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
  { uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
  { uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
  { uint64_t x59 = (x48 + (x58 << 0x4));
  { uint64_t x60 = (x59 + (x58 << 0x1));
  { uint64_t x61 = (x60 + x58);
  { uint64_t x62 = (x47 + (x57 << 0x4));
  { uint64_t x63 = (x62 + (x57 << 0x1));
  { uint64_t x64 = (x63 + x57);
  { uint64_t x65 = (x46 + (x56 << 0x4));
  { uint64_t x66 = (x65 + (x56 << 0x1));
  { uint64_t x67 = (x66 + x56);
  { uint64_t x68 = (x45 + (x55 << 0x4));
  { uint64_t x69 = (x68 + (x55 << 0x1));
  { uint64_t x70 = (x69 + x55);
  { uint64_t x71 = (x44 + (x54 << 0x4));
  { uint64_t x72 = (x71 + (x54 << 0x1));
  { uint64_t x73 = (x72 + x54);
  { uint64_t x74 = (x43 + (x53 << 0x4));
  { uint64_t x75 = (x74 + (x53 << 0x1));
  { uint64_t x76 = (x75 + x53);
  { uint64_t x77 = (x42 + (x52 << 0x4));
  { uint64_t x78 = (x77 + (x52 << 0x1));
  { uint64_t x79 = (x78 + x52);
  { uint64_t x80 = (x41 + (x51 << 0x4));
  { uint64_t x81 = (x80 + (x51 << 0x1));
  { uint64_t x82 = (x81 + x51);
  { uint64_t x83 = (x40 + (x50 << 0x4));
  { uint64_t x84 = (x83 + (x50 << 0x1));
  { uint64_t x85 = (x84 + x50);
  { uint64_t x86 = (x85 >> 0x1a);
  { uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
  { uint64_t x88 = (x86 + x82);
  { uint64_t x89 = (x88 >> 0x19);
  { uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
  { uint64_t x91 = (x89 + x79);
  { uint64_t x92 = (x91 >> 0x1a);
  { uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
  { uint64_t x94 = (x92 + x76);
  { uint64_t x95 = (x94 >> 0x19);
  { uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
  { uint64_t x97 = (x95 + x73);
  { uint64_t x98 = (x97 >> 0x1a);
  { uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
  { uint64_t x100 = (x98 + x70);
  { uint64_t x101 = (x100 >> 0x19);
  { uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
  { uint64_t x103 = (x101 + x67);
  { uint64_t x104 = (x103 >> 0x1a);
  { uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
  { uint64_t x106 = (x104 + x64);
  { uint64_t x107 = (x106 >> 0x19);
  { uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
  { uint64_t x109 = (x107 + x61);
  { uint64_t x110 = (x109 >> 0x1a);
  { uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
  { uint64_t x112 = (x110 + x49);
  { uint64_t x113 = (x112 >> 0x19);
  { uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
  { uint64_t x115 = (x87 + (0x13 * x113));
  { uint32_t x116 = (uint32_t) (x115 >> 0x1a);
  { uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
  { uint32_t x118 = (x116 + x90);
  { uint32_t x119 = (x118 >> 0x19);
  { uint32_t x120 = (x118 & 0x1ffffff);
  out[0] = x117;
  out[1] = x120;
  out[2] = (x119 + x93);
  out[3] = x96;
  out[4] = x99;
  out[5] = x102;
  out[6] = x105;
  out[7] = x108;
  out[8] = x111;
  out[9] = x114;
  }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
  assert_fe(out);
}

static void fe_sq2_tt(fe *h, const fe *f) {
  fe_sq2_impl(h->v, f->v);
}

static void fe_pow22523(fe *out, const fe *z) {
  fe t0;
  fe t1;
  fe t2;
  int i;

  fe_sq_tt(&t0, z);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, z, &t1);
  fe_mul_ttt(&t0, &t0, &t1);
  fe_sq_tt(&t0, &t0);
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, &t1, &t0);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 20; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, &t1, &t0);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 100; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t0, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t0, &t0);
  }
  fe_mul_ttt(out, &t0, z);
}

void x25519_ge_tobytes(uint8_t *s, const ge_p2 *h) {
  fe recip;
  fe x;
  fe y;

  fe_invert(&recip, &h->Z);
  fe_mul_ttt(&x, &h->X, &recip);
  fe_mul_ttt(&y, &h->Y, &recip);
  fe_tobytes(s, &y);
  s[31] ^= fe_isnegative(&x) << 7;
}

static void ge_p3_tobytes(uint8_t *s, const ge_p3 *h) {
  fe recip;
  fe x;
  fe y;

  fe_invert(&recip, &h->Z);
  fe_mul_ttt(&x, &h->X, &recip);
  fe_mul_ttt(&y, &h->Y, &recip);
  fe_tobytes(s, &y);
  s[31] ^= fe_isnegative(&x) << 7;
}

static const fe d = {{56195235, 13857412, 51736253, 6949390,   114729,
                     24766616,  60832955, 30306712,  48412415, 21499315}};

static const fe sqrtm1 = {{34513072, 25610706,  9377949,  3500415, 12389472,
                          33281959,   41962654, 31548777, 326685,  11406482}};

int x25519_ge_frombytes_vartime(ge_p3 *h, const uint8_t *s) {
  fe u;
  fe_loose v;
  fe v3;
  fe vxx;
  fe_loose check;

  fe_frombytes(&h->Y, s);
  fe_1(&h->Z);
  fe_sq_tt(&v3, &h->Y);
  fe_mul_ttt(&vxx, &v3, &d);
  fe_sub(&v, &v3, &h->Z);  // u = y^2-1
  fe_carry(&u, &v);
  fe_add(&v, &vxx, &h->Z);  // v = dy^2+1

  fe_sq_tl(&v3, &v);
  fe_mul_ttl(&v3, &v3, &v);  // v3 = v^3
  fe_sq_tt(&h->X, &v3);
  fe_mul_ttl(&h->X, &h->X, &v);
  fe_mul_ttt(&h->X, &h->X, &u);  // x = uv^7

  fe_pow22523(&h->X, &h->X);  // x = (uv^7)^((q-5)/8)
  fe_mul_ttt(&h->X, &h->X, &v3);
  fe_mul_ttt(&h->X, &h->X, &u);  // x = uv^3(uv^7)^((q-5)/8)

  fe_sq_tt(&vxx, &h->X);
  fe_mul_ttl(&vxx, &vxx, &v);
  fe_sub(&check, &vxx, &u);
  if (fe_isnonzero(&check)) {
    fe_add(&check, &vxx, &u);
    if (fe_isnonzero(&check)) {
      return -1;
    }
    fe_mul_ttt(&h->X, &h->X, &sqrtm1);
  }

  if (fe_isnegative(&h->X) != (s[31] >> 7)) {
    fe_loose t;
    fe_neg(&t, &h->X);
    fe_carry(&h->X, &t);
  }

  fe_mul_ttt(&h->T, &h->X, &h->Y);
  return 0;
}

static void ge_p2_0(ge_p2 *h) {
  fe_0(&h->X);
  fe_1(&h->Y);
  fe_1(&h->Z);
}

static void ge_p3_0(ge_p3 *h) {
  fe_0(&h->X);
  fe_1(&h->Y);
  fe_1(&h->Z);
  fe_0(&h->T);
}

static void ge_cached_0(ge_cached *h) {
  fe_loose_1(&h->YplusX);
  fe_loose_1(&h->YminusX);
  fe_loose_1(&h->Z);
  fe_loose_0(&h->T2d);
}

static void ge_precomp_0(ge_precomp *h) {
  fe_loose_1(&h->yplusx);
  fe_loose_1(&h->yminusx);
  fe_loose_0(&h->xy2d);
}

// r = p
static void ge_p3_to_p2(ge_p2 *r, const ge_p3 *p) {
  fe_copy(&r->X, &p->X);
  fe_copy(&r->Y, &p->Y);
  fe_copy(&r->Z, &p->Z);
}

static const fe d2 = {{45281625, 27714825,  36363642, 13898781, 229458,
                      15978800,  54557047, 27058993,  29715967, 9444199}};

// r = p
void x25519_ge_p3_to_cached(ge_cached *r, const ge_p3 *p) {
  fe_add(&r->YplusX, &p->Y, &p->X);
  fe_sub(&r->YminusX, &p->Y, &p->X);
  fe_copy_lt(&r->Z, &p->Z);
  fe_mul_ltt(&r->T2d, &p->T, &d2);
}

// r = p
void x25519_ge_p1p1_to_p2(ge_p2 *r, const ge_p1p1 *p) {
  fe_mul_tll(&r->X, &p->X, &p->T);
  fe_mul_tll(&r->Y, &p->Y, &p->Z);
  fe_mul_tll(&r->Z, &p->Z, &p->T);
}

// r = p
void x25519_ge_p1p1_to_p3(ge_p3 *r, const ge_p1p1 *p) {
  fe_mul_tll(&r->X, &p->X, &p->T);
  fe_mul_tll(&r->Y, &p->Y, &p->Z);
  fe_mul_tll(&r->Z, &p->Z, &p->T);
  fe_mul_tll(&r->T, &p->X, &p->Y);
}

// r = p
static void ge_p1p1_to_cached(ge_cached *r, const ge_p1p1 *p) {
  ge_p3 t;
  x25519_ge_p1p1_to_p3(&t, p);
  x25519_ge_p3_to_cached(r, &t);
}

// r = 2 * p
static void ge_p2_dbl(ge_p1p1 *r, const ge_p2 *p) {
  fe trX, trZ, trT;
  fe t0;

  fe_sq_tt(&trX, &p->X);
  fe_sq_tt(&trZ, &p->Y);
  fe_sq2_tt(&trT, &p->Z);
  fe_add(&r->Y, &p->X, &p->Y);
  fe_sq_tl(&t0, &r->Y);

  fe_add(&r->Y, &trZ, &trX);
  fe_sub(&r->Z, &trZ, &trX);
  fe_carry(&trZ, &r->Y);
  fe_sub(&r->X, &t0, &trZ);
  fe_carry(&trZ, &r->Z);
  fe_sub(&r->T, &trT, &trZ);
}

// r = 2 * p
static void ge_p3_dbl(ge_p1p1 *r, const ge_p3 *p) {
  ge_p2 q;
  ge_p3_to_p2(&q, p);
  ge_p2_dbl(r, &q);
}

// r = p + q
static void ge_madd(ge_p1p1 *r, const ge_p3 *p, const ge_precomp *q) {
  fe trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->yplusx);
  fe_mul_tll(&trY, &r->Y, &q->yminusx);
  fe_mul_tlt(&trT, &q->xy2d, &p->T);
  fe_add(&r->T, &p->Z, &p->Z);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_add(&r->Z, &trZ, &trT);
  fe_sub(&r->T, &trZ, &trT);
}

// r = p - q
static void ge_msub(ge_p1p1 *r, const ge_p3 *p, const ge_precomp *q) {
  fe trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->yminusx);
  fe_mul_tll(&trY, &r->Y, &q->yplusx);
  fe_mul_tlt(&trT, &q->xy2d, &p->T);
  fe_add(&r->T, &p->Z, &p->Z);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_sub(&r->Z, &trZ, &trT);
  fe_add(&r->T, &trZ, &trT);
}

// r = p + q
void x25519_ge_add(ge_p1p1 *r, const ge_p3 *p, const ge_cached *q) {
  fe trX, trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->YplusX);
  fe_mul_tll(&trY, &r->Y, &q->YminusX);
  fe_mul_tlt(&trT, &q->T2d, &p->T);
  fe_mul_ttl(&trX, &p->Z, &q->Z);
  fe_add(&r->T, &trX, &trX);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_add(&r->Z, &trZ, &trT);
  fe_sub(&r->T, &trZ, &trT);
}

// r = p - q
void x25519_ge_sub(ge_p1p1 *r, const ge_p3 *p, const ge_cached *q) {
  fe trX, trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->YminusX);
  fe_mul_tll(&trY, &r->Y, &q->YplusX);
  fe_mul_tlt(&trT, &q->T2d, &p->T);
  fe_mul_ttl(&trX, &p->Z, &q->Z);
  fe_add(&r->T, &trX, &trX);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_sub(&r->Z, &trZ, &trT);
  fe_add(&r->T, &trZ, &trT);
}

static uint8_t equal(signed char b, signed char c) {
  uint8_t ub = b;
  uint8_t uc = c;
  uint8_t x = ub ^ uc;  // 0: yes; 1..255: no
  uint32_t y = x;       // 0: yes; 1..255: no
  y -= 1;               // 4294967295: yes; 0..254: no
  y >>= 31;             // 1: yes; 0: no
  return y;
}

static void cmov(ge_precomp *t, const ge_precomp *u, uint8_t b) {
  fe_cmov(&t->yplusx, &u->yplusx, b);
  fe_cmov(&t->yminusx, &u->yminusx, b);
  fe_cmov(&t->xy2d, &u->xy2d, b);
}

void x25519_ge_scalarmult_small_precomp(
    ge_p3 *h, const uint8_t a[32], const uint8_t precomp_table[15 * 2 * 32]) {
  // precomp_table is first expanded into matching |ge_precomp|
  // elements.
  ge_precomp multiples[15];

  unsigned i;
  for (i = 0; i < 15; i++) {
    const uint8_t *bytes = &precomp_table[i*(2 * 32)];
    fe x, y;
    fe_frombytes(&x, bytes);
    fe_frombytes(&y, bytes + 32);

    ge_precomp *out = &multiples[i];
    fe_add(&out->yplusx, &y, &x);
    fe_sub(&out->yminusx, &y, &x);
    fe_mul_ltt(&out->xy2d, &x, &y);
    fe_mul_llt(&out->xy2d, &out->xy2d, &d2);
  }

  // See the comment above |k25519SmallPrecomp| about the structure of the
  // precomputed elements. This loop does 64 additions and 64 doublings to
  // calculate the result.
  ge_p3_0(h);

  for (i = 63; i < 64; i--) {
    unsigned j;
    signed char index = 0;

    for (j = 0; j < 4; j++) {
      const uint8_t bit = 1 & (a[(8 * j) + (i / 8)] >> (i & 7));
      index |= (bit << j);
    }

    ge_precomp e;
    ge_precomp_0(&e);

    for (j = 1; j < 16; j++) {
      cmov(&e, &multiples[j-1], equal(index, j));
    }

    ge_cached cached;
    ge_p1p1 r;
    x25519_ge_p3_to_cached(&cached, h);
    x25519_ge_add(&r, h, &cached);
    x25519_ge_p1p1_to_p3(h, &r);

    ge_madd(&r, h, &e);
    x25519_ge_p1p1_to_p3(h, &r);
  }
}

#if defined(OPENSSL_SMALL)

// This block of code replaces the standard base-point table with a much smaller
// one. The standard table is 30,720 bytes while this one is just 960.
//
// This table contains 15 pairs of group elements, (x, y), where each field
// element is serialised with |fe_tobytes|. If |i| is the index of the group
// element then consider i+1 as a four-bit number: (i₀, i₁, i₂, i₃) (where i₀
// is the most significant bit). The value of the group element is then:
// (i₀×2^192 + i₁×2^128 + i₂×2^64 + i₃)G, where G is the generator.
static const uint8_t k25519SmallPrecomp[15 * 2 * 32] = {
    0x1a, 0xd5, 0x25, 0x8f, 0x60, 0x2d, 0x56, 0xc9, 0xb2, 0xa7, 0x25, 0x95,
    0x60, 0xc7, 0x2c, 0x69, 0x5c, 0xdc, 0xd6, 0xfd, 0x31, 0xe2, 0xa4, 0xc0,
    0xfe, 0x53, 0x6e, 0xcd, 0xd3, 0x36, 0x69, 0x21, 0x58, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x02, 0xa2, 0xed, 0xf4, 0x8f, 0x6b, 0x0b, 0x3e,
    0xeb, 0x35, 0x1a, 0xd5, 0x7e, 0xdb, 0x78, 0x00, 0x96, 0x8a, 0xa0, 0xb4,
    0xcf, 0x60, 0x4b, 0xd4, 0xd5, 0xf9, 0x2d, 0xbf, 0x88, 0xbd, 0x22, 0x62,
    0x13, 0x53, 0xe4, 0x82, 0x57, 0xfa, 0x1e, 0x8f, 0x06, 0x2b, 0x90, 0xba,
    0x08, 0xb6, 0x10, 0x54, 0x4f, 0x7c, 0x1b, 0x26, 0xed, 0xda, 0x6b, 0xdd,
    0x25, 0xd0, 0x4e, 0xea, 0x42, 0xbb, 0x25, 0x03, 0xa2, 0xfb, 0xcc, 0x61,
    0x67, 0x06, 0x70, 0x1a, 0xc4, 0x78, 0x3a, 0xff, 0x32, 0x62, 0xdd, 0x2c,
    0xab, 0x50, 0x19, 0x3b, 0xf2, 0x9b, 0x7d, 0xb8, 0xfd, 0x4f, 0x29, 0x9c,
    0xa7, 0x91, 0xba, 0x0e, 0x46, 0x5e, 0x51, 0xfe, 0x1d, 0xbf, 0xe5, 0xe5,
    0x9b, 0x95, 0x0d, 0x67, 0xf8, 0xd1, 0xb5, 0x5a, 0xa1, 0x93, 0x2c, 0xc3,
    0xde, 0x0e, 0x97, 0x85, 0x2d, 0x7f, 0xea, 0xab, 0x3e, 0x47, 0x30, 0x18,
    0x24, 0xe8, 0xb7, 0x60, 0xae, 0x47, 0x80, 0xfc, 0xe5, 0x23, 0xe7, 0xc2,
    0xc9, 0x85, 0xe6, 0x98, 0xa0, 0x29, 0x4e, 0xe1, 0x84, 0x39, 0x2d, 0x95,
    0x2c, 0xf3, 0x45, 0x3c, 0xff, 0xaf, 0x27, 0x4c, 0x6b, 0xa6, 0xf5, 0x4b,
    0x11, 0xbd, 0xba, 0x5b, 0x9e, 0xc4, 0xa4, 0x51, 0x1e, 0xbe, 0xd0, 0x90,
    0x3a, 0x9c, 0xc2, 0x26, 0xb6, 0x1e, 0xf1, 0x95, 0x7d, 0xc8, 0x6d, 0x52,
    0xe6, 0x99, 0x2c, 0x5f, 0x9a, 0x96, 0x0c, 0x68, 0x29, 0xfd, 0xe2, 0xfb,
    0xe6, 0xbc, 0xec, 0x31, 0x08, 0xec, 0xe6, 0xb0, 0x53, 0x60, 0xc3, 0x8c,
    0xbe, 0xc1, 0xb3, 0x8a, 0x8f, 0xe4, 0x88, 0x2b, 0x55, 0xe5, 0x64, 0x6e,
    0x9b, 0xd0, 0xaf, 0x7b, 0x64, 0x2a, 0x35, 0x25, 0x10, 0x52, 0xc5, 0x9e,
    0x58, 0x11, 0x39, 0x36, 0x45, 0x51, 0xb8, 0x39, 0x93, 0xfc, 0x9d, 0x6a,
    0xbe, 0x58, 0xcb, 0xa4, 0x0f, 0x51, 0x3c, 0x38, 0x05, 0xca, 0xab, 0x43,
    0x63, 0x0e, 0xf3, 0x8b, 0x41, 0xa6, 0xf8, 0x9b, 0x53, 0x70, 0x80, 0x53,
    0x86, 0x5e, 0x8f, 0xe3, 0xc3, 0x0d, 0x18, 0xc8, 0x4b, 0x34, 0x1f, 0xd8,
    0x1d, 0xbc, 0xf2, 0x6d, 0x34, 0x3a, 0xbe, 0xdf, 0xd9, 0xf6, 0xf3, 0x89,
    0xa1, 0xe1, 0x94, 0x9f, 0x5d, 0x4c, 0x5d, 0xe9, 0xa1, 0x49, 0x92, 0xef,
    0x0e, 0x53, 0x81, 0x89, 0x58, 0x87, 0xa6, 0x37, 0xf1, 0xdd, 0x62, 0x60,
    0x63, 0x5a, 0x9d, 0x1b, 0x8c, 0xc6, 0x7d, 0x52, 0xea, 0x70, 0x09, 0x6a,
    0xe1, 0x32, 0xf3, 0x73, 0x21, 0x1f, 0x07, 0x7b, 0x7c, 0x9b, 0x49, 0xd8,
    0xc0, 0xf3, 0x25, 0x72, 0x6f, 0x9d, 0xed, 0x31, 0x67, 0x36, 0x36, 0x54,
    0x40, 0x92, 0x71, 0xe6, 0x11, 0x28, 0x11, 0xad, 0x93, 0x32, 0x85, 0x7b,
    0x3e, 0xb7, 0x3b, 0x49, 0x13, 0x1c, 0x07, 0xb0, 0x2e, 0x93, 0xaa, 0xfd,
    0xfd, 0x28, 0x47, 0x3d, 0x8d, 0xd2, 0xda, 0xc7, 0x44, 0xd6, 0x7a, 0xdb,
    0x26, 0x7d, 0x1d, 0xb8, 0xe1, 0xde, 0x9d, 0x7a, 0x7d, 0x17, 0x7e, 0x1c,
    0x37, 0x04, 0x8d, 0x2d, 0x7c, 0x5e, 0x18, 0x38, 0x1e, 0xaf, 0xc7, 0x1b,
    0x33, 0x48, 0x31, 0x00, 0x59, 0xf6, 0xf2, 0xca, 0x0f, 0x27, 0x1b, 0x63,
    0x12, 0x7e, 0x02, 0x1d, 0x49, 0xc0, 0x5d, 0x79, 0x87, 0xef, 0x5e, 0x7a,
    0x2f, 0x1f, 0x66, 0x55, 0xd8, 0x09, 0xd9, 0x61, 0x38, 0x68, 0xb0, 0x07,
    0xa3, 0xfc, 0xcc, 0x85, 0x10, 0x7f, 0x4c, 0x65, 0x65, 0xb3, 0xfa, 0xfa,
    0xa5, 0x53, 0x6f, 0xdb, 0x74, 0x4c, 0x56, 0x46, 0x03, 0xe2, 0xd5, 0x7a,
    0x29, 0x1c, 0xc6, 0x02, 0xbc, 0x59, 0xf2, 0x04, 0x75, 0x63, 0xc0, 0x84,
    0x2f, 0x60, 0x1c, 0x67, 0x76, 0xfd, 0x63, 0x86, 0xf3, 0xfa, 0xbf, 0xdc,
    0xd2, 0x2d, 0x90, 0x91, 0xbd, 0x33, 0xa9, 0xe5, 0x66, 0x0c, 0xda, 0x42,
    0x27, 0xca, 0xf4, 0x66, 0xc2, 0xec, 0x92, 0x14, 0x57, 0x06, 0x63, 0xd0,
    0x4d, 0x15, 0x06, 0xeb, 0x69, 0x58, 0x4f, 0x77, 0xc5, 0x8b, 0xc7, 0xf0,
    0x8e, 0xed, 0x64, 0xa0, 0xb3, 0x3c, 0x66, 0x71, 0xc6, 0x2d, 0xda, 0x0a,
    0x0d, 0xfe, 0x70, 0x27, 0x64, 0xf8, 0x27, 0xfa, 0xf6, 0x5f, 0x30, 0xa5,
    0x0d, 0x6c, 0xda, 0xf2, 0x62, 0x5e, 0x78, 0x47, 0xd3, 0x66, 0x00, 0x1c,
    0xfd, 0x56, 0x1f, 0x5d, 0x3f, 0x6f, 0xf4, 0x4c, 0xd8, 0xfd, 0x0e, 0x27,
    0xc9, 0x5c, 0x2b, 0xbc, 0xc0, 0xa4, 0xe7, 0x23, 0x29, 0x02, 0x9f, 0x31,
    0xd6, 0xe9, 0xd7, 0x96, 0xf4, 0xe0, 0x5e, 0x0b, 0x0e, 0x13, 0xee, 0x3c,
    0x09, 0xed, 0xf2, 0x3d, 0x76, 0x91, 0xc3, 0xa4, 0x97, 0xae, 0xd4, 0x87,
    0xd0, 0x5d, 0xf6, 0x18, 0x47, 0x1f, 0x1d, 0x67, 0xf2, 0xcf, 0x63, 0xa0,
    0x91, 0x27, 0xf8, 0x93, 0x45, 0x75, 0x23, 0x3f, 0xd1, 0xf1, 0xad, 0x23,
    0xdd, 0x64, 0x93, 0x96, 0x41, 0x70, 0x7f, 0xf7, 0xf5, 0xa9, 0x89, 0xa2,
    0x34, 0xb0, 0x8d, 0x1b, 0xae, 0x19, 0x15, 0x49, 0x58, 0x23, 0x6d, 0x87,
    0x15, 0x4f, 0x81, 0x76, 0xfb, 0x23, 0xb5, 0xea, 0xcf, 0xac, 0x54, 0x8d,
    0x4e, 0x42, 0x2f, 0xeb, 0x0f, 0x63, 0xdb, 0x68, 0x37, 0xa8, 0xcf, 0x8b,
    0xab, 0xf5, 0xa4, 0x6e, 0x96, 0x2a, 0xb2, 0xd6, 0xbe, 0x9e, 0xbd, 0x0d,
    0xb4, 0x42, 0xa9, 0xcf, 0x01, 0x83, 0x8a, 0x17, 0x47, 0x76, 0xc4, 0xc6,
    0x83, 0x04, 0x95, 0x0b, 0xfc, 0x11, 0xc9, 0x62, 0xb8, 0x0c, 0x76, 0x84,
    0xd9, 0xb9, 0x37, 0xfa, 0xfc, 0x7c, 0xc2, 0x6d, 0x58, 0x3e, 0xb3, 0x04,
    0xbb, 0x8c, 0x8f, 0x48, 0xbc, 0x91, 0x27, 0xcc, 0xf9, 0xb7, 0x22, 0x19,
    0x83, 0x2e, 0x09, 0xb5, 0x72, 0xd9, 0x54, 0x1c, 0x4d, 0xa1, 0xea, 0x0b,
    0xf1, 0xc6, 0x08, 0x72, 0x46, 0x87, 0x7a, 0x6e, 0x80, 0x56, 0x0a, 0x8a,
    0xc0, 0xdd, 0x11, 0x6b, 0xd6, 0xdd, 0x47, 0xdf, 0x10, 0xd9, 0xd8, 0xea,
    0x7c, 0xb0, 0x8f, 0x03, 0x00, 0x2e, 0xc1, 0x8f, 0x44, 0xa8, 0xd3, 0x30,
    0x06, 0x89, 0xa2, 0xf9, 0x34, 0xad, 0xdc, 0x03, 0x85, 0xed, 0x51, 0xa7,
    0x82, 0x9c, 0xe7, 0x5d, 0x52, 0x93, 0x0c, 0x32, 0x9a, 0x5b, 0xe1, 0xaa,
    0xca, 0xb8, 0x02, 0x6d, 0x3a, 0xd4, 0xb1, 0x3a, 0xf0, 0x5f, 0xbe, 0xb5,
    0x0d, 0x10, 0x6b, 0x38, 0x32, 0xac, 0x76, 0x80, 0xbd, 0xca, 0x94, 0x71,
    0x7a, 0xf2, 0xc9, 0x35, 0x2a, 0xde, 0x9f, 0x42, 0x49, 0x18, 0x01, 0xab,
    0xbc, 0xef, 0x7c, 0x64, 0x3f, 0x58, 0x3d, 0x92, 0x59, 0xdb, 0x13, 0xdb,
    0x58, 0x6e, 0x0a, 0xe0, 0xb7, 0x91, 0x4a, 0x08, 0x20, 0xd6, 0x2e, 0x3c,
    0x45, 0xc9, 0x8b, 0x17, 0x79, 0xe7, 0xc7, 0x90, 0x99, 0x3a, 0x18, 0x25,
};

void x25519_ge_scalarmult_base(ge_p3 *h, const uint8_t a[32]) {
  x25519_ge_scalarmult_small_precomp(h, a, k25519SmallPrecomp);
}

#else

// k25519Precomp[i][j] = (j+1)*256^i*B
static const ge_precomp k25519Precomp[32][8] = {
    {
        {
            {{25967493, 19198397, 29566455, 3660896, 54414519, 4014786,
             27544626, 21800161, 61029707, 2047604}},
            {{54563134, 934261, 64385954, 3049989, 66381436, 9406985, 12720692,
             5043384, 19500929, 18085054}},
            {{58370664, 4489569, 9688441, 18769238, 10184608, 21191052,
             29287918, 11864899, 42594502, 29115885}},
        },
        {
            {{54292951, 20578084, 45527620, 11784319, 41753206, 30803714,
             55390960, 29739860, 66750418, 23343128}},
            {{45405608, 6903824, 27185491, 6451973, 37531140, 24000426,
             51492312, 11189267, 40279186, 28235350}},
            {{26966623, 11152617, 32442495, 15396054, 14353839, 20802097,
             63980037, 24013313, 51636816, 29387734}},
        },
        {
            {{15636272, 23865875, 24204772, 25642034, 616976, 16869170,
             27787599, 18782243, 28944399, 32004408}},
            {{16568933, 4717097, 55552716, 32452109, 15682895, 21747389,
             16354576, 21778470, 7689661, 11199574}},
            {{30464137, 27578307, 55329429, 17883566, 23220364, 15915852,
             7512774, 10017326, 49359771, 23634074}},
        },
        {
            {{50071967, 13921891, 10945806, 27521001, 27105051, 17470053,
             38182653, 15006022, 3284568, 27277892}},
            {{23599295, 25248385, 55915199, 25867015, 13236773, 10506355,
             7464579, 9656445, 13059162, 10374397}},
            {{7798537, 16710257, 3033922, 2874086, 28997861, 2835604, 32406664,
             29715387, 66467155, 33453106}},
        },
        {
            {{10861363, 11473154, 27284546, 1981175, 37044515, 12577860,
             32867885, 14515107, 51670560, 10819379}},
            {{4708026, 6336745, 20377586, 9066809, 55836755, 6594695, 41455196,
             12483687, 54440373, 5581305}},
            {{19563141, 16186464, 37722007, 4097518, 10237984, 29206317,
             28542349, 13850243, 43430843, 17738489}},
        },
        {
            {{51736881, 20691677, 32573249, 4720197, 40672342, 5875510,
             47920237, 18329612, 57289923, 21468654}},
            {{58559652, 109982, 15149363, 2178705, 22900618, 4543417, 3044240,
             17864545, 1762327, 14866737}},
            {{48909169, 17603008, 56635573, 1707277, 49922944, 3916100,
             38872452, 3959420, 27914454, 4383652}},
        },
        {
            {{5153727, 9909285, 1723747, 30776558, 30523604, 5516873, 19480852,
             5230134, 43156425, 18378665}},
            {{36839857, 30090922, 7665485, 10083793, 28475525, 1649722,
             20654025, 16520125, 30598449, 7715701}},
            {{28881826, 14381568, 9657904, 3680757, 46927229, 7843315,
             35708204, 1370707, 29794553, 32145132}},
        },
        {
            {{14499471, 30824833, 33917750, 29299779, 28494861, 14271267,
             30290735, 10876454, 33954766, 2381725}},
            {{59913433, 30899068, 52378708, 462250, 39384538, 3941371,
             60872247, 3696004, 34808032, 15351954}},
            {{27431194, 8222322, 16448760, 29646437, 48401861, 11938354,
             34147463, 30583916, 29551812, 10109425}},
        },
    },
    {
        {
            {{53451805, 20399000, 35825113, 11777097, 21447386, 6519384,
             64730580, 31926875, 10092782, 28790261}},
            {{27939166, 14210322, 4677035, 16277044, 44144402, 21156292,
             34600109, 12005537, 49298737, 12803509}},
            {{17228999, 17892808, 65875336, 300139, 65883994, 21839654,
             30364212, 24516238, 18016356, 4397660}},
        },
        {
            {{56150021, 25864224, 4776340, 18600194, 27850027, 17952220,
             40489757, 14544524, 49631360, 982638}},
            {{29253598, 15796703, 64244882, 23645547, 10057022, 3163536, 7332899,
             29434304, 46061167, 9934962}},
            {{5793284, 16271923, 42977250, 23438027, 29188559, 1206517,
             52360934, 4559894, 36984942, 22656481}},
        },
        {
            {{39464912, 22061425, 16282656, 22517939, 28414020, 18542168,
             24191033, 4541697, 53770555, 5500567}},
            {{12650548, 32057319, 9052870, 11355358, 49428827, 25154267,
             49678271, 12264342, 10874051, 13524335}},
            {{25556948, 30508442, 714650, 2510400, 23394682, 23139102, 33119037,
             5080568, 44580805, 5376627}},
        },
        {
            {{41020600, 29543379, 50095164, 30016803, 60382070, 1920896,
             44787559, 24106988, 4535767, 1569007}},
            {{64853442, 14606629, 45416424, 25514613, 28430648, 8775819,
             36614302, 3044289, 31848280, 12543772}},
            {{45080285, 2943892, 35251351, 6777305, 13784462, 29262229,
             39731668, 31491700, 7718481, 14474653}},
        },
        {
            {{2385296, 2454213, 44477544, 46602, 62670929, 17874016, 656964,
             26317767, 24316167, 28300865}},
            {{13741529, 10911568, 33875447, 24950694, 46931033, 32521134,
             33040650, 20129900, 46379407, 8321685}},
            {{21060490, 31341688, 15712756, 29218333, 1639039, 10656336,
             23845965, 21679594, 57124405, 608371}},
        },
        {
            {{53436132, 18466845, 56219170, 25997372, 61071954, 11305546,
             1123968, 26773855, 27229398, 23887}},
            {{43864724, 33260226, 55364135, 14712570, 37643165, 31524814,
             12797023, 27114124, 65475458, 16678953}},
            {{37608244, 4770661, 51054477, 14001337, 7830047, 9564805,
             65600720, 28759386, 49939598, 4904952}},
        },
        {
            {{24059538, 14617003, 19037157, 18514524, 19766092, 18648003,
             5169210, 16191880, 2128236, 29227599}},
            {{50127693, 4124965, 58568254, 22900634, 30336521, 19449185,
             37302527, 916032, 60226322, 30567899}},
            {{44477957, 12419371, 59974635, 26081060, 50629959, 16739174,
             285431, 2763829, 15736322, 4143876}},
        },
        {
            {{2379333, 11839345, 62998462, 27565766, 11274297, 794957, 212801,
             18959769, 23527083, 17096164}},
            {{33431108, 22423954, 49269897, 17927531, 8909498, 8376530,
             34483524, 4087880, 51919953, 19138217}},
            {{1767664, 7197987, 53903638, 31531796, 54017513, 448825, 5799055,
             4357868, 62334673, 17231393}},
        },
    },
    {
        {
            {{6721966, 13833823, 43585476, 32003117, 26354292, 21691111,
             23365146, 29604700, 7390889, 2759800}},
            {{4409022, 2052381, 23373853, 10530217, 7676779, 20668478, 21302352,
             29290375, 1244379, 20634787}},
            {{62687625, 7169618, 4982368, 30596842, 30256824, 30776892, 14086412,
             9208236, 15886429, 16489664}},
        },
        {
            {{1996056, 10375649, 14346367, 13311202, 60234729, 17116020,
             53415665, 398368, 36502409, 32841498}},
            {{41801399, 9795879, 64331450, 14878808, 33577029, 14780362,
             13348553, 12076947, 36272402, 5113181}},
            {{49338080, 11797795, 31950843, 13929123, 41220562, 12288343,
             36767763, 26218045, 13847710, 5387222}},
        },
        {
            {{48526701, 30138214, 17824842, 31213466, 22744342, 23111821,
             8763060, 3617786, 47508202, 10370990}},
            {{20246567, 19185054, 22358228, 33010720, 18507282, 23140436,
             14554436, 24808340, 32232923, 16763880}},
            {{9648486, 10094563, 26416693, 14745928, 36734546, 27081810,
             11094160, 15689506, 3140038, 17044340}},
        },
        {
            {{50948792, 5472694, 31895588, 4744994, 8823515, 10365685,
             39884064, 9448612, 38334410, 366294}},
            {{19153450, 11523972, 56012374, 27051289, 42461232, 5420646,
             28344573, 8041113, 719605, 11671788}},
            {{8678006, 2694440, 60300850, 2517371, 4964326, 11152271, 51675948,
             18287915, 27000812, 23358879}},
        },
        {
            {{51950941, 7134311, 8639287, 30739555, 59873175, 10421741, 564065,
             5336097, 6750977, 19033406}},
            {{11836410, 29574944, 26297893, 16080799, 23455045, 15735944,
             1695823, 24735310, 8169719, 16220347}},
            {{48993007, 8653646, 17578566, 27461813, 59083086, 17541668,
             55964556, 30926767, 61118155, 19388398}},
        },
        {
            {{43800366, 22586119, 15213227, 23473218, 36255258, 22504427,
             27884328, 2847284, 2655861, 1738395}},
            {{39571412, 19301410, 41772562, 25551651, 57738101, 8129820,
             21651608, 30315096, 48021414, 22549153}},
            {{1533110, 3437855, 23735889, 459276, 29970501, 11335377, 26030092,
             5821408, 10478196, 8544890}},
        },
        {
            {{32173102, 17425121, 24896206, 3921497, 22579056, 30143578,
             19270448, 12217473, 17789017, 30158437}},
            {{36555903, 31326030, 51530034, 23407230, 13243888, 517024,
             15479401, 29701199, 30460519, 1052596}},
            {{55493970, 13323617, 32618793, 8175907, 51878691, 12596686,
             27491595, 28942073, 3179267, 24075541}},
        },
        {
            {{31947050, 19187781, 62468280, 18214510, 51982886, 27514722,
             52352086, 17142691, 19072639, 24043372}},
            {{11685058, 11822410, 3158003, 19601838, 33402193, 29389366,
             5977895, 28339415, 473098, 5040608}},
            {{46817982, 8198641, 39698732, 11602122, 1290375, 30754672,
             28326861, 1721092, 47550222, 30422825}},
        },
    },
    {
        {
            {{7881532, 10687937, 7578723, 7738378, 48157852, 31000479, 21820785,
             8076149, 39240368, 11538388}},
            {{47173198, 3899860, 18283497, 26752864, 51380203, 22305220,
             8754524, 7446702, 61432810, 5797015}},
            {{55813245, 29760862, 51326753, 25589858, 12708868, 25098233,
             2014098, 24503858, 64739691, 27677090}},
        },
        {
            {{44636488, 21985690, 39426843, 1146374, 18956691, 16640559,
             1192730, 29840233, 15123618, 10811505}},
            {{14352079, 30134717, 48166819, 10822654, 32750596, 4699007, 67038501,
             15776355, 38222085, 21579878}},
            {{38867681, 25481956, 62129901, 28239114, 29416930, 1847569,
             46454691, 17069576, 4714546, 23953777}},
        },
        {
            {{15200332, 8368572, 19679101, 15970074, 35236190, 1959450,
             24611599, 29010600, 55362987, 12340219}},
            {{12876937, 23074376, 33134380, 6590940, 60801088, 14872439,
             9613953, 8241152, 15370987, 9608631}},
            {{62965568, 21540023, 8446280, 33162829, 4407737, 13629032, 59383996,
             15866073, 38898243, 24740332}},
        },
        {
            {{26660628, 17876777, 8393733, 358047, 59707573, 992987, 43204631,
             858696, 20571223, 8420556}},
            {{14620696, 13067227, 51661590, 8264466, 14106269, 15080814,
             33531827, 12516406, 45534429, 21077682}},
            {{236881, 10476226, 57258, 18877408, 6472997, 2466984, 17258519,
             7256740, 8791136, 15069930}},
        },
        {
            {{1276391, 24182514, 22949634, 17231625, 43615824, 27852245,
             14711874, 4874229, 36445724, 31223040}},
            {{5855666, 4990204, 53397016, 7294283, 59304582, 1924646, 65685689,
             25642053, 34039526, 9234252}},
            {{20590503, 24535444, 31529743, 26201766, 64402029, 10650547,
             31559055, 21944845, 18979185, 13396066}},
        },
        {
            {{24474287, 4968103, 22267082, 4407354, 24063882, 25229252,
             48291976, 13594781, 33514650, 7021958}},
            {{55541958, 26988926, 45743778, 15928891, 40950559, 4315420,
             41160136, 29637754, 45628383, 12868081}},
            {{38473832, 13504660, 19988037, 31421671, 21078224, 6443208,
             45662757, 2244499, 54653067, 25465048}},
        },
        {
            {{36513336, 13793478, 61256044, 319135, 41385692, 27290532,
             33086545, 8957937, 51875216, 5540520}},
            {{55478669, 22050529, 58989363, 25911358, 2620055, 1022908,
             43398120, 31985447, 50980335, 18591624}},
            {{23152952, 775386, 27395463, 14006635, 57407746, 4649511, 1689819,
             892185, 55595587, 18348483}},
        },
        {
            {{9770129, 9586738, 26496094, 4324120, 1556511, 30004408, 27453818,
             4763127, 47929250, 5867133}},
            {{34343820, 1927589, 31726409, 28801137, 23962433, 17534932,
             27846558, 5931263, 37359161, 17445976}},
            {{27461885, 30576896, 22380809, 1815854, 44075111, 30522493,
             7283489, 18406359, 47582163, 7734628}},
        },
    },
    {
        {
            {{59098600, 23963614, 55988460, 6196037, 29344158, 20123547,
             7585294, 30377806, 18549496, 15302069}},
            {{34450527, 27383209, 59436070, 22502750, 6258877, 13504381,
             10458790, 27135971, 58236621, 8424745}},
            {{24687186, 8613276, 36441818, 30320886, 1863891, 31723888,
             19206233, 7134917, 55824382, 32725512}},
        },
        {
            {{11334899, 24336410, 8025292, 12707519, 17523892, 23078361,
             10243737, 18868971, 62042829, 16498836}},
            {{8911542, 6887158, 57524604, 26595841, 11145640, 24010752, 17303924,
             19430194, 6536640, 10543906}},
            {{38162480, 15479762, 49642029, 568875, 65611181, 11223453,
             64439674, 16928857, 39873154, 8876770}},
        },
        {
            {{41365946, 20987567, 51458897, 32707824, 34082177, 32758143,
             33627041, 15824473, 66504438, 24514614}},
            {{10330056, 70051, 7957388, 24551765, 9764901, 15609756, 27698697,
             28664395, 1657393, 3084098}},
            {{10477963, 26084172, 12119565, 20303627, 29016246, 28188843,
             31280318, 14396151, 36875289, 15272408}},
        },
        {
            {{54820555, 3169462, 28813183, 16658753, 25116432, 27923966,
             41934906, 20918293, 42094106, 1950503}},
            {{40928506, 9489186, 11053416, 18808271, 36055143, 5825629,
             58724558, 24786899, 15341278, 8373727}},
            {{28685821, 7759505, 52730348, 21551571, 35137043, 4079241,
             298136, 23321830, 64230656, 15190419}},
        },
        {
            {{34175969, 13806335, 52771379, 17760000, 43104243, 10940927,
             8669718, 2742393, 41075551, 26679428}},
            {{65528476, 21825014, 41129205, 22109408, 49696989, 22641577,
             9291593, 17306653, 54954121, 6048604}},
            {{36803549, 14843443, 1539301, 11864366, 20201677, 1900163,
             13934231, 5128323, 11213262, 9168384}},
        },
        {
            {{40828332, 11007846, 19408960, 32613674, 48515898, 29225851,
             62020803, 22449281, 20470156, 17155731}},
            {{43972811, 9282191, 14855179, 18164354, 59746048, 19145871,
             44324911, 14461607, 14042978, 5230683}},
            {{29969548, 30812838, 50396996, 25001989, 9175485, 31085458,
             21556950, 3506042, 61174973, 21104723}},
        },
        {
            {{63964118, 8744660, 19704003, 4581278, 46678178, 6830682,
             45824694, 8971512, 38569675, 15326562}},
            {{47644235, 10110287, 49846336, 30050539, 43608476, 1355668,
             51585814, 15300987, 46594746, 9168259}},
            {{61755510, 4488612, 43305616, 16314346, 7780487, 17915493,
             38160505, 9601604, 33087103, 24543045}},
        },
        {
            {{47665694, 18041531, 46311396, 21109108, 37284416, 10229460,
             39664535, 18553900, 61111993, 15664671}},
            {{23294591, 16921819, 44458082, 25083453, 27844203, 11461195,
             13099750, 31094076, 18151675, 13417686}},
            {{42385932, 29377914, 35958184, 5988918, 40250079, 6685064,
             1661597, 21002991, 15271675, 18101767}},
        },
    },
    {
        {
            {{11433023, 20325767, 8239630, 28274915, 65123427, 32828713,
             48410099, 2167543, 60187563, 20114249}},
            {{35672693, 15575145, 30436815, 12192228, 44645511, 9395378,
             57191156, 24915434, 12215109, 12028277}},
            {{14098381, 6555944, 23007258, 5757252, 51681032, 20603929,
             30123439, 4617780, 50208775, 32898803}},
        },
        {
            {{63082644, 18313596, 11893167, 13718664, 52299402, 1847384,
             51288865, 10154008, 23973261, 20869958}},
            {{40577025, 29858441, 65199965, 2534300, 35238307, 17004076,
             18341389, 22134481, 32013173, 23450893}},
            {{41629544, 10876442, 55337778, 18929291, 54739296, 1838103,
             21911214, 6354752, 4425632, 32716610}},
        },
        {
            {{56675475, 18941465, 22229857, 30463385, 53917697, 776728,
             49693489, 21533969, 4725004, 14044970}},
            {{19268631, 26250011, 1555348, 8692754, 45634805, 23643767, 6347389,
             32142648, 47586572, 17444675}},
            {{42244775, 12986007, 56209986, 27995847, 55796492, 33405905,
             19541417, 8180106, 9282262, 10282508}},
        },
        {
            {{40903763, 4428546, 58447668, 20360168, 4098401, 19389175,
             15522534, 8372215, 5542595, 22851749}},
            {{56546323, 14895632, 26814552, 16880582, 49628109, 31065071,
             64326972, 6993760, 49014979, 10114654}},
            {{47001790, 32625013, 31422703, 10427861, 59998115, 6150668,
             38017109, 22025285, 25953724, 33448274}},
        },
        {
            {{62874467, 25515139, 57989738, 3045999, 2101609, 20947138,
             19390019, 6094296, 63793585, 12831124}},
            {{51110167, 7578151, 5310217, 14408357, 33560244, 33329692,
             31575953, 6326196, 7381791, 31132593}},
            {{46206085, 3296810, 24736065, 17226043, 18374253, 7318640,
             6295303, 8082724, 51746375, 12339663}},
        },
        {
            {{27724736, 2291157, 6088201, 19369634, 1792726, 5857634, 13848414,
             15768922, 25091167, 14856294}},
            {{48242193, 8331042, 24373479, 8541013, 66406866, 24284974, 12927299,
             20858939, 44926390, 24541532}},
            {{55685435, 28132841, 11632844, 3405020, 30536730, 21880393,
             39848098, 13866389, 30146206, 9142070}},
        },
        {
            {{3924129, 18246916, 53291741, 23499471, 12291819, 32886066,
             39406089, 9326383, 58871006, 4171293}},
            {{51186905, 16037936, 6713787, 16606682, 45496729, 2790943,
             26396185, 3731949, 345228, 28091483}},
            {{45781307, 13448258, 25284571, 1143661, 20614966, 24705045,
             2031538, 21163201, 50855680, 19972348}},
        },
        {
            {{31016192, 16832003, 26371391, 19103199, 62081514, 14854136,
             17477601, 3842657, 28012650, 17149012}},
            {{62033029, 9368965, 58546785, 28953529, 51858910, 6970559,
             57918991, 16292056, 58241707, 3507939}},
            {{29439664, 3537914, 23333589, 6997794, 49553303, 22536363,
             51899661, 18503164, 57943934, 6580395}},
        },
    },
    {
        {
            {{54923003, 25874643, 16438268, 10826160, 58412047, 27318820,
             17860443, 24280586, 65013061, 9304566}},
            {{20714545, 29217521, 29088194, 7406487, 11426967, 28458727,
             14792666, 18945815, 5289420, 33077305}},
            {{50443312, 22903641, 60948518, 20248671, 9192019, 31751970,
             17271489, 12349094, 26939669, 29802138}},
        },
        {
            {{54218966, 9373457, 31595848, 16374215, 21471720, 13221525,
             39825369, 21205872, 63410057, 117886}},
            {{22263325, 26994382, 3984569, 22379786, 51994855, 32987646,
             28311252, 5358056, 43789084, 541963}},
            {{16259200, 3261970, 2309254, 18019958, 50223152, 28972515,
             24134069, 16848603, 53771797, 20002236}},
        },
        {
            {{9378160, 20414246, 44262881, 20809167, 28198280, 26310334,
             64709179, 32837080, 690425, 14876244}},
            {{24977353, 33240048, 58884894, 20089345, 28432342, 32378079,
             54040059, 21257083, 44727879, 6618998}},
            {{65570671, 11685645, 12944378, 13682314, 42719353, 19141238,
             8044828, 19737104, 32239828, 27901670}},
        },
        {
            {{48505798, 4762989, 66182614, 8885303, 38696384, 30367116, 9781646,
             23204373, 32779358, 5095274}},
            {{34100715, 28339925, 34843976, 29869215, 9460460, 24227009,
             42507207, 14506723, 21639561, 30924196}},
            {{50707921, 20442216, 25239337, 15531969, 3987758, 29055114,
             65819361, 26690896, 17874573, 558605}},
        },
        {
            {{53508735, 10240080, 9171883, 16131053, 46239610, 9599699,
             33499487, 5080151, 2085892, 5119761}},
            {{44903700, 31034903, 50727262, 414690, 42089314, 2170429,
             30634760, 25190818, 35108870, 27794547}},
            {{60263160, 15791201, 8550074, 32241778, 29928808, 21462176,
             27534429, 26362287, 44757485, 12961481}},
        },
        {
            {{42616785, 23983660, 10368193, 11582341, 43711571, 31309144,
             16533929, 8206996, 36914212, 28394793}},
            {{55987368, 30172197, 2307365, 6362031, 66973409, 8868176, 50273234,
             7031274, 7589640, 8945490}},
            {{34956097, 8917966, 6661220, 21876816, 65916803, 17761038,
             7251488, 22372252, 24099108, 19098262}},
        },
        {
            {{5019539, 25646962, 4244126, 18840076, 40175591, 6453164,
             47990682, 20265406, 60876967, 23273695}},
            {{10853575, 10721687, 26480089, 5861829, 44113045, 1972174,
             65242217, 22996533, 63745412, 27113307}},
            {{50106456, 5906789, 221599, 26991285, 7828207, 20305514, 24362660,
             31546264, 53242455, 7421391}},
        },
        {
            {{8139908, 27007935, 32257645, 27663886, 30375718, 1886181,
             45933756, 15441251, 28826358, 29431403}},
            {{6267067, 9695052, 7709135, 16950835, 34239795, 31668296,
             14795159, 25714308, 13746020, 31812384}},
            {{28584883, 7787108, 60375922, 18503702, 22846040, 25983196,
             63926927, 33190907, 4771361, 25134474}},
        },
    },
    {
        {
            {{24949256, 6376279, 39642383, 25379823, 48462709, 23623825,
             33543568, 21412737, 3569626, 11342593}},
            {{26514970, 4740088, 27912651, 3697550, 19331575, 22082093, 6809885,
             4608608, 7325975, 18753361}},
            {{55490446, 19000001, 42787651, 7655127, 65739590, 5214311,
             39708324, 10258389, 49462170, 25367739}},
        },
        {
            {{11431185, 15823007, 26570245, 14329124, 18029990, 4796082,
             35662685, 15580663, 9280358, 29580745}},
            {{66948081, 23228174, 44253547, 29249434, 46247496, 19933429,
             34297962, 22372809, 51563772, 4387440}},
            {{46309467, 12194511, 3937617, 27748540, 39954043, 9340369,
             42594872, 8548136, 20617071, 26072431}},
        },
        {
            {{66170039, 29623845, 58394552, 16124717, 24603125, 27329039,
             53333511, 21678609, 24345682, 10325460}},
            {{47253587, 31985546, 44906155, 8714033, 14007766, 6928528,
             16318175, 32543743, 4766742, 3552007}},
            {{45357481, 16823515, 1351762, 32751011, 63099193, 3950934, 3217514,
             14481909, 10988822, 29559670}},
        },
        {
            {{15564307, 19242862, 3101242, 5684148, 30446780, 25503076,
             12677126, 27049089, 58813011, 13296004}},
            {{57666574, 6624295, 36809900, 21640754, 62437882, 31497052,
             31521203, 9614054, 37108040, 12074673}},
            {{4771172, 33419193, 14290748, 20464580, 27992297, 14998318,
             65694928, 31997715, 29832612, 17163397}},
        },
        {
            {{7064884, 26013258, 47946901, 28486894, 48217594, 30641695,
             25825241, 5293297, 39986204, 13101589}},
            {{64810282, 2439669, 59642254, 1719964, 39841323, 17225986,
             32512468, 28236839, 36752793, 29363474}},
            {{37102324, 10162315, 33928688, 3981722, 50626726, 20484387,
             14413973, 9515896, 19568978, 9628812}},
        },
        {
            {{33053803, 199357, 15894591, 1583059, 27380243, 28973997, 49269969,
             27447592, 60817077, 3437739}},
            {{48129987, 3884492, 19469877, 12726490, 15913552, 13614290,
             44147131, 70103, 7463304, 4176122}},
            {{39984863, 10659916, 11482427, 17484051, 12771466, 26919315,
             34389459, 28231680, 24216881, 5944158}},
        },
        {
            {{8894125, 7450974, 64444715, 23788679, 39028346, 21165316,
             19345745, 14680796, 11632993, 5847885}},
            {{26942781, 31239115, 9129563, 28647825, 26024104, 11769399,
             55590027, 6367193, 57381634, 4782139}},
            {{19916442, 28726022, 44198159, 22140040, 25606323, 27581991,
             33253852, 8220911, 6358847, 31680575}},
        },
        {
            {{801428, 31472730, 16569427, 11065167, 29875704, 96627, 7908388,
             29073952, 53570360, 1387154}},
            {{19646058, 5720633, 55692158, 12814208, 11607948, 12749789,
             14147075, 15156355, 45242033, 11835259}},
            {{19299512, 1155910, 28703737, 14890794, 2925026, 7269399, 26121523,
             15467869, 40548314, 5052482}},
        },
    },
    {
        {
            {{64091413, 10058205, 1980837, 3964243, 22160966, 12322533, 60677741,
             20936246, 12228556, 26550755}},
            {{32944382, 14922211, 44263970, 5188527, 21913450, 24834489,
             4001464, 13238564, 60994061, 8653814}},
            {{22865569, 28901697, 27603667, 21009037, 14348957, 8234005,
             24808405, 5719875, 28483275, 2841751}},
        },
        {
            {{50687877, 32441126, 66781144, 21446575, 21886281, 18001658,
             65220897, 33238773, 19932057, 20815229}},
            {{55452759, 10087520, 58243976, 28018288, 47830290, 30498519,
             3999227, 13239134, 62331395, 19644223}},
            {{1382174, 21859713, 17266789, 9194690, 53784508, 9720080,
             20403944, 11284705, 53095046, 3093229}},
        },
        {
            {{16650902, 22516500, 66044685, 1570628, 58779118, 7352752, 66806440,
             16271224, 43059443, 26862581}},
            {{45197768, 27626490, 62497547, 27994275, 35364760, 22769138,
             24123613, 15193618, 45456747, 16815042}},
            {{57172930, 29264984, 41829040, 4372841, 2087473, 10399484,
             31870908, 14690798, 17361620, 11864968}},
        },
        {
            {{55801235, 6210371, 13206574, 5806320, 38091172, 19587231,
             54777658, 26067830, 41530403, 17313742}},
            {{14668443, 21284197, 26039038, 15305210, 25515617, 4542480,
             10453892, 6577524, 9145645, 27110552}},
            {{5974855, 3053895, 57675815, 23169240, 35243739, 3225008,
             59136222, 3936127, 61456591, 30504127}},
        },
        {
            {{30625386, 28825032, 41552902, 20761565, 46624288, 7695098,
             17097188, 17250936, 39109084, 1803631}},
            {{63555773, 9865098, 61880298, 4272700, 61435032, 16864731,
             14911343, 12196514, 45703375, 7047411}},
            {{20093258, 9920966, 55970670, 28210574, 13161586, 12044805,
             34252013, 4124600, 34765036, 23296865}},
        },
        {
            {{46320040, 14084653, 53577151, 7842146, 19119038, 19731827,
             4752376, 24839792, 45429205, 2288037}},
            {{40289628, 30270716, 29965058, 3039786, 52635099, 2540456,
             29457502, 14625692, 42289247, 12570231}},
            {{66045306, 22002608, 16920317, 12494842, 1278292, 27685323,
             45948920, 30055751, 55134159, 4724942}},
        },
        {
            {{17960970, 21778898, 62967895, 23851901, 58232301, 32143814,
             54201480, 24894499, 37532563, 1903855}},
            {{23134274, 19275300, 56426866, 31942495, 20684484, 15770816,
             54119114, 3190295, 26955097, 14109738}},
            {{15308788, 5320727, 36995055, 19235554, 22902007, 7767164,
             29425325, 22276870, 31960941, 11934971}},
        },
        {
            {{39713153, 8435795, 4109644, 12222639, 42480996, 14818668,
             20638173, 4875028, 10491392, 1379718}},
            {{53949449, 9197840, 3875503, 24618324, 65725151, 27674630,
             33518458, 16176658, 21432314, 12180697}},
            {{55321537, 11500837, 13787581, 19721842, 44678184, 10140204,
             1465425, 12689540, 56807545, 19681548}},
        },
    },
    {
        {
            {{5414091, 18168391, 46101199, 9643569, 12834970, 1186149,
             64485948, 32212200, 26128230, 6032912}},
            {{40771450, 19788269, 32496024, 19900513, 17847800, 20885276,
             3604024, 8316894, 41233830, 23117073}},
            {{3296484, 6223048, 24680646, 21307972, 44056843, 5903204,
             58246567, 28915267, 12376616, 3188849}},
        },
        {
            {{29190469, 18895386, 27549112, 32370916, 3520065, 22857131,
             32049514, 26245319, 50999629, 23702124}},
            {{52364359, 24245275, 735817, 32955454, 46701176, 28496527,
             25246077, 17758763, 18640740, 32593455}},
            {{60180029, 17123636, 10361373, 5642961, 4910474, 12345252,
             35470478, 33060001, 10530746, 1053335}},
        },
        {
            {{37842897, 19367626, 53570647, 21437058, 47651804, 22899047,
             35646494, 30605446, 24018830, 15026644}},
            {{44516310, 30409154, 64819587, 5953842, 53668675, 9425630,
             25310643, 13003497, 64794073, 18408815}},
            {{39688860, 32951110, 59064879, 31885314, 41016598, 13987818,
             39811242, 187898, 43942445, 31022696}},
        },
        {
            {{45364466, 19743956, 1844839, 5021428, 56674465, 17642958,
             9716666, 16266922, 62038647, 726098}},
            {{29370903, 27500434, 7334070, 18212173, 9385286, 2247707,
             53446902, 28714970, 30007387, 17731091}},
            {{66172485, 16086690, 23751945, 33011114, 65941325, 28365395, 9137108,
             730663, 9835848, 4555336}},
        },
        {
            {{43732429, 1410445, 44855111, 20654817, 30867634, 15826977,
             17693930, 544696, 55123566, 12422645}},
            {{31117226, 21338698, 53606025, 6561946, 57231997, 20796761,
             61990178, 29457725, 29120152, 13924425}},
            {{49707966, 19321222, 19675798, 30819676, 56101901, 27695611,
             57724924, 22236731, 7240930, 33317044}},
        },
        {
            {{35747106, 22207651, 52101416, 27698213, 44655523, 21401660,
             1222335, 4389483, 3293637, 18002689}},
            {{50424044, 19110186, 11038543, 11054958, 53307689, 30215898,
             42789283, 7733546, 12796905, 27218610}},
            {{58349431, 22736595, 41689999, 10783768, 36493307, 23807620,
             38855524, 3647835, 3222231, 22393970}},
        },
        {
            {{18606113, 1693100, 41660478, 18384159, 4112352, 10045021,
             23603893, 31506198, 59558087, 2484984}},
            {{9255298, 30423235, 54952701, 32550175, 13098012, 24339566,
             16377219, 31451620, 47306788, 30519729}},
            {{44379556, 7496159, 61366665, 11329248, 19991973, 30206930,
             35390715, 9936965, 37011176, 22935634}},
        },
        {
            {{21878571, 28553135, 4338335, 13643897, 64071999, 13160959,
             19708896, 5415497, 59748361, 29445138}},
            {{27736842, 10103576, 12500508, 8502413, 63695848, 23920873,
             10436917, 32004156, 43449720, 25422331}},
            {{19492550, 21450067, 37426887, 32701801, 63900692, 12403436,
             30066266, 8367329, 13243957, 8709688}},
        },
    },
    {
        {
            {{12015105, 2801261, 28198131, 10151021, 24818120, 28811299,
             55914672, 27908697, 5150967, 7274186}},
            {{2831347, 21062286, 1478974, 6122054, 23825128, 20820846,
             31097298, 6083058, 31021603, 23760822}},
            {{64578913, 31324785, 445612, 10720828, 53259337, 22048494,
             43601132, 16354464, 15067285, 19406725}},
        },
        {
            {{7840923, 14037873, 33744001, 15934015, 66380651, 29911725,
             21403987, 1057586, 47729402, 21151211}},
            {{915865, 17085158, 15608284, 24765302, 42751837, 6060029,
             49737545, 8410996, 59888403, 16527024}},
            {{32922597, 32997445, 20336073, 17369864, 10903704, 28169945,
             16957573, 52992, 23834301, 6588044}},
        },
        {
            {{32752011, 11232950, 3381995, 24839566, 22652987, 22810329,
             17159698, 16689107, 46794284, 32248439}},
            {{62419196, 9166775, 41398568, 22707125, 11576751, 12733943,
             7924251, 30802151, 1976122, 26305405}},
            {{21251203, 16309901, 64125849, 26771309, 30810596, 12967303, 156041,
             30183180, 12331344, 25317235}},
        },
        {
            {{8651595, 29077400, 51023227, 28557437, 13002506, 2950805,
             29054427, 28447462, 10008135, 28886531}},
            {{31486061, 15114593, 52847614, 12951353, 14369431, 26166587,
             16347320, 19892343, 8684154, 23021480}},
            {{19443825, 11385320, 24468943, 23895364, 43189605, 2187568,
             40845657, 27467510, 31316347, 14219878}},
        },
        {
            {{38514374, 1193784, 32245219, 11392485, 31092169, 15722801,
             27146014, 6992409, 29126555, 9207390}},
            {{32382916, 1110093, 18477781, 11028262, 39697101, 26006320,
             62128346, 10843781, 59151264, 19118701}},
            {{2814918, 7836403, 27519878, 25686276, 46214848, 22000742,
             45614304, 8550129, 28346258, 1994730}},
        },
        {
            {{47530565, 8085544, 53108345, 29605809, 2785837, 17323125,
             47591912, 7174893, 22628102, 8115180}},
            {{36703732, 955510, 55975026, 18476362, 34661776, 20276352,
             41457285, 3317159, 57165847, 930271}},
            {{51805164, 26720662, 28856489, 1357446, 23421993, 1057177,
             24091212, 32165462, 44343487, 22903716}},
        },
        {
            {{44357633, 28250434, 54201256, 20785565, 51297352, 25757378,
             52269845, 17000211, 65241845, 8398969}},
            {{35139535, 2106402, 62372504, 1362500, 12813763, 16200670,
             22981545, 27263159, 18009407, 17781660}},
            {{49887941, 24009210, 39324209, 14166834, 29815394, 7444469,
             29551787, 29827013, 19288548, 1325865}},
        },
        {
            {{15100138, 17718680, 43184885, 32549333, 40658671, 15509407,
             12376730, 30075286, 33166106, 25511682}},
            {{20909212, 13023121, 57899112, 16251777, 61330449, 25459517,
             12412150, 10018715, 2213263, 19676059}},
            {{32529814, 22479743, 30361438, 16864679, 57972923, 1513225,
             22922121, 6382134, 61341936, 8371347}},
        },
    },
    {
        {
            {{9923462, 11271500, 12616794, 3544722, 37110496, 31832805,
             12891686, 25361300, 40665920, 10486143}},
            {{44511638, 26541766, 8587002, 25296571, 4084308, 20584370, 361725,
             2610596, 43187334, 22099236}},
            {{5408392, 32417741, 62139741, 10561667, 24145918, 14240566,
             31319731, 29318891, 19985174, 30118346}},
        },
        {
            {{53114407, 16616820, 14549246, 3341099, 32155958, 13648976,
             49531796, 8849296, 65030, 8370684}},
            {{58787919, 21504805, 31204562, 5839400, 46481576, 32497154,
             47665921, 6922163, 12743482, 23753914}},
            {{64747493, 12678784, 28815050, 4759974, 43215817, 4884716,
             23783145, 11038569, 18800704, 255233}},
        },
        {
            {{61839187, 31780545, 13957885, 7990715, 23132995, 728773, 13393847,
             9066957, 19258688, 18800639}},
            {{64172210, 22726896, 56676774, 14516792, 63468078, 4372540,
             35173943, 2209389, 65584811, 2055793}},
            {{580882, 16705327, 5468415, 30871414, 36182444, 18858431,
             59905517, 24560042, 37087844, 7394434}},
        },
        {
            {{23838809, 1822728, 51370421, 15242726, 8318092, 29821328,
             45436683, 30062226, 62287122, 14799920}},
            {{13345610, 9759151, 3371034, 17416641, 16353038, 8577942, 31129804,
             13496856, 58052846, 7402517}},
            {{2286874, 29118501, 47066405, 31546095, 53412636, 5038121,
             11006906, 17794080, 8205060, 1607563}},
        },
        {
            {{14414067, 25552300, 3331829, 30346215, 22249150, 27960244,
             18364660, 30647474, 30019586, 24525154}},
            {{39420813, 1585952, 56333811, 931068, 37988643, 22552112,
             52698034, 12029092, 9944378, 8024}},
            {{4368715, 29844802, 29874199, 18531449, 46878477, 22143727,
             50994269, 32555346, 58966475, 5640029}},
        },
        {
            {{10299591, 13746483, 11661824, 16234854, 7630238, 5998374, 9809887,
             16859868, 15219797, 19226649}},
            {{27425505, 27835351, 3055005, 10660664, 23458024, 595578, 51710259,
             32381236, 48766680, 9742716}},
            {{6744077, 2427284, 26042789, 2720740, 66260958, 1118973, 32324614,
             7406442, 12420155, 1994844}},
        },
        {
            {{14012502, 28529712, 48724410, 23975962, 40623521, 29617992,
             54075385, 22644628, 24319928, 27108099}},
            {{16412671, 29047065, 10772640, 15929391, 50040076, 28895810,
             10555944, 23070383, 37006495, 28815383}},
            {{22397363, 25786748, 57815702, 20761563, 17166286, 23799296,
             39775798, 6199365, 21880021, 21303672}},
        },
        {
            {{62825557, 5368522, 35991846, 8163388, 36785801, 3209127,
             16557151, 8890729, 8840445, 4957760}},
            {{51661137, 709326, 60189418, 22684253, 37330941, 6522331,
             45388683, 12130071, 52312361, 5005756}},
            {{64994094, 19246303, 23019041, 15765735, 41839181, 6002751,
             10183197, 20315106, 50713577, 31378319}},
        },
    },
    {
        {
            {{48083108, 1632004, 13466291, 25559332, 43468412, 16573536,
             35094956, 30497327, 22208661, 2000468}},
            {{3065054, 32141671, 41510189, 33192999, 49425798, 27851016,
             58944651, 11248526, 63417650, 26140247}},
            {{10379208, 27508878, 8877318, 1473647, 37817580, 21046851,
             16690914, 2553332, 63976176, 16400288}},
        },
        {
            {{15716668, 1254266, 48636174, 7446273, 58659946, 6344163,
             45011593, 26268851, 26894936, 9132066}},
            {{24158868, 12938817, 11085297, 25376834, 39045385, 29097348,
             36532400, 64451, 60291780, 30861549}},
            {{13488534, 7794716, 22236231, 5989356, 25426474, 20976224, 2350709,
             30135921, 62420857, 2364225}},
        },
        {
            {{16335033, 9132434, 25640582, 6678888, 1725628, 8517937, 55301840,
             21856974, 15445874, 25756331}},
            {{29004188, 25687351, 28661401, 32914020, 54314860, 25611345,
             31863254, 29418892, 66830813, 17795152}},
            {{60986784, 18687766, 38493958, 14569918, 56250865, 29962602,
             10343411, 26578142, 37280576, 22738620}},
        },
        {
            {{27081650, 3463984, 14099042, 29036828, 1616302, 27348828, 29542635,
             15372179, 17293797, 960709}},
            {{20263915, 11434237, 61343429, 11236809, 13505955, 22697330,
             50997518, 6493121, 47724353, 7639713}},
            {{64278047, 18715199, 25403037, 25339236, 58791851, 17380732,
             18006286, 17510682, 29994676, 17746311}},
        },
        {
            {{9769828, 5202651, 42951466, 19923039, 39057860, 21992807,
             42495722, 19693649, 35924288, 709463}},
            {{12286395, 13076066, 45333675, 32377809, 42105665, 4057651,
             35090736, 24663557, 16102006, 13205847}},
            {{13733362, 5599946, 10557076, 3195751, 61550873, 8536969, 41568694,
             8525971, 10151379, 10394400}},
        },
        {
            {{4024660, 17416881, 22436261, 12276534, 58009849, 30868332,
             19698228, 11743039, 33806530, 8934413}},
            {{51229064, 29029191, 58528116, 30620370, 14634844, 32856154,
             57659786, 3137093, 55571978, 11721157}},
            {{17555920, 28540494, 8268605, 2331751, 44370049, 9761012, 9319229,
             8835153, 57903375, 32274386}},
        },
        {
            {{66647436, 25724417, 20614117, 16688288, 59594098, 28747312,
             22300303, 505429, 6108462, 27371017}},
            {{62038564, 12367916, 36445330, 3234472, 32617080, 25131790,
             29880582, 20071101, 40210373, 25686972}},
            {{35133562, 5726538, 26934134, 10237677, 63935147, 32949378,
             24199303, 3795095, 7592688, 18562353}},
        },
        {
            {{21594432, 18590204, 17466407, 29477210, 32537083, 2739898,
             6407723, 12018833, 38852812, 4298411}},
            {{46458361, 21592935, 39872588, 570497, 3767144, 31836892,
             13891941, 31985238, 13717173, 10805743}},
            {{52432215, 17910135, 15287173, 11927123, 24177847, 25378864,
             66312432, 14860608, 40169934, 27690595}},
        },
    },
    {
        {
            {{12962541, 5311799, 57048096, 11658279, 18855286, 25600231,
             13286262, 20745728, 62727807, 9882021}},
            {{18512060, 11319350, 46985740, 15090308, 18818594, 5271736,
             44380960, 3666878, 43141434, 30255002}},
            {{60319844, 30408388, 16192428, 13241070, 15898607, 19348318,
             57023983, 26893321, 64705764, 5276064}},
        },
        {
            {{30169808, 28236784, 26306205, 21803573, 27814963, 7069267,
             7152851, 3684982, 1449224, 13082861}},
            {{10342807, 3098505, 2119311, 193222, 25702612, 12233820, 23697382,
             15056736, 46092426, 25352431}},
            {{33958735, 3261607, 22745853, 7948688, 19370557, 18376767,
             40936887, 6482813, 56808784, 22494330}},
        },
        {
            {{32869458, 28145887, 25609742, 15678670, 56421095, 18083360,
             26112420, 2521008, 44444576, 6904814}},
            {{29506904, 4457497, 3377935, 23757988, 36598817, 12935079, 1561737,
             3841096, 38105225, 26896789}},
            {{10340844, 26924055, 48452231, 31276001, 12621150, 20215377,
             30878496, 21730062, 41524312, 5181965}},
        },
        {
            {{25940096, 20896407, 17324187, 23247058, 58437395, 15029093,
             24396252, 17103510, 64786011, 21165857}},
            {{45343161, 9916822, 65808455, 4079497, 66080518, 11909558, 1782390,
             12641087, 20603771, 26992690}},
            {{48226577, 21881051, 24849421, 11501709, 13161720, 28785558,
             1925522, 11914390, 4662781, 7820689}},
        },
        {
            {{12241050, 33128450, 8132690, 9393934, 32846760, 31954812, 29749455,
             12172924, 16136752, 15264020}},
            {{56758909, 18873868, 58896884, 2330219, 49446315, 19008651,
             10658212, 6671822, 19012087, 3772772}},
            {{3753511, 30133366, 10617073, 2028709, 14841030, 26832768, 28718731,
             17791548, 20527770, 12988982}},
        },
        {
            {{52286360, 27757162, 63400876, 12689772, 66209881, 22639565,
             42925817, 22989488, 3299664, 21129479}},
            {{50331161, 18301130, 57466446, 4978982, 3308785, 8755439, 6943197,
             6461331, 41525717, 8991217}},
            {{49882601, 1816361, 65435576, 27467992, 31783887, 25378441,
             34160718, 7417949, 36866577, 1507264}},
        },
        {
            {{29692644, 6829891, 56610064, 4334895, 20945975, 21647936,
             38221255, 8209390, 14606362, 22907359}},
            {{63627275, 8707080, 32188102, 5672294, 22096700, 1711240, 34088169,
             9761486, 4170404, 31469107}},
            {{55521375, 14855944, 62981086, 32022574, 40459774, 15084045,
             22186522, 16002000, 52832027, 25153633}},
        },
        {
            {{62297408, 13761028, 35404987, 31070512, 63796392, 7869046,
             59995292, 23934339, 13240844, 10965870}},
            {{59366301, 25297669, 52340529, 19898171, 43876480, 12387165,
             4498947, 14147411, 29514390, 4302863}},
            {{53695440, 21146572, 20757301, 19752600, 14785142, 8976368,
             62047588, 31410058, 17846987, 19582505}},
        },
    },
    {
        {
            {{64864412, 32799703, 62511833, 32488122, 60861691, 1455298,
             45461136, 24339642, 61886162, 12650266}},
            {{57202067, 17484121, 21134159, 12198166, 40044289, 708125, 387813,
             13770293, 47974538, 10958662}},
            {{22470984, 12369526, 23446014, 28113323, 45588061, 23855708,
             55336367, 21979976, 42025033, 4271861}},
        },
        {
            {{41939299, 23500789, 47199531, 15361594, 61124506, 2159191,
             75375, 29275903, 34582642, 8469672}},
            {{15854951, 4148314, 58214974, 7259001, 11666551, 13824734,
             36577666, 2697371, 24154791, 24093489}},
            {{15446137, 17747788, 29759746, 14019369, 30811221, 23944241,
             35526855, 12840103, 24913809, 9815020}},
        },
        {
            {{62399578, 27940162, 35267365, 21265538, 52665326, 10799413,
             58005188, 13438768, 18735128, 9466238}},
            {{11933045, 9281483, 5081055, 28370608, 64480701, 28648802, 59381042,
             22658328, 44380208, 16199063}},
            {{14576810, 379472, 40322331, 25237195, 37682355, 22741457,
             67006097, 1876698, 30801119, 2164795}},
        },
        {
            {{15995086, 3199873, 13672555, 13712240, 47730029, 28906785,
             54027253, 18058162, 53616056, 1268051}},
            {{56818250, 29895392, 63822271, 10948817, 23037027, 3794475,
             63638526, 20954210, 50053494, 3565903}},
            {{29210069, 24135095, 61189071, 28601646, 10834810, 20226706,
             50596761, 22733718, 39946641, 19523900}},
        },
        {
            {{53946955, 15508587, 16663704, 25398282, 38758921, 9019122,
             37925443, 29785008, 2244110, 19552453}},
            {{61955989, 29753495, 57802388, 27482848, 16243068, 14684434,
             41435776, 17373631, 13491505, 4641841}},
            {{10813398, 643330, 47920349, 32825515, 30292061, 16954354,
             27548446, 25833190, 14476988, 20787001}},
        },
        {
            {{10292079, 9984945, 6481436, 8279905, 59857350, 7032742, 27282937,
             31910173, 39196053, 12651323}},
            {{35923332, 32741048, 22271203, 11835308, 10201545, 15351028,
             17099662, 3988035, 21721536, 30405492}},
            {{10202177, 27008593, 35735631, 23979793, 34958221, 25434748,
             54202543, 3852693, 13216206, 14842320}},
        },
        {
            {{51293224, 22953365, 60569911, 26295436, 60124204, 26972653,
             35608016, 13765823, 39674467, 9900183}},
            {{14465486, 19721101, 34974879, 18815558, 39665676, 12990491,
             33046193, 15796406, 60056998, 25514317}},
            {{30924398, 25274812, 6359015, 20738097, 16508376, 9071735,
             41620263, 15413634, 9524356, 26535554}},
        },
        {
            {{12274201, 20378885, 32627640, 31769106, 6736624, 13267305,
             5237659, 28444949, 15663515, 4035784}},
            {{64157555, 8903984, 17349946, 601635, 50676049, 28941875,
             53376124, 17665097, 44850385, 4659090}},
            {{50192582, 28601458, 36715152, 18395610, 20774811, 15897498,
             5736189, 15026997, 64930608, 20098846}},
        },
    },
    {
        {
            {{58249865, 31335375, 28571665, 23398914, 66634396, 23448733,
             63307367, 278094, 23440562, 33264224}},
            {{10226222, 27625730, 15139955, 120818, 52241171, 5218602, 32937275,
             11551483, 50536904, 26111567}},
            {{17932739, 21117156, 43069306, 10749059, 11316803, 7535897,
             22503767, 5561594, 63462240, 3898660}},
        },
        {
            {{7749907, 32584865, 50769132, 33537967, 42090752, 15122142, 65535333,
             7152529, 21831162, 1245233}},
            {{26958440, 18896406, 4314585, 8346991, 61431100, 11960071,
             34519569, 32934396, 36706772, 16838219}},
            {{54942968, 9166946, 33491384, 13673479, 29787085, 13096535,
             6280834, 14587357, 44770839, 13987524}},
        },
        {
            {{42758936, 7778774, 21116000, 15572597, 62275598, 28196653,
             62807965, 28429792, 59639082, 30696363}},
            {{9681908, 26817309, 35157219, 13591837, 60225043, 386949, 31622781,
             6439245, 52527852, 4091396}},
            {{58682418, 1470726, 38999185, 31957441, 3978626, 28430809,
             47486180, 12092162, 29077877, 18812444}},
        },
        {
            {{5269168, 26694706, 53878652, 25533716, 25932562, 1763552,
             61502754, 28048550, 47091016, 2357888}},
            {{32264008, 18146780, 61721128, 32394338, 65017541, 29607531,
             23104803, 20684524, 5727337, 189038}},
            {{14609104, 24599962, 61108297, 16931650, 52531476, 25810533,
             40363694, 10942114, 41219933, 18669734}},
        },
        {
            {{20513481, 5557931, 51504251, 7829530, 26413943, 31535028,
             45729895, 7471780, 13913677, 28416557}},
            {{41534488, 11967825, 29233242, 12948236, 60354399, 4713226,
             58167894, 14059179, 12878652, 8511905}},
            {{41452044, 3393630, 64153449, 26478905, 64858154, 9366907,
             36885446, 6812973, 5568676, 30426776}},
        },
        {
            {{11630004, 12144454, 2116339, 13606037, 27378885, 15676917,
             49700111, 20050058, 52713667, 8070817}},
            {{27117677, 23547054, 35826092, 27984343, 1127281, 12772488,
             37262958, 10483305, 55556115, 32525717}},
            {{10637467, 27866368, 5674780, 1072708, 40765276, 26572129,
             65424888, 9177852, 39615702, 15431202}},
        },
        {
            {{20525126, 10892566, 54366392, 12779442, 37615830, 16150074,
             38868345, 14943141, 52052074, 25618500}},
            {{37084402, 5626925, 66557297, 23573344, 753597, 11981191, 25244767,
             30314666, 63752313, 9594023}},
            {{43356201, 2636869, 61944954, 23450613, 585133, 7877383, 11345683,
             27062142, 13352334, 22577348}},
        },
        {
            {{65177046, 28146973, 3304648, 20669563, 17015805, 28677341,
             37325013, 25801949, 53893326, 33235227}},
            {{20239939, 6607058, 6203985, 3483793, 48721888, 32775202, 46385121,
             15077869, 44358105, 14523816}},
            {{27406023, 27512775, 27423595, 29057038, 4996213, 10002360,
             38266833, 29008937, 36936121, 28748764}},
        },
    },
    {
        {
            {{11374242, 12660715, 17861383, 21013599, 10935567, 1099227,
             53222788, 24462691, 39381819, 11358503}},
            {{54378055, 10311866, 1510375, 10778093, 64989409, 24408729,
             32676002, 11149336, 40985213, 4985767}},
            {{48012542, 341146, 60911379, 33315398, 15756972, 24757770, 66125820,
             13794113, 47694557, 17933176}},
        },
        {
            {{6490062, 11940286, 25495923, 25828072, 8668372, 24803116, 3367602,
             6970005, 65417799, 24549641}},
            {{1656478, 13457317, 15370807, 6364910, 13605745, 8362338, 47934242,
             28078708, 50312267, 28522993}},
            {{44835530, 20030007, 67044178, 29220208, 48503227, 22632463,
             46537798, 26546453, 67009010, 23317098}},
        },
        {
            {{17747446, 10039260, 19368299, 29503841, 46478228, 17513145,
             31992682, 17696456, 37848500, 28042460}},
            {{31932008, 28568291, 47496481, 16366579, 22023614, 88450, 11371999,
             29810185, 4882241, 22927527}},
            {{29796488, 37186, 19818052, 10115756, 55279832, 3352735, 18551198,
             3272828, 61917932, 29392022}},
        },
        {
            {{12501267, 4044383, 58495907, 20162046, 34678811, 5136598,
             47878486, 30024734, 330069, 29895023}},
            {{6384877, 2899513, 17807477, 7663917, 64749976, 12363164, 25366522,
             24980540, 66837568, 12071498}},
            {{58743349, 29511910, 25133447, 29037077, 60897836, 2265926,
             34339246, 1936674, 61949167, 3829362}},
        },
        {
            {{28425966, 27718999, 66531773, 28857233, 52891308, 6870929, 7921550,
             26986645, 26333139, 14267664}},
            {{56041645, 11871230, 27385719, 22994888, 62522949, 22365119,
             10004785, 24844944, 45347639, 8930323}},
            {{45911060, 17158396, 25654215, 31829035, 12282011, 11008919,
             1541940, 4757911, 40617363, 17145491}},
        },
        {
            {{13537262, 25794942, 46504023, 10961926, 61186044, 20336366,
             53952279, 6217253, 51165165, 13814989}},
            {{49686272, 15157789, 18705543, 29619, 24409717, 33293956, 27361680,
             9257833, 65152338, 31777517}},
            {{42063564, 23362465, 15366584, 15166509, 54003778, 8423555,
             37937324, 12361134, 48422886, 4578289}},
        },
        {
            {{24579768, 3711570, 1342322, 22374306, 40103728, 14124955,
             44564335, 14074918, 21964432, 8235257}},
            {{60580251, 31142934, 9442965, 27628844, 12025639, 32067012,
             64127349, 31885225, 13006805, 2355433}},
            {{50803946, 19949172, 60476436, 28412082, 16974358, 22643349,
             27202043, 1719366, 1141648, 20758196}},
        },
        {
            {{54244920, 20334445, 58790597, 22536340, 60298718, 28710537,
             13475065, 30420460, 32674894, 13715045}},
            {{11423316, 28086373, 32344215, 8962751, 24989809, 9241752,
             53843611, 16086211, 38367983, 17912338}},
            {{65699196, 12530727, 60740138, 10847386, 19531186, 19422272,
             55399715, 7791793, 39862921, 4383346}},
        },
    },
    {
        {
            {{38137966, 5271446, 65842855, 23817442, 54653627, 16732598,
             62246457, 28647982, 27193556, 6245191}},
            {{51914908, 5362277, 65324971, 2695833, 4960227, 12840725, 23061898,
             3260492, 22510453, 8577507}},
            {{54476394, 11257345, 34415870, 13548176, 66387860, 10879010,
             31168030, 13952092, 37537372, 29918525}},
        },
        {
            {{3877321, 23981693, 32416691, 5405324, 56104457, 19897796,
             3759768, 11935320, 5611860, 8164018}},
            {{50833043, 14667796, 15906460, 12155291, 44997715, 24514713,
             32003001, 24722143, 5773084, 25132323}},
            {{43320746, 25300131, 1950874, 8937633, 18686727, 16459170, 66203139,
             12376319, 31632953, 190926}},
        },
        {
            {{42515238, 17415546, 58684872, 13378745, 14162407, 6901328,
             58820115, 4508563, 41767309, 29926903}},
            {{8884438, 27670423, 6023973, 10104341, 60227295, 28612898, 18722940,
             18768427, 65436375, 827624}},
            {{34388281, 17265135, 34605316, 7101209, 13354605, 2659080,
             65308289, 19446395, 42230385, 1541285}},
        },
        {
            {{2901328, 32436745, 3880375, 23495044, 49487923, 29941650,
             45306746, 29986950, 20456844, 31669399}},
            {{27019610, 12299467, 53450576, 31951197, 54247203, 28692960,
             47568713, 28538373, 29439640, 15138866}},
            {{21536104, 26928012, 34661045, 22864223, 44700786, 5175813,
             61688824, 17193268, 7779327, 109896}},
        },
        {
            {{30279725, 14648750, 59063993, 6425557, 13639621, 32810923, 28698389,
             12180118, 23177719, 33000357}},
            {{26572828, 3405927, 35407164, 12890904, 47843196, 5335865,
             60615096, 2378491, 4439158, 20275085}},
            {{44392139, 3489069, 57883598, 33221678, 18875721, 32414337,
             14819433, 20822905, 49391106, 28092994}},
        },
        {
            {{62052362, 16566550, 15953661, 3767752, 56672365, 15627059,
             66287910, 2177224, 8550082, 18440267}},
            {{48635543, 16596774, 66727204, 15663610, 22860960, 15585581,
             39264755, 29971692, 43848403, 25125843}},
            {{34628313, 15707274, 58902952, 27902350, 29464557, 2713815,
             44383727, 15860481, 45206294, 1494192}},
        },
        {
            {{47546773, 19467038, 41524991, 24254879, 13127841, 759709,
             21923482, 16529112, 8742704, 12967017}},
            {{38643965, 1553204, 32536856, 23080703, 42417258, 33148257,
             58194238, 30620535, 37205105, 15553882}},
            {{21877890, 3230008, 9881174, 10539357, 62311749, 2841331, 11543572,
             14513274, 19375923, 20906471}},
        },
        {
            {{8832269, 19058947, 13253510, 5137575, 5037871, 4078777, 24880818,
             27331716, 2862652, 9455043}},
            {{29306751, 5123106, 20245049, 19404543, 9592565, 8447059, 65031740,
             30564351, 15511448, 4789663}},
            {{46429108, 7004546, 8824831, 24119455, 63063159, 29803695,
             61354101, 108892, 23513200, 16652362}},
        },
    },
    {
        {
            {{33852691, 4144781, 62632835, 26975308, 10770038, 26398890,
             60458447, 20618131, 48789665, 10212859}},
            {{2756062, 8598110, 7383731, 26694540, 22312758, 32449420, 21179800,
             2600940, 57120566, 21047965}},
            {{42463153, 13317461, 36659605, 17900503, 21365573, 22684775,
             11344423, 864440, 64609187, 16844368}},
        },
        {
            {{40676061, 6148328, 49924452, 19080277, 18782928, 33278435,
             44547329, 211299, 2719757, 4940997}},
            {{65784982, 3911312, 60160120, 14759764, 37081714, 7851206,
             21690126, 8518463, 26699843, 5276295}},
            {{53958991, 27125364, 9396248, 365013, 24703301, 23065493, 1321585,
             149635, 51656090, 7159368}},
        },
        {
            {{9987761, 30149673, 17507961, 9505530, 9731535, 31388918, 22356008,
             8312176, 22477218, 25151047}},
            {{18155857, 17049442, 19744715, 9006923, 15154154, 23015456,
             24256459, 28689437, 44560690, 9334108}},
            {{2986088, 28642539, 10776627, 30080588, 10620589, 26471229,
             45695018, 14253544, 44521715, 536905}},
        },
        {
            {{4377737, 8115836, 24567078, 15495314, 11625074, 13064599, 7390551,
             10589625, 10838060, 18134008}},
            {{47766460, 867879, 9277171, 30335973, 52677291, 31567988,
             19295825, 17757482, 6378259, 699185}},
            {{7895007, 4057113, 60027092, 20476675, 49222032, 33231305, 66392824,
             15693154, 62063800, 20180469}},
        },
        {
            {{59371282, 27685029, 52542544, 26147512, 11385653, 13201616,
             31730678, 22591592, 63190227, 23885106}},
            {{10188286, 17783598, 59772502, 13427542, 22223443, 14896287,
             30743455, 7116568, 45322357, 5427592}},
            {{696102, 13206899, 27047647, 22922350, 15285304, 23701253,
             10798489, 28975712, 19236242, 12477404}},
        },
        {
            {{55879425, 11243795, 50054594, 25513566, 66320635, 25386464,
             63211194, 11180503, 43939348, 7733643}},
            {{17800790, 19518253, 40108434, 21787760, 23887826, 3149671,
             23466177, 23016261, 10322026, 15313801}},
            {{26246234, 11968874, 32263343, 28085704, 6830754, 20231401,
             51314159, 33452449, 42659621, 10890803}},
        },
        {
            {{35743198, 10271362, 54448239, 27287163, 16690206, 20491888,
             52126651, 16484930, 25180797, 28219548}},
            {{66522290, 10376443, 34522450, 22268075, 19801892, 10997610,
             2276632, 9482883, 316878, 13820577}},
            {{57226037, 29044064, 64993357, 16457135, 56008783, 11674995,
             30756178, 26039378, 30696929, 29841583}},
        },
        {
            {{32988917, 23951020, 12499365, 7910787, 56491607, 21622917,
             59766047, 23569034, 34759346, 7392472}},
            {{58253184, 15927860, 9866406, 29905021, 64711949, 16898650,
             36699387, 24419436, 25112946, 30627788}},
            {{64604801, 33117465, 25621773, 27875660, 15085041, 28074555,
             42223985, 20028237, 5537437, 19640113}},
        },
    },
    {
        {
            {{55883280, 2320284, 57524584, 10149186, 33664201, 5808647,
             52232613, 31824764, 31234589, 6090599}},
            {{57475529, 116425, 26083934, 2897444, 60744427, 30866345, 609720,
             15878753, 60138459, 24519663}},
            {{39351007, 247743, 51914090, 24551880, 23288160, 23542496,
             43239268, 6503645, 20650474, 1804084}},
        },
        {
            {{39519059, 15456423, 8972517, 8469608, 15640622, 4439847, 3121995,
             23224719, 27842615, 33352104}},
            {{51801891, 2839643, 22530074, 10026331, 4602058, 5048462, 28248656,
             5031932, 55733782, 12714368}},
            {{20807691, 26283607, 29286140, 11421711, 39232341, 19686201,
             45881388, 1035545, 47375635, 12796919}},
        },
        {
            {{12076880, 19253146, 58323862, 21705509, 42096072, 16400683,
             49517369, 20654993, 3480664, 18371617}},
            {{34747315, 5457596, 28548107, 7833186, 7303070, 21600887,
             42745799, 17632556, 33734809, 2771024}},
            {{45719598, 421931, 26597266, 6860826, 22486084, 26817260,
             49971378, 29344205, 42556581, 15673396}},
        },
        {
            {{46924223, 2338215, 19788685, 23933476, 63107598, 24813538,
             46837679, 4733253, 3727144, 20619984}},
            {{6120100, 814863, 55314462, 32931715, 6812204, 17806661, 2019593,
             7975683, 31123697, 22595451}},
            {{30069250, 22119100, 30434653, 2958439, 18399564, 32578143,
             12296868, 9204260, 50676426, 9648164}},
        },
        {
            {{32705413, 32003455, 30705657, 7451065, 55303258, 9631812, 3305266,
             5248604, 41100532, 22176930}},
            {{17219846, 2375039, 35537917, 27978816, 47649184, 9219902, 294711,
             15298639, 2662509, 17257359}},
            {{65935918, 25995736, 62742093, 29266687, 45762450, 25120105,
             32087528, 32331655, 32247247, 19164571}},
        },
        {
            {{14312609, 1221556, 17395390, 24854289, 62163122, 24869796,
             38911119, 23916614, 51081240, 20175586}},
            {{65680039, 23875441, 57873182, 6549686, 59725795, 33085767, 23046501,
             9803137, 17597934, 2346211}},
            {{18510781, 15337574, 26171504, 981392, 44867312, 7827555,
             43617730, 22231079, 3059832, 21771562}},
        },
        {
            {{10141598, 6082907, 17829293, 31606789, 9830091, 13613136,
             41552228, 28009845, 33606651, 3592095}},
            {{33114149, 17665080, 40583177, 20211034, 33076704, 8716171,
             1151462, 1521897, 66126199, 26716628}},
            {{34169699, 29298616, 23947180, 33230254, 34035889, 21248794,
             50471177, 3891703, 26353178, 693168}},
        },
        {
            {{30374239, 1595580, 50224825, 13186930, 4600344, 406904, 9585294,
             33153764, 31375463, 14369965}},
            {{52738210, 25781902, 1510300, 6434173, 48324075, 27291703,
             32732229, 20445593, 17901440, 16011505}},
            {{18171223, 21619806, 54608461, 15197121, 56070717, 18324396,
             47936623, 17508055, 8764034, 12309598}},
        },
    },
    {
        {
            {{5975889, 28311244, 47649501, 23872684, 55567586, 14015781,
             43443107, 1228318, 17544096, 22960650}},
            {{5811932, 31839139, 3442886, 31285122, 48741515, 25194890,
             49064820, 18144304, 61543482, 12348899}},
            {{35709185, 11407554, 25755363, 6891399, 63851926, 14872273,
             42259511, 8141294, 56476330, 32968952}},
        },
        {
            {{54433560, 694025, 62032719, 13300343, 14015258, 19103038,
             57410191, 22225381, 30944592, 1130208}},
            {{8247747, 26843490, 40546482, 25845122, 52706924, 18905521,
             4652151, 2488540, 23550156, 33283200}},
            {{17294297, 29765994, 7026747, 15626851, 22990044, 113481, 2267737,
             27646286, 66700045, 33416712}},
        },
        {
            {{16091066, 17300506, 18599251, 7340678, 2137637, 32332775,
             63744702, 14550935, 3260525, 26388161}},
            {{62198760, 20221544, 18550886, 10864893, 50649539, 26262835,
             44079994, 20349526, 54360141, 2701325}},
            {{58534169, 16099414, 4629974, 17213908, 46322650, 27548999,
             57090500, 9276970, 11329923, 1862132}},
        },
        {
            {{14763057, 17650824, 36190593, 3689866, 3511892, 10313526,
             45157776, 12219230, 58070901, 32614131}},
            {{8894987, 30108338, 6150752, 3013931, 301220, 15693451, 35127648,
             30644714, 51670695, 11595569}},
            {{15214943, 3537601, 40870142, 19495559, 4418656, 18323671,
             13947275, 10730794, 53619402, 29190761}},
        },
        {
            {{64570558, 7682792, 32759013, 263109, 37124133, 25598979,
             44776739, 23365796, 977107, 699994}},
            {{54642373, 4195083, 57897332, 550903, 51543527, 12917919,
             19118110, 33114591, 36574330, 19216518}},
            {{31788442, 19046775, 4799988, 7372237, 8808585, 18806489, 9408236,
             23502657, 12493931, 28145115}},
        },
        {
            {{41428258, 5260743, 47873055, 27269961, 63412921, 16566086,
             27218280, 2607121, 29375955, 6024730}},
            {{842132, 30759739, 62345482, 24831616, 26332017, 21148791,
             11831879, 6985184, 57168503, 2854095}},
            {{62261602, 25585100, 2516241, 27706719, 9695690, 26333246, 16512644,
             960770, 12121869, 16648078}},
        },
        {
            {{51890212, 14667095, 53772635, 2013716, 30598287, 33090295,
             35603941, 25672367, 20237805, 2838411}},
            {{47820798, 4453151, 15298546, 17376044, 22115042, 17581828,
             12544293, 20083975, 1068880, 21054527}},
            {{57549981, 17035596, 33238497, 13506958, 30505848, 32439836,
             58621956, 30924378, 12521377, 4845654}},
        },
        {
            {{38910324, 10744107, 64150484, 10199663, 7759311, 20465832,
             3409347, 32681032, 60626557, 20668561}},
            {{43547042, 6230155, 46726851, 10655313, 43068279, 21933259,
             10477733, 32314216, 63995636, 13974497}},
            {{12966261, 15550616, 35069916, 31939085, 21025979, 32924988,
             5642324, 7188737, 18895762, 12629579}},
        },
    },
    {
        {
            {{14741879, 18607545, 22177207, 21833195, 1279740, 8058600,
             11758140, 789443, 32195181, 3895677}},
            {{10758205, 15755439, 62598914, 9243697, 62229442, 6879878, 64904289,
             29988312, 58126794, 4429646}},
            {{64654951, 15725972, 46672522, 23143759, 61304955, 22514211,
             59972993, 21911536, 18047435, 18272689}},
        },
        {
            {{41935844, 22247266, 29759955, 11776784, 44846481, 17733976,
             10993113, 20703595, 49488162, 24145963}},
            {{21987233, 700364, 42603816, 14972007, 59334599, 27836036,
             32155025, 2581431, 37149879, 8773374}},
            {{41540495, 454462, 53896929, 16126714, 25240068, 8594567,
             20656846, 12017935, 59234475, 19634276}},
        },
        {
            {{6028163, 6263078, 36097058, 22252721, 66289944, 2461771,
             35267690, 28086389, 65387075, 30777706}},
            {{54829870, 16624276, 987579, 27631834, 32908202, 1248608, 7719845,
             29387734, 28408819, 6816612}},
            {{56750770, 25316602, 19549650, 21385210, 22082622, 16147817,
             20613181, 13982702, 56769294, 5067942}},
        },
        {
            {{36602878, 29732664, 12074680, 13582412, 47230892, 2443950,
             47389578, 12746131, 5331210, 23448488}},
            {{30528792, 3601899, 65151774, 4619784, 39747042, 18118043,
             24180792, 20984038, 27679907, 31905504}},
            {{9402385, 19597367, 32834042, 10838634, 40528714, 20317236,
             26653273, 24868867, 22611443, 20839026}},
        },
        {
            {{22190590, 1118029, 22736441, 15130463, 36648172, 27563110,
             19189624, 28905490, 4854858, 6622139}},
            {{58798126, 30600981, 58846284, 30166382, 56707132, 33282502,
             13424425, 29987205, 26404408, 13001963}},
            {{35867026, 18138731, 64114613, 8939345, 11562230, 20713762,
             41044498, 21932711, 51703708, 11020692}},
        },
        {
            {{1866042, 25604943, 59210214, 23253421, 12483314, 13477547,
             3175636, 21130269, 28761761, 1406734}},
            {{66660290, 31776765, 13018550, 3194501, 57528444, 22392694,
             24760584, 29207344, 25577410, 20175752}},
            {{42818486, 4759344, 66418211, 31701615, 2066746, 10693769,
             37513074, 9884935, 57739938, 4745409}},
        },
        {
            {{57967561, 6049713, 47577803, 29213020, 35848065, 9944275,
             51646856, 22242579, 10931923, 21622501}},
            {{50547351, 14112679, 59096219, 4817317, 59068400, 22139825,
             44255434, 10856640, 46638094, 13434653}},
            {{22759470, 23480998, 50342599, 31683009, 13637441, 23386341,
             1765143, 20900106, 28445306, 28189722}},
        },
        {
            {{29875063, 12493613, 2795536, 29768102, 1710619, 15181182,
             56913147, 24765756, 9074233, 1167180}},
            {{40903181, 11014232, 57266213, 30918946, 40200743, 7532293,
             48391976, 24018933, 3843902, 9367684}},
            {{56139269, 27150720, 9591133, 9582310, 11349256, 108879, 16235123,
             8601684, 66969667, 4242894}},
        },
    },
    {
        {
            {{22092954, 20363309, 65066070, 21585919, 32186752, 22037044,
             60534522, 2470659, 39691498, 16625500}},
            {{56051142, 3042015, 13770083, 24296510, 584235, 33009577, 59338006,
             2602724, 39757248, 14247412}},
            {{6314156, 23289540, 34336361, 15957556, 56951134, 168749,
             58490057, 14290060, 27108877, 32373552}},
        },
        {
            {{58522267, 26383465, 13241781, 10960156, 34117849, 19759835,
             33547975, 22495543, 39960412, 981873}},
            {{22833421, 9293594, 34459416, 19935764, 57971897, 14756818,
             44180005, 19583651, 56629059, 17356469}},
            {{59340277, 3326785, 38997067, 10783823, 19178761, 14905060,
             22680049, 13906969, 51175174, 3797898}},
        },
        {
            {{21721337, 29341686, 54902740, 9310181, 63226625, 19901321,
             23740223, 30845200, 20491982, 25512280}},
            {{9209251, 18419377, 53852306, 27386633, 66377847, 15289672,
             25947805, 15286587, 30997318, 26851369}},
            {{7392013, 16618386, 23946583, 25514540, 53843699, 32020573,
             52911418, 31232855, 17649997, 33304352}},
        },
        {
            {{57807776, 19360604, 30609525, 30504889, 41933794, 32270679,
             51867297, 24028707, 64875610, 7662145}},
            {{49550191, 1763593, 33994528, 15908609, 37067994, 21380136,
             7335079, 25082233, 63934189, 3440182}},
            {{47219164, 27577423, 42997570, 23865561, 10799742, 16982475,
             40449, 29122597, 4862399, 1133}},
        },
        {
            {{34252636, 25680474, 61686474, 14860949, 50789833, 7956141,
             7258061, 311861, 36513873, 26175010}},
            {{63335436, 31988495, 28985339, 7499440, 24445838, 9325937, 29727763,
             16527196, 18278453, 15405622}},
            {{62726958, 8508651, 47210498, 29880007, 61124410, 15149969,
             53795266, 843522, 45233802, 13626196}},
        },
        {
            {{2281448, 20067377, 56193445, 30944521, 1879357, 16164207,
             56324982, 3953791, 13340839, 15928663}},
            {{31727126, 26374577, 48671360, 25270779, 2875792, 17164102,
             41838969, 26539605, 43656557, 5964752}},
            {{4100401, 27594980, 49929526, 6017713, 48403027, 12227140,
             40424029, 11344143, 2538215, 25983677}},
        },
        {
            {{57675240, 6123112, 11159803, 31397824, 30016279, 14966241,
             46633881, 1485420, 66479608, 17595569}},
            {{40304287, 4260918, 11851389, 9658551, 35091757, 16367491,
             46903439, 20363143, 11659921, 22439314}},
            {{26180377, 10015009, 36264640, 24973138, 5418196, 9480663, 2231568,
             23384352, 33100371, 32248261}},
        },
        {
            {{15121094, 28352561, 56718958, 15427820, 39598927, 17561924,
             21670946, 4486675, 61177054, 19088051}},
            {{16166467, 24070699, 56004733, 6023907, 35182066, 32189508,
             2340059, 17299464, 56373093, 23514607}},
            {{28042865, 29997343, 54982337, 12259705, 63391366, 26608532,
             6766452, 24864833, 18036435, 5803270}},
        },
    },
    {
        {
            {{66291264, 6763911, 11803561, 1585585, 10958447, 30883267, 23855390,
             4598332, 60949433, 19436993}},
            {{36077558, 19298237, 17332028, 31170912, 31312681, 27587249,
             696308, 50292, 47013125, 11763583}},
            {{66514282, 31040148, 34874710, 12643979, 12650761, 14811489, 665117,
             20940800, 47335652, 22840869}},
        },
        {
            {{30464590, 22291560, 62981387, 20819953, 19835326, 26448819,
             42712688, 2075772, 50088707, 992470}},
            {{18357166, 26559999, 7766381, 16342475, 37783946, 411173, 14578841,
             8080033, 55534529, 22952821}},
            {{19598397, 10334610, 12555054, 2555664, 18821899, 23214652,
             21873262, 16014234, 26224780, 16452269}},
        },
        {
            {{36884939, 5145195, 5944548, 16385966, 3976735, 2009897, 55731060,
             25936245, 46575034, 3698649}},
            {{14187449, 3448569, 56472628, 22743496, 44444983, 30120835,
             7268409, 22663988, 27394300, 12015369}},
            {{19695742, 16087646, 28032085, 12999827, 6817792, 11427614,
             20244189, 32241655, 53849736, 30151970}},
        },
        {
            {{30860084, 12735208, 65220619, 28854697, 50133957, 2256939,
             58942851, 12298311, 58558340, 23160969}},
            {{61389038, 22309106, 65198214, 15569034, 26642876, 25966672,
             61319509, 18435777, 62132699, 12651792}},
            {{64260450, 9953420, 11531313, 28271553, 26895122, 20857343,
             53990043, 17036529, 9768697, 31021214}},
        },
        {
            {{42389405, 1894650, 66821166, 28850346, 15348718, 25397902,
             32767512, 12765450, 4940095, 10678226}},
            {{18860224, 15980149, 48121624, 31991861, 40875851, 22482575,
             59264981, 13944023, 42736516, 16582018}},
            {{51604604, 4970267, 37215820, 4175592, 46115652, 31354675,
             55404809, 15444559, 56105103, 7989036}},
        },
        {
            {{31490433, 5568061, 64696061, 2182382, 34772017, 4531685,
             35030595, 6200205, 47422751, 18754260}},
            {{49800177, 17674491, 35586086, 33551600, 34221481, 16375548,
             8680158, 17182719, 28550067, 26697300}},
            {{38981977, 27866340, 16837844, 31733974, 60258182, 12700015,
             37068883, 4364037, 1155602, 5988841}},
        },
        {
            {{21890435, 20281525, 54484852, 12154348, 59276991, 15300495,
             23148983, 29083951, 24618406, 8283181}},
            {{33972757, 23041680, 9975415, 6841041, 35549071, 16356535,
             3070187, 26528504, 1466168, 10740210}},
            {{65599446, 18066246, 53605478, 22898515, 32799043, 909394,
             53169961, 27774712, 34944214, 18227391}},
        },
        {
            {{3960804, 19286629, 39082773, 17636380, 47704005, 13146867,
             15567327, 951507, 63848543, 32980496}},
            {{24740822, 5052253, 37014733, 8961360, 25877428, 6165135,
             42740684, 14397371, 59728495, 27410326}},
            {{38220480, 3510802, 39005586, 32395953, 55870735, 22922977,
             51667400, 19101303, 65483377, 27059617}},
        },
    },
    {
        {
            {{793280, 24323954, 8836301, 27318725, 39747955, 31184838, 33152842,
             28669181, 57202663, 32932579}},
            {{5666214, 525582, 20782575, 25516013, 42570364, 14657739, 16099374,
             1468826, 60937436, 18367850}},
            {{62249590, 29775088, 64191105, 26806412, 7778749, 11688288,
             36704511, 23683193, 65549940, 23690785}},
        },
        {
            {{10896313, 25834728, 824274, 472601, 47648556, 3009586, 25248958,
             14783338, 36527388, 17796587}},
            {{10566929, 12612572, 35164652, 11118702, 54475488, 12362878,
             21752402, 8822496, 24003793, 14264025}},
            {{27713843, 26198459, 56100623, 9227529, 27050101, 2504721,
             23886875, 20436907, 13958494, 27821979}},
        },
        {
            {{43627235, 4867225, 39861736, 3900520, 29838369, 25342141,
             35219464, 23512650, 7340520, 18144364}},
            {{4646495, 25543308, 44342840, 22021777, 23184552, 8566613,
             31366726, 32173371, 52042079, 23179239}},
            {{49838347, 12723031, 50115803, 14878793, 21619651, 27356856,
             27584816, 3093888, 58265170, 3849920}},
        },
        {
            {{58043933, 2103171, 25561640, 18428694, 61869039, 9582957,
             32477045, 24536477, 5002293, 18004173}},
            {{55051311, 22376525, 21115584, 20189277, 8808711, 21523724,
             16489529, 13378448, 41263148, 12741425}},
            {{61162478, 10645102, 36197278, 15390283, 63821882, 26435754,
             24306471, 15852464, 28834118, 25908360}},
        },
        {
            {{49773116, 24447374, 42577584, 9434952, 58636780, 32971069,
             54018092, 455840, 20461858, 5491305}},
            {{13669229, 17458950, 54626889, 23351392, 52539093, 21661233,
             42112877, 11293806, 38520660, 24132599}},
            {{28497909, 6272777, 34085870, 14470569, 8906179, 32328802,
             18504673, 19389266, 29867744, 24758489}},
        },
        {
            {{50901822, 13517195, 39309234, 19856633, 24009063, 27180541,
             60741263, 20379039, 22853428, 29542421}},
            {{24191359, 16712145, 53177067, 15217830, 14542237, 1646131,
             18603514, 22516545, 12876622, 31441985}},
            {{17902668, 4518229, 66697162, 30725184, 26878216, 5258055, 54248111,
             608396, 16031844, 3723494}},
        },
        {
            {{38476072, 12763727, 46662418, 7577503, 33001348, 20536687,
             17558841, 25681542, 23896953, 29240187}},
            {{47103464, 21542479, 31520463, 605201, 2543521, 5991821, 64163800,
             7229063, 57189218, 24727572}},
            {{28816026, 298879, 38943848, 17633493, 19000927, 31888542,
             54428030, 30605106, 49057085, 31471516}},
        },
        {
            {{16000882, 33209536, 3493091, 22107234, 37604268, 20394642,
             12577739, 16041268, 47393624, 7847706}},
            {{10151868, 10572098, 27312476, 7922682, 14825339, 4723128,
             34252933, 27035413, 57088296, 3852847}},
            {{55678375, 15697595, 45987307, 29133784, 5386313, 15063598,
             16514493, 17622322, 29330898, 18478208}},
        },
    },
    {
        {
            {{41609129, 29175637, 51885955, 26653220, 16615730, 2051784,
             3303702, 15490, 39560068, 12314390}},
            {{15683501, 27551389, 18109119, 23573784, 15337967, 27556609,
             50391428, 15921865, 16103996, 29823217}},
            {{43939021, 22773182, 13588191, 31925625, 63310306, 32479502,
             47835256, 5402698, 37293151, 23713330}},
        },
        {
            {{23190676, 2384583, 34394524, 3462153, 37205209, 32025299,
             55842007, 8911516, 41903005, 2739712}},
            {{21374101, 30000182, 33584214, 9874410, 15377179, 11831242,
             33578960, 6134906, 4931255, 11987849}},
            {{67101132, 30575573, 50885377, 7277596, 105524, 33232381, 35628324,
             13861387, 37032554, 10117929}},
        },
        {
            {{37607694, 22809559, 40945095, 13051538, 41483300, 5089642,
             60783361, 6704078, 12890019, 15728940}},
            {{45136504, 21783052, 66157804, 29135591, 14704839, 2695116, 903376,
             23126293, 12885166, 8311031}},
            {{49592363, 5352193, 10384213, 19742774, 7506450, 13453191,
             26423267, 4384730, 1888765, 28119028}},
        },
        {
            {{41291507, 30447119, 53614264, 30371925, 30896458, 19632703,
             34857219, 20846562, 47644429, 30214188}},
            {{43500868, 30888657, 66582772, 4651135, 5765089, 4618330, 6092245,
             14845197, 17151279, 23700316}},
            {{42278406, 20820711, 51942885, 10367249, 37577956, 33289075,
             22825804, 26467153, 50242379, 16176524}},
        },
        {
            {{43525589, 6564960, 20063689, 3798228, 62368686, 7359224, 2006182,
             23191006, 38362610, 23356922}},
            {{56482264, 29068029, 53788301, 28429114, 3432135, 27161203,
             23632036, 31613822, 32808309, 1099883}},
            {{15030958, 5768825, 39657628, 30667132, 60681485, 18193060,
             51830967, 26745081, 2051440, 18328567}},
        },
        {
            {{63746541, 26315059, 7517889, 9824992, 23555850, 295369, 5148398,
             19400244, 44422509, 16633659}},
            {{4577067, 16802144, 13249840, 18250104, 19958762, 19017158,
             18559669, 22794883, 8402477, 23690159}},
            {{38702534, 32502850, 40318708, 32646733, 49896449, 22523642,
             9453450, 18574360, 17983009, 9967138}},
        },
        {
            {{41346370, 6524721, 26585488, 9969270, 24709298, 1220360, 65430874,
             7806336, 17507396, 3651560}},
            {{56688388, 29436320, 14584638, 15971087, 51340543, 8861009,
             26556809, 27979875, 48555541, 22197296}},
            {{2839082, 14284142, 4029895, 3472686, 14402957, 12689363, 40466743,
             8459446, 61503401, 25932490}},
        },
        {
            {{62269556, 30018987, 9744960, 2871048, 25113978, 3187018, 41998051,
             32705365, 17258083, 25576693}},
            {{18164541, 22959256, 49953981, 32012014, 19237077, 23809137,
             23357532, 18337424, 26908269, 12150756}},
            {{36843994, 25906566, 5112248, 26517760, 65609056, 26580174, 43167,
             28016731, 34806789, 16215818}},
        },
    },
    {
        {
            {{60209940, 9824393, 54804085, 29153342, 35711722, 27277596,
             32574488, 12532905, 59605792, 24879084}},
            {{39765323, 17038963, 39957339, 22831480, 946345, 16291093,
             254968, 7168080, 21676107, 31611404}},
            {{21260942, 25129680, 50276977, 21633609, 43430902, 3968120,
             63456915, 27338965, 63552672, 25641356}},
        },
        {
            {{16544735, 13250366, 50304436, 15546241, 62525861, 12757257,
             64646556, 24874095, 48201831, 23891632}},
            {{64693606, 17976703, 18312302, 4964443, 51836334, 20900867,
             26820650, 16690659, 25459437, 28989823}},
            {{41964155, 11425019, 28423002, 22533875, 60963942, 17728207,
             9142794, 31162830, 60676445, 31909614}},
        },
        {
            {{44004212, 6253475, 16964147, 29785560, 41994891, 21257994,
             39651638, 17209773, 6335691, 7249989}},
            {{36775618, 13979674, 7503222, 21186118, 55152142, 28932738,
             36836594, 2682241, 25993170, 21075909}},
            {{4364628, 5930691, 32304656, 23509878, 59054082, 15091130,
             22857016, 22955477, 31820367, 15075278}},
        },
        {
            {{31879134, 24635739, 17258760, 90626, 59067028, 28636722, 24162787,
             23903546, 49138625, 12833044}},
            {{19073683, 14851414, 42705695, 21694263, 7625277, 11091125,
             47489674, 2074448, 57694925, 14905376}},
            {{24483648, 21618865, 64589997, 22007013, 65555733, 15355505,
             41826784, 9253128, 27628530, 25998952}},
        },
        {
            {{17597607, 8340603, 19355617, 552187, 26198470, 30377849, 4593323,
             24396850, 52997988, 15297015}},
            {{510886, 14337390, 35323607, 16638631, 6328095, 2713355, 46891447,
             21690211, 8683220, 2921426}},
            {{18606791, 11874196, 27155355, 28272950, 43077121, 6265445,
             41930624, 32275507, 4674689, 13890525}},
        },
        {
            {{13609624, 13069022, 39736503, 20498523, 24360585, 9592974,
             14977157, 9835105, 4389687, 288396}},
            {{9922506, 33035038, 13613106, 5883594, 48350519, 33120168, 54804801,
             8317627, 23388070, 16052080}},
            {{12719997, 11937594, 35138804, 28525742, 26900119, 8561328,
             46953177, 21921452, 52354592, 22741539}},
        },
        {
            {{15961858, 14150409, 26716931, 32888600, 44314535, 13603568,
             11829573, 7467844, 38286736, 929274}},
            {{11038231, 21972036, 39798381, 26237869, 56610336, 17246600,
             43629330, 24182562, 45715720, 2465073}},
            {{20017144, 29231206, 27915241, 1529148, 12396362, 15675764,
             13817261, 23896366, 2463390, 28932292}},
        },
        {
            {{50749986, 20890520, 55043680, 4996453, 65852442, 1073571,
             9583558, 12851107, 4003896, 12673717}},
            {{65377275, 18398561, 63845933, 16143081, 19294135, 13385325,
             14741514, 24450706, 7903885, 2348101}},
            {{24536016, 17039225, 12715591, 29692277, 1511292, 10047386,
             63266518, 26425272, 38731325, 10048126}},
        },
    },
    {
        {
            {{54486638, 27349611, 30718824, 2591312, 56491836, 12192839,
             18873298, 26257342, 34811107, 15221631}},
            {{40630742, 22450567, 11546243, 31701949, 9180879, 7656409,
             45764914, 2095754, 29769758, 6593415}},
            {{35114656, 30646970, 4176911, 3264766, 12538965, 32686321, 26312344,
             27435754, 30958053, 8292160}},
        },
        {
            {{31429803, 19595316, 29173531, 15632448, 12174511, 30794338,
             32808830, 3977186, 26143136, 30405556}},
            {{22648882, 1402143, 44308880, 13746058, 7936347, 365344, 58440231,
             31879998, 63350620, 31249806}},
            {{51616947, 8012312, 64594134, 20851969, 43143017, 23300402,
             65496150, 32018862, 50444388, 8194477}},
        },
        {
            {{27338066, 26047012, 59694639, 10140404, 48082437, 26964542,
             27277190, 8855376, 28572286, 3005164}},
            {{26287105, 4821776, 25476601, 29408529, 63344350, 17765447,
             49100281, 1182478, 41014043, 20474836}},
            {{59937691, 3178079, 23970071, 6201893, 49913287, 29065239,
             45232588, 19571804, 32208682, 32356184}},
        },
        {
            {{50451143, 2817642, 56822502, 14811297, 6024667, 13349505,
             39793360, 23056589, 39436278, 22014573}},
            {{15941010, 24148500, 45741813, 8062054, 31876073, 33315803,
             51830470, 32110002, 15397330, 29424239}},
            {{8934485, 20068965, 43822466, 20131190, 34662773, 14047985,
             31170398, 32113411, 39603297, 15087183}},
        },
        {
            {{48751602, 31397940, 24524912, 16876564, 15520426, 27193656,
             51606457, 11461895, 16788528, 27685490}},
            {{65161459, 16013772, 21750665, 3714552, 49707082, 17498998,
             63338576, 23231111, 31322513, 21938797}},
            {{21426636, 27904214, 53460576, 28206894, 38296674, 28633461,
             48833472, 18933017, 13040861, 21441484}},
        },
        {
            {{11293895, 12478086, 39972463, 15083749, 37801443, 14748871,
             14555558, 20137329, 1613710, 4896935}},
            {{41213962, 15323293, 58619073, 25496531, 25967125, 20128972,
             2825959, 28657387, 43137087, 22287016}},
            {{51184079, 28324551, 49665331, 6410663, 3622847, 10243618,
             20615400, 12405433, 43355834, 25118015}},
        },
        {
            {{60017550, 12556207, 46917512, 9025186, 50036385, 4333800,
             4378436, 2432030, 23097949, 32988414}},
            {{4565804, 17528778, 20084411, 25711615, 1724998, 189254, 24767264,
             10103221, 48596551, 2424777}},
            {{366633, 21577626, 8173089, 26664313, 30788633, 5745705, 59940186,
             1344108, 63466311, 12412658}},
        },
        {
            {{43107073, 7690285, 14929416, 33386175, 34898028, 20141445,
             24162696, 18227928, 63967362, 11179384}},
            {{18289503, 18829478, 8056944, 16430056, 45379140, 7842513,
             61107423, 32067534, 48424218, 22110928}},
            {{476239, 6601091, 60956074, 23831056, 17503544, 28690532, 27672958,
             13403813, 11052904, 5219329}},
        },
    },
    {
        {
            {{20678527, 25178694, 34436965, 8849122, 62099106, 14574751,
             31186971, 29580702, 9014761, 24975376}},
            {{53464795, 23204192, 51146355, 5075807, 65594203, 22019831,
             34006363, 9160279, 8473550, 30297594}},
            {{24900749, 14435722, 17209120, 18261891, 44516588, 9878982,
             59419555, 17218610, 42540382, 11788947}},
        },
        {
            {{63990690, 22159237, 53306774, 14797440, 9652448, 26708528,
             47071426, 10410732, 42540394, 32095740}},
            {{51449703, 16736705, 44641714, 10215877, 58011687, 7563910,
             11871841, 21049238, 48595538, 8464117}},
            {{43708233, 8348506, 52522913, 32692717, 63158658, 27181012,
             14325288, 8628612, 33313881, 25183915}},
        },
        {
            {{46921872, 28586496, 22367355, 5271547, 66011747, 28765593,
             42303196, 23317577, 58168128, 27736162}},
            {{60160060, 31759219, 34483180, 17533252, 32635413, 26180187,
             15989196, 20716244, 28358191, 29300528}},
            {{43547083, 30755372, 34757181, 31892468, 57961144, 10429266,
             50471180, 4072015, 61757200, 5596588}},
        },
        {
            {{38872266, 30164383, 12312895, 6213178, 3117142, 16078565,
             29266239, 2557221, 1768301, 15373193}},
            {{59865506, 30307471, 62515396, 26001078, 66980936, 32642186, 66017961,
             29049440, 42448372, 3442909}},
            {{36898293, 5124042, 14181784, 8197961, 18964734, 21615339,
             22597930, 7176455, 48523386, 13365929}},
        },
        {
            {{59231455, 32054473, 8324672, 4690079, 6261860, 890446, 24538107,
             24984246, 57419264, 30522764}},
            {{25008885, 22782833, 62803832, 23916421, 16265035, 15721635,
             683793, 21730648, 15723478, 18390951}},
            {{57448220, 12374378, 40101865, 26528283, 59384749, 21239917,
             11879681, 5400171, 519526, 32318556}},
        },
        {
            {{22258397, 17222199, 59239046, 14613015, 44588609, 30603508,
             46754982, 7315966, 16648397, 7605640}},
            {{59027556, 25089834, 58885552, 9719709, 19259459, 18206220,
             23994941, 28272877, 57640015, 4763277}},
            {{45409620, 9220968, 51378240, 1084136, 41632757, 30702041,
             31088446, 25789909, 55752334, 728111}},
        },
        {
            {{26047201, 21802961, 60208540, 17032633, 24092067, 9158119,
             62835319, 20998873, 37743427, 28056159}},
            {{17510331, 33231575, 5854288, 8403524, 17133918, 30441820, 38997856,
             12327944, 10750447, 10014012}},
            {{56796096, 3936951, 9156313, 24656749, 16498691, 32559785,
             39627812, 32887699, 3424690, 7540221}},
        },
        {
            {{30322361, 26590322, 11361004, 29411115, 7433303, 4989748, 60037442,
             17237212, 57864598, 15258045}},
            {{13054543, 30774935, 19155473, 469045, 54626067, 4566041, 5631406,
             2711395, 1062915, 28418087}},
            {{47868616, 22299832, 37599834, 26054466, 61273100, 13005410,
             61042375, 12194496, 32960380, 1459310}},
        },
    },
    {
        {
            {{19852015, 7027924, 23669353, 10020366, 8586503, 26896525, 394196,
             27452547, 18638002, 22379495}},
            {{31395515, 15098109, 26581030, 8030562, 50580950, 28547297,
             9012485, 25970078, 60465776, 28111795}},
            {{57916680, 31207054, 65111764, 4529533, 25766844, 607986, 67095642,
             9677542, 34813975, 27098423}},
        },
        {
            {{64664349, 33404494, 29348901, 8186665, 1873760, 12489863, 36174285,
             25714739, 59256019, 25416002}},
            {{51872508, 18120922, 7766469, 746860, 26346930, 23332670,
             39775412, 10754587, 57677388, 5203575}},
            {{31834314, 14135496, 66338857, 5159117, 20917671, 16786336,
             59640890, 26216907, 31809242, 7347066}},
        },
        {
            {{57502122, 21680191, 20414458, 13033986, 13716524, 21862551,
             19797969, 21343177, 15192875, 31466942}},
            {{54445282, 31372712, 1168161, 29749623, 26747876, 19416341,
             10609329, 12694420, 33473243, 20172328}},
            {{33184999, 11180355, 15832085, 22169002, 65475192, 225883,
             15089336, 22530529, 60973201, 14480052}},
        },
        {
            {{31308717, 27934434, 31030839, 31657333, 15674546, 26971549,
             5496207, 13685227, 27595050, 8737275}},
            {{46790012, 18404192, 10933842, 17376410, 8335351, 26008410,
             36100512, 20943827, 26498113, 66511}},
            {{22644435, 24792703, 50437087, 4884561, 64003250, 19995065,
             30540765, 29267685, 53781076, 26039336}},
        },
        {
            {{39091017, 9834844, 18617207, 30873120, 63706907, 20246925,
             8205539, 13585437, 49981399, 15115438}},
            {{23711543, 32881517, 31206560, 25191721, 6164646, 23844445,
             33572981, 32128335, 8236920, 16492939}},
            {{43198286, 20038905, 40809380, 29050590, 25005589, 25867162,
             19574901, 10071562, 6708380, 27332008}},
        },
        {
            {{2101372, 28624378, 19702730, 2367575, 51681697, 1047674, 5301017,
             9328700, 29955601, 21876122}},
            {{3096359, 9271816, 45488000, 18032587, 52260867, 25961494,
             41216721, 20918836, 57191288, 6216607}},
            {{34493015, 338662, 41913253, 2510421, 37895298, 19734218,
             24822829, 27407865, 40341383, 7525078}},
        },
        {
            {{44042215, 19568808, 16133486, 25658254, 63719298, 778787,
             66198528, 30771936, 47722230, 11994100}},
            {{21691500, 19929806, 66467532, 19187410, 3285880, 30070836,
             42044197, 9718257, 59631427, 13381417}},
            {{18445390, 29352196, 14979845, 11622458, 65381754, 29971451,
             23111647, 27179185, 28535281, 15779576}},
        },
        {
            {{30098034, 3089662, 57874477, 16662134, 45801924, 11308410,
             53040410, 12021729, 9955285, 17251076}},
            {{9734894, 18977602, 59635230, 24415696, 2060391, 11313496,
             48682835, 9924398, 20194861, 13380996}},
            {{40730762, 25589224, 44941042, 15789296, 49053522, 27385639,
             65123949, 15707770, 26342023, 10146099}},
        },
    },
    {
        {
            {{41091971, 33334488, 21339190, 33513044, 19745255, 30675732,
             37471583, 2227039, 21612326, 33008704}},
            {{54031477, 1184227, 23562814, 27583990, 46757619, 27205717,
             25764460, 12243797, 46252298, 11649657}},
            {{57077370, 11262625, 27384172, 2271902, 26947504, 17556661, 39943,
             6114064, 33514190, 2333242}},
        },
        {
            {{45675257, 21132610, 8119781, 7219913, 45278342, 24538297,
             60429113, 20883793, 24350577, 20104431}},
            {{62992557, 22282898, 43222677, 4843614, 37020525, 690622,
             35572776, 23147595, 8317859, 12352766}},
            {{18200138, 19078521, 34021104, 30857812, 43406342, 24451920,
             43556767, 31266881, 20712162, 6719373}},
        },
        {
            {{26656189, 6075253, 59250308, 1886071, 38764821, 4262325, 11117530,
             29791222, 26224234, 30256974}},
            {{49939907, 18700334, 63713187, 17184554, 47154818, 14050419,
             21728352, 9493610, 18620611, 17125804}},
            {{53785524, 13325348, 11432106, 5964811, 18609221, 6062965,
             61839393, 23828875, 36407290, 17074774}},
        },
        {
            {{43248326, 22321272, 26961356, 1640861, 34695752, 16816491,
             12248508, 28313793, 13735341, 1934062}},
            {{25089769, 6742589, 17081145, 20148166, 21909292, 17486451,
             51972569, 29789085, 45830866, 5473615}},
            {{31883658, 25593331, 1083431, 21982029, 22828470, 13290673,
             59983779, 12469655, 29111212, 28103418}},
        },
        {
            {{24244947, 18504025, 40845887, 2791539, 52111265, 16666677,
             24367466, 6388839, 56813277, 452382}},
            {{41468082, 30136590, 5217915, 16224624, 19987036, 29472163,
             42872612, 27639183, 15766061, 8407814}},
            {{46701865, 13990230, 15495425, 16395525, 5377168, 15166495,
             58191841, 29165478, 59040954, 2276717}},
        },
        {
            {{30157899, 12924066, 49396814, 9245752, 19895028, 3368142,
             43281277, 5096218, 22740376, 26251015}},
            {{2041139, 19298082, 7783686, 13876377, 41161879, 20201972,
             24051123, 13742383, 51471265, 13295221}},
            {{33338218, 25048699, 12532112, 7977527, 9106186, 31839181,
             49388668, 28941459, 62657506, 18884987}},
        },
        {
            {{47063583, 5454096, 52762316, 6447145, 28862071, 1883651,
             64639598, 29412551, 7770568, 9620597}},
            {{23208049, 7979712, 33071466, 8149229, 1758231, 22719437, 30945527,
             31860109, 33606523, 18786461}},
            {{1439939, 17283952, 66028874, 32760649, 4625401, 10647766, 62065063,
             1220117, 30494170, 22113633}},
        },
        {
            {{62071265, 20526136, 64138304, 30492664, 15640973, 26852766,
             40369837, 926049, 65424525, 20220784}},
            {{13908495, 30005160, 30919927, 27280607, 45587000, 7989038,
             9021034, 9078865, 3353509, 4033511}},
            {{37445433, 18440821, 32259990, 33209950, 24295848, 20642309,
             23161162, 8839127, 27485041, 7356032}},
        },
    },
    {
        {
            {{9661008, 705443, 11980065, 28184278, 65480320, 14661172, 60762722,
             2625014, 28431036, 16782598}},
            {{43269631, 25243016, 41163352, 7480957, 49427195, 25200248,
             44562891, 14150564, 15970762, 4099461}},
            {{29262576, 16756590, 26350592, 24760869, 8529670, 22346382,
             13617292, 23617289, 11465738, 8317062}},
        },
        {
            {{41615764, 26591503, 32500199, 24135381, 44070139, 31252209,
             14898636, 3848455, 20969334, 28396916}},
            {{46724414, 19206718, 48772458, 13884721, 34069410, 2842113,
             45498038, 29904543, 11177094, 14989547}},
            {{42612143, 21838415, 16959895, 2278463, 12066309, 10137771,
             13515641, 2581286, 38621356, 9930239}},
        },
        {
            {{49357223, 31456605, 16544299, 20545132, 51194056, 18605350,
             18345766, 20150679, 16291480, 28240394}},
            {{33879670, 2553287, 32678213, 9875984, 8534129, 6889387, 57432090,
             6957616, 4368891, 9788741}},
            {{16660737, 7281060, 56278106, 12911819, 20108584, 25452756,
             45386327, 24941283, 16250551, 22443329}},
        },
        {
            {{47343357, 2390525, 50557833, 14161979, 1905286, 6414907, 4689584,
             10604807, 36918461, 4782746}},
            {{65754325, 14736940, 59741422, 20261545, 7710541, 19398842,
             57127292, 4383044, 22546403, 437323}},
            {{31665558, 21373968, 50922033, 1491338, 48740239, 3294681,
             27343084, 2786261, 36475274, 19457415}},
        },
        {
            {{52641566, 32870716, 33734756, 7448551, 19294360, 14334329,
             47418233, 2355318, 47824193, 27440058}},
            {{15121312, 17758270, 6377019, 27523071, 56310752, 20596586,
             18952176, 15496498, 37728731, 11754227}},
            {{64471568, 20071356, 8488726, 19250536, 12728760, 31931939,
             7141595, 11724556, 22761615, 23420291}},
        },
        {
            {{16918416, 11729663, 49025285, 3022986, 36093132, 20214772,
             38367678, 21327038, 32851221, 11717399}},
            {{11166615, 7338049, 60386341, 4531519, 37640192, 26252376,
             31474878, 3483633, 65915689, 29523600}},
            {{66923210, 9921304, 31456609, 20017994, 55095045, 13348922,
             33142652, 6546660, 47123585, 29606055}},
        },
        {
            {{34648249, 11266711, 55911757, 25655328, 31703693, 3855903,
             58571733, 20721383, 36336829, 18068118}},
            {{49102387, 12709067, 3991746, 27075244, 45617340, 23004006,
             35973516, 17504552, 10928916, 3011958}},
            {{60151107, 17960094, 31696058, 334240, 29576716, 14796075,
             36277808, 20749251, 18008030, 10258577}},
        },
        {
            {{44660220, 15655568, 7018479, 29144429, 36794597, 32352840,
             65255398, 1367119, 25127874, 6671743}},
            {{29701166, 19180498, 56230743, 9279287, 67091296, 13127209,
             21382910, 11042292, 25838796, 4642684}},
            {{46678630, 14955536, 42982517, 8124618, 61739576, 27563961,
             30468146, 19653792, 18423288, 4177476}},
        },
    },
};

static uint8_t negative(signed char b) {
  uint32_t x = b;
  x >>= 31;  // 1: yes; 0: no
  return x;
}

static void table_select(ge_precomp *t, int pos, signed char b) {
  ge_precomp minust;
  uint8_t bnegative = negative(b);
  uint8_t babs = b - ((uint8_t)((-bnegative) & b) << 1);

  ge_precomp_0(t);
  cmov(t, &k25519Precomp[pos][0], equal(babs, 1));
  cmov(t, &k25519Precomp[pos][1], equal(babs, 2));
  cmov(t, &k25519Precomp[pos][2], equal(babs, 3));
  cmov(t, &k25519Precomp[pos][3], equal(babs, 4));
  cmov(t, &k25519Precomp[pos][4], equal(babs, 5));
  cmov(t, &k25519Precomp[pos][5], equal(babs, 6));
  cmov(t, &k25519Precomp[pos][6], equal(babs, 7));
  cmov(t, &k25519Precomp[pos][7], equal(babs, 8));
  fe_copy_ll(&minust.yplusx, &t->yminusx);
  fe_copy_ll(&minust.yminusx, &t->yplusx);

  // NOTE: the input table is canonical, but types don't encode it
  fe tmp;
  fe_carry(&tmp, &t->xy2d);
  fe_neg(&minust.xy2d, &tmp);

  cmov(t, &minust, bnegative);
}

// h = a * B
// where a = a[0]+256*a[1]+...+256^31 a[31]
// B is the Ed25519 base point (x,4/5) with x positive.
//
// Preconditions:
//   a[31] <= 127
void x25519_ge_scalarmult_base(ge_p3 *h, const uint8_t *a) {
  signed char e[64];
  signed char carry;
  ge_p1p1 r;
  ge_p2 s;
  ge_precomp t;
  int i;

  for (i = 0; i < 32; ++i) {
    e[2 * i + 0] = (a[i] >> 0) & 15;
    e[2 * i + 1] = (a[i] >> 4) & 15;
  }
  // each e[i] is between 0 and 15
  // e[63] is between 0 and 7

  carry = 0;
  for (i = 0; i < 63; ++i) {
    e[i] += carry;
    carry = e[i] + 8;
    carry >>= 4;
    e[i] -= carry << 4;
  }
  e[63] += carry;
  // each e[i] is between -8 and 8

  ge_p3_0(h);
  for (i = 1; i < 64; i += 2) {
    table_select(&t, i / 2, e[i]);
    ge_madd(&r, h, &t);
    x25519_ge_p1p1_to_p3(h, &r);
  }

  ge_p3_dbl(&r, h);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p3(h, &r);

  for (i = 0; i < 64; i += 2) {
    table_select(&t, i / 2, e[i]);
    ge_madd(&r, h, &t);
    x25519_ge_p1p1_to_p3(h, &r);
  }
}

#endif

static void cmov_cached(ge_cached *t, ge_cached *u, uint8_t b) {
  fe_cmov(&t->YplusX, &u->YplusX, b);
  fe_cmov(&t->YminusX, &u->YminusX, b);
  fe_cmov(&t->Z, &u->Z, b);
  fe_cmov(&t->T2d, &u->T2d, b);
}

// r = scalar * A.
// where a = a[0]+256*a[1]+...+256^31 a[31].
void x25519_ge_scalarmult(ge_p2 *r, const uint8_t *scalar, const ge_p3 *A) {
  ge_p2 Ai_p2[8];
  ge_cached Ai[16];
  ge_p1p1 t;

  ge_cached_0(&Ai[0]);
  x25519_ge_p3_to_cached(&Ai[1], A);
  ge_p3_to_p2(&Ai_p2[1], A);

  unsigned i;
  for (i = 2; i < 16; i += 2) {
    ge_p2_dbl(&t, &Ai_p2[i / 2]);
    ge_p1p1_to_cached(&Ai[i], &t);
    if (i < 8) {
      x25519_ge_p1p1_to_p2(&Ai_p2[i], &t);
    }
    x25519_ge_add(&t, A, &Ai[i]);
    ge_p1p1_to_cached(&Ai[i + 1], &t);
    if (i < 7) {
      x25519_ge_p1p1_to_p2(&Ai_p2[i + 1], &t);
    }
  }

  ge_p2_0(r);
  ge_p3 u;

  for (i = 0; i < 256; i += 4) {
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p3(&u, &t);

    uint8_t index = scalar[31 - i/8];
    index >>= 4 - (i & 4);
    index &= 0xf;

    unsigned j;
    ge_cached selected;
    ge_cached_0(&selected);
    for (j = 0; j < 16; j++) {
      cmov_cached(&selected, &Ai[j], equal(j, index));
    }

    x25519_ge_add(&t, &u, &selected);
    x25519_ge_p1p1_to_p2(r, &t);
  }
}

static void slide(signed char *r, const uint8_t *a) {
  int i;
  int b;
  int k;

  for (i = 0; i < 256; ++i) {
    r[i] = 1 & (a[i >> 3] >> (i & 7));
  }

  for (i = 0; i < 256; ++i) {
    if (r[i]) {
      for (b = 1; b <= 6 && i + b < 256; ++b) {
        if (r[i + b]) {
          if (r[i] + (r[i + b] << b) <= 15) {
            r[i] += r[i + b] << b;
            r[i + b] = 0;
          } else if (r[i] - (r[i + b] << b) >= -15) {
            r[i] -= r[i + b] << b;
            for (k = i + b; k < 256; ++k) {
              if (!r[k]) {
                r[k] = 1;
                break;
              }
              r[k] = 0;
            }
          } else {
            break;
          }
        }
      }
    }
  }
}

static const ge_precomp Bi[8] = {
    {
        {{25967493, 19198397, 29566455, 3660896, 54414519, 4014786, 27544626,
         21800161, 61029707, 2047604}},
        {{54563134, 934261, 64385954, 3049989, 66381436, 9406985, 12720692,
         5043384, 19500929, 18085054}},
        {{58370664, 4489569, 9688441, 18769238, 10184608, 21191052, 29287918,
         11864899, 42594502, 29115885}},
    },
    {
        {{15636272, 23865875, 24204772, 25642034, 616976, 16869170, 27787599,
         18782243, 28944399, 32004408}},
        {{16568933, 4717097, 55552716, 32452109, 15682895, 21747389, 16354576,
         21778470, 7689661, 11199574}},
        {{30464137, 27578307, 55329429, 17883566, 23220364, 15915852, 7512774,
         10017326, 49359771, 23634074}},
    },
    {
        {{10861363, 11473154, 27284546, 1981175, 37044515, 12577860, 32867885,
         14515107, 51670560, 10819379}},
        {{4708026, 6336745, 20377586, 9066809, 55836755, 6594695, 41455196,
         12483687, 54440373, 5581305}},
        {{19563141, 16186464, 37722007, 4097518, 10237984, 29206317, 28542349,
         13850243, 43430843, 17738489}},
    },
    {
        {{5153727, 9909285, 1723747, 30776558, 30523604, 5516873, 19480852,
         5230134, 43156425, 18378665}},
        {{36839857, 30090922, 7665485, 10083793, 28475525, 1649722, 20654025,
         16520125, 30598449, 7715701}},
        {{28881826, 14381568, 9657904, 3680757, 46927229, 7843315, 35708204,
         1370707, 29794553, 32145132}},
    },
    {
        {{44589871, 26862249, 14201701, 24808930, 43598457, 8844725, 18474211,
         32192982, 54046167, 13821876}},
        {{60653668, 25714560, 3374701, 28813570, 40010246, 22982724, 31655027,
         26342105, 18853321, 19333481}},
        {{4566811, 20590564, 38133974, 21313742, 59506191, 30723862, 58594505,
         23123294, 2207752, 30344648}},
    },
    {
        {{41954014, 29368610, 29681143, 7868801, 60254203, 24130566, 54671499,
         32891431, 35997400, 17421995}},
        {{25576264, 30851218, 7349803, 21739588, 16472781, 9300885, 3844789,
         15725684, 171356, 6466918}},
        {{23103977, 13316479, 9739013, 17404951, 817874, 18515490, 8965338,
         19466374, 36393951, 16193876}},
    },
    {
        {{33587053, 3180712, 64714734, 14003686, 50205390, 17283591, 17238397,
         4729455, 49034351, 9256799}},
        {{41926547, 29380300, 32336397, 5036987, 45872047, 11360616, 22616405,
         9761698, 47281666, 630304}},
        {{53388152, 2639452, 42871404, 26147950, 9494426, 27780403, 60554312,
         17593437, 64659607, 19263131}},
    },
    {
        {{63957664, 28508356, 9282713, 6866145, 35201802, 32691408, 48168288,
         15033783, 25105118, 25659556}},
        {{42782475, 15950225, 35307649, 18961608, 55446126, 28463506,
         1573891, 30928545, 2198789, 17749813}},
        {{64009494, 10324966, 64867251, 7453182, 61661885, 30818928, 53296841,
         17317989, 34647629, 21263748}},
    },
};

// r = a * A + b * B
// where a = a[0]+256*a[1]+...+256^31 a[31].
// and b = b[0]+256*b[1]+...+256^31 b[31].
// B is the Ed25519 base point (x,4/5) with x positive.
static void ge_double_scalarmult_vartime(ge_p2 *r, const uint8_t *a,
                                         const ge_p3 *A, const uint8_t *b) {
  signed char aslide[256];
  signed char bslide[256];
  ge_cached Ai[8];  // A,3A,5A,7A,9A,11A,13A,15A
  ge_p1p1 t;
  ge_p3 u;
  ge_p3 A2;
  int i;

  slide(aslide, a);
  slide(bslide, b);

  x25519_ge_p3_to_cached(&Ai[0], A);
  ge_p3_dbl(&t, A);
  x25519_ge_p1p1_to_p3(&A2, &t);
  x25519_ge_add(&t, &A2, &Ai[0]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[1], &u);
  x25519_ge_add(&t, &A2, &Ai[1]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[2], &u);
  x25519_ge_add(&t, &A2, &Ai[2]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[3], &u);
  x25519_ge_add(&t, &A2, &Ai[3]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[4], &u);
  x25519_ge_add(&t, &A2, &Ai[4]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[5], &u);
  x25519_ge_add(&t, &A2, &Ai[5]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[6], &u);
  x25519_ge_add(&t, &A2, &Ai[6]);
  x25519_ge_p1p1_to_p3(&u, &t);
  x25519_ge_p3_to_cached(&Ai[7], &u);

  ge_p2_0(r);

  for (i = 255; i >= 0; --i) {
    if (aslide[i] || bslide[i]) {
      break;
    }
  }

  for (; i >= 0; --i) {
    ge_p2_dbl(&t, r);

    if (aslide[i] > 0) {
      x25519_ge_p1p1_to_p3(&u, &t);
      x25519_ge_add(&t, &u, &Ai[aslide[i] / 2]);
    } else if (aslide[i] < 0) {
      x25519_ge_p1p1_to_p3(&u, &t);
      x25519_ge_sub(&t, &u, &Ai[(-aslide[i]) / 2]);
    }

    if (bslide[i] > 0) {
      x25519_ge_p1p1_to_p3(&u, &t);
      ge_madd(&t, &u, &Bi[bslide[i] / 2]);
    } else if (bslide[i] < 0) {
      x25519_ge_p1p1_to_p3(&u, &t);
      ge_msub(&t, &u, &Bi[(-bslide[i]) / 2]);
    }

    x25519_ge_p1p1_to_p2(r, &t);
  }
}

// The set of scalars is \Z/l
// where l = 2^252 + 27742317777372353535851937790883648493.

// Input:
//   s[0]+256*s[1]+...+256^63*s[63] = s
//
// Output:
//   s[0]+256*s[1]+...+256^31*s[31] = s mod l
//   where l = 2^252 + 27742317777372353535851937790883648493.
//   Overwrites s in place.
void x25519_sc_reduce(uint8_t *s) {
  int64_t s0 = 2097151 & load_3(s);
  int64_t s1 = 2097151 & (load_4(s + 2) >> 5);
  int64_t s2 = 2097151 & (load_3(s + 5) >> 2);
  int64_t s3 = 2097151 & (load_4(s + 7) >> 7);
  int64_t s4 = 2097151 & (load_4(s + 10) >> 4);
  int64_t s5 = 2097151 & (load_3(s + 13) >> 1);
  int64_t s6 = 2097151 & (load_4(s + 15) >> 6);
  int64_t s7 = 2097151 & (load_3(s + 18) >> 3);
  int64_t s8 = 2097151 & load_3(s + 21);
  int64_t s9 = 2097151 & (load_4(s + 23) >> 5);
  int64_t s10 = 2097151 & (load_3(s + 26) >> 2);
  int64_t s11 = 2097151 & (load_4(s + 28) >> 7);
  int64_t s12 = 2097151 & (load_4(s + 31) >> 4);
  int64_t s13 = 2097151 & (load_3(s + 34) >> 1);
  int64_t s14 = 2097151 & (load_4(s + 36) >> 6);
  int64_t s15 = 2097151 & (load_3(s + 39) >> 3);
  int64_t s16 = 2097151 & load_3(s + 42);
  int64_t s17 = 2097151 & (load_4(s + 44) >> 5);
  int64_t s18 = 2097151 & (load_3(s + 47) >> 2);
  int64_t s19 = 2097151 & (load_4(s + 49) >> 7);
  int64_t s20 = 2097151 & (load_4(s + 52) >> 4);
  int64_t s21 = 2097151 & (load_3(s + 55) >> 1);
  int64_t s22 = 2097151 & (load_4(s + 57) >> 6);
  int64_t s23 = (load_4(s + 60) >> 3);
  int64_t carry0;
  int64_t carry1;
  int64_t carry2;
  int64_t carry3;
  int64_t carry4;
  int64_t carry5;
  int64_t carry6;
  int64_t carry7;
  int64_t carry8;
  int64_t carry9;
  int64_t carry10;
  int64_t carry11;
  int64_t carry12;
  int64_t carry13;
  int64_t carry14;
  int64_t carry15;
  int64_t carry16;

  s11 += s23 * 666643;
  s12 += s23 * 470296;
  s13 += s23 * 654183;
  s14 -= s23 * 997805;
  s15 += s23 * 136657;
  s16 -= s23 * 683901;
  s23 = 0;

  s10 += s22 * 666643;
  s11 += s22 * 470296;
  s12 += s22 * 654183;
  s13 -= s22 * 997805;
  s14 += s22 * 136657;
  s15 -= s22 * 683901;
  s22 = 0;

  s9 += s21 * 666643;
  s10 += s21 * 470296;
  s11 += s21 * 654183;
  s12 -= s21 * 997805;
  s13 += s21 * 136657;
  s14 -= s21 * 683901;
  s21 = 0;

  s8 += s20 * 666643;
  s9 += s20 * 470296;
  s10 += s20 * 654183;
  s11 -= s20 * 997805;
  s12 += s20 * 136657;
  s13 -= s20 * 683901;
  s20 = 0;

  s7 += s19 * 666643;
  s8 += s19 * 470296;
  s9 += s19 * 654183;
  s10 -= s19 * 997805;
  s11 += s19 * 136657;
  s12 -= s19 * 683901;
  s19 = 0;

  s6 += s18 * 666643;
  s7 += s18 * 470296;
  s8 += s18 * 654183;
  s9 -= s18 * 997805;
  s10 += s18 * 136657;
  s11 -= s18 * 683901;
  s18 = 0;

  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;
  carry12 = (s12 + (1 << 20)) >> 21;
  s13 += carry12;
  s12 -= carry12 << 21;
  carry14 = (s14 + (1 << 20)) >> 21;
  s15 += carry14;
  s14 -= carry14 << 21;
  carry16 = (s16 + (1 << 20)) >> 21;
  s17 += carry16;
  s16 -= carry16 << 21;

  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;
  carry13 = (s13 + (1 << 20)) >> 21;
  s14 += carry13;
  s13 -= carry13 << 21;
  carry15 = (s15 + (1 << 20)) >> 21;
  s16 += carry15;
  s15 -= carry15 << 21;

  s5 += s17 * 666643;
  s6 += s17 * 470296;
  s7 += s17 * 654183;
  s8 -= s17 * 997805;
  s9 += s17 * 136657;
  s10 -= s17 * 683901;
  s17 = 0;

  s4 += s16 * 666643;
  s5 += s16 * 470296;
  s6 += s16 * 654183;
  s7 -= s16 * 997805;
  s8 += s16 * 136657;
  s9 -= s16 * 683901;
  s16 = 0;

  s3 += s15 * 666643;
  s4 += s15 * 470296;
  s5 += s15 * 654183;
  s6 -= s15 * 997805;
  s7 += s15 * 136657;
  s8 -= s15 * 683901;
  s15 = 0;

  s2 += s14 * 666643;
  s3 += s14 * 470296;
  s4 += s14 * 654183;
  s5 -= s14 * 997805;
  s6 += s14 * 136657;
  s7 -= s14 * 683901;
  s14 = 0;

  s1 += s13 * 666643;
  s2 += s13 * 470296;
  s3 += s13 * 654183;
  s4 -= s13 * 997805;
  s5 += s13 * 136657;
  s6 -= s13 * 683901;
  s13 = 0;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = (s0 + (1 << 20)) >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry2 = (s2 + (1 << 20)) >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry4 = (s4 + (1 << 20)) >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;

  carry1 = (s1 + (1 << 20)) >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry3 = (s3 + (1 << 20)) >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry5 = (s5 + (1 << 20)) >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;
  carry11 = s11 >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;

  s[0] = s0 >> 0;
  s[1] = s0 >> 8;
  s[2] = (s0 >> 16) | (s1 << 5);
  s[3] = s1 >> 3;
  s[4] = s1 >> 11;
  s[5] = (s1 >> 19) | (s2 << 2);
  s[6] = s2 >> 6;
  s[7] = (s2 >> 14) | (s3 << 7);
  s[8] = s3 >> 1;
  s[9] = s3 >> 9;
  s[10] = (s3 >> 17) | (s4 << 4);
  s[11] = s4 >> 4;
  s[12] = s4 >> 12;
  s[13] = (s4 >> 20) | (s5 << 1);
  s[14] = s5 >> 7;
  s[15] = (s5 >> 15) | (s6 << 6);
  s[16] = s6 >> 2;
  s[17] = s6 >> 10;
  s[18] = (s6 >> 18) | (s7 << 3);
  s[19] = s7 >> 5;
  s[20] = s7 >> 13;
  s[21] = s8 >> 0;
  s[22] = s8 >> 8;
  s[23] = (s8 >> 16) | (s9 << 5);
  s[24] = s9 >> 3;
  s[25] = s9 >> 11;
  s[26] = (s9 >> 19) | (s10 << 2);
  s[27] = s10 >> 6;
  s[28] = (s10 >> 14) | (s11 << 7);
  s[29] = s11 >> 1;
  s[30] = s11 >> 9;
  s[31] = s11 >> 17;
}

// Input:
//   a[0]+256*a[1]+...+256^31*a[31] = a
//   b[0]+256*b[1]+...+256^31*b[31] = b
//   c[0]+256*c[1]+...+256^31*c[31] = c
//
// Output:
//   s[0]+256*s[1]+...+256^31*s[31] = (ab+c) mod l
//   where l = 2^252 + 27742317777372353535851937790883648493.
static void sc_muladd(uint8_t *s, const uint8_t *a, const uint8_t *b,
                      const uint8_t *c) {
  int64_t a0 = 2097151 & load_3(a);
  int64_t a1 = 2097151 & (load_4(a + 2) >> 5);
  int64_t a2 = 2097151 & (load_3(a + 5) >> 2);
  int64_t a3 = 2097151 & (load_4(a + 7) >> 7);
  int64_t a4 = 2097151 & (load_4(a + 10) >> 4);
  int64_t a5 = 2097151 & (load_3(a + 13) >> 1);
  int64_t a6 = 2097151 & (load_4(a + 15) >> 6);
  int64_t a7 = 2097151 & (load_3(a + 18) >> 3);
  int64_t a8 = 2097151 & load_3(a + 21);
  int64_t a9 = 2097151 & (load_4(a + 23) >> 5);
  int64_t a10 = 2097151 & (load_3(a + 26) >> 2);
  int64_t a11 = (load_4(a + 28) >> 7);
  int64_t b0 = 2097151 & load_3(b);
  int64_t b1 = 2097151 & (load_4(b + 2) >> 5);
  int64_t b2 = 2097151 & (load_3(b + 5) >> 2);
  int64_t b3 = 2097151 & (load_4(b + 7) >> 7);
  int64_t b4 = 2097151 & (load_4(b + 10) >> 4);
  int64_t b5 = 2097151 & (load_3(b + 13) >> 1);
  int64_t b6 = 2097151 & (load_4(b + 15) >> 6);
  int64_t b7 = 2097151 & (load_3(b + 18) >> 3);
  int64_t b8 = 2097151 & load_3(b + 21);
  int64_t b9 = 2097151 & (load_4(b + 23) >> 5);
  int64_t b10 = 2097151 & (load_3(b + 26) >> 2);
  int64_t b11 = (load_4(b + 28) >> 7);
  int64_t c0 = 2097151 & load_3(c);
  int64_t c1 = 2097151 & (load_4(c + 2) >> 5);
  int64_t c2 = 2097151 & (load_3(c + 5) >> 2);
  int64_t c3 = 2097151 & (load_4(c + 7) >> 7);
  int64_t c4 = 2097151 & (load_4(c + 10) >> 4);
  int64_t c5 = 2097151 & (load_3(c + 13) >> 1);
  int64_t c6 = 2097151 & (load_4(c + 15) >> 6);
  int64_t c7 = 2097151 & (load_3(c + 18) >> 3);
  int64_t c8 = 2097151 & load_3(c + 21);
  int64_t c9 = 2097151 & (load_4(c + 23) >> 5);
  int64_t c10 = 2097151 & (load_3(c + 26) >> 2);
  int64_t c11 = (load_4(c + 28) >> 7);
  int64_t s0;
  int64_t s1;
  int64_t s2;
  int64_t s3;
  int64_t s4;
  int64_t s5;
  int64_t s6;
  int64_t s7;
  int64_t s8;
  int64_t s9;
  int64_t s10;
  int64_t s11;
  int64_t s12;
  int64_t s13;
  int64_t s14;
  int64_t s15;
  int64_t s16;
  int64_t s17;
  int64_t s18;
  int64_t s19;
  int64_t s20;
  int64_t s21;
  int64_t s22;
  int64_t s23;
  int64_t carry0;
  int64_t carry1;
  int64_t carry2;
  int64_t carry3;
  int64_t carry4;
  int64_t carry5;
  int64_t carry6;
  int64_t carry7;
  int64_t carry8;
  int64_t carry9;
  int64_t carry10;
  int64_t carry11;
  int64_t carry12;
  int64_t carry13;
  int64_t carry14;
  int64_t carry15;
  int64_t carry16;
  int64_t carry17;
  int64_t carry18;
  int64_t carry19;
  int64_t carry20;
  int64_t carry21;
  int64_t carry22;

  s0 = c0 + a0 * b0;
  s1 = c1 + a0 * b1 + a1 * b0;
  s2 = c2 + a0 * b2 + a1 * b1 + a2 * b0;
  s3 = c3 + a0 * b3 + a1 * b2 + a2 * b1 + a3 * b0;
  s4 = c4 + a0 * b4 + a1 * b3 + a2 * b2 + a3 * b1 + a4 * b0;
  s5 = c5 + a0 * b5 + a1 * b4 + a2 * b3 + a3 * b2 + a4 * b1 + a5 * b0;
  s6 = c6 + a0 * b6 + a1 * b5 + a2 * b4 + a3 * b3 + a4 * b2 + a5 * b1 + a6 * b0;
  s7 = c7 + a0 * b7 + a1 * b6 + a2 * b5 + a3 * b4 + a4 * b3 + a5 * b2 +
       a6 * b1 + a7 * b0;
  s8 = c8 + a0 * b8 + a1 * b7 + a2 * b6 + a3 * b5 + a4 * b4 + a5 * b3 +
       a6 * b2 + a7 * b1 + a8 * b0;
  s9 = c9 + a0 * b9 + a1 * b8 + a2 * b7 + a3 * b6 + a4 * b5 + a5 * b4 +
       a6 * b3 + a7 * b2 + a8 * b1 + a9 * b0;
  s10 = c10 + a0 * b10 + a1 * b9 + a2 * b8 + a3 * b7 + a4 * b6 + a5 * b5 +
        a6 * b4 + a7 * b3 + a8 * b2 + a9 * b1 + a10 * b0;
  s11 = c11 + a0 * b11 + a1 * b10 + a2 * b9 + a3 * b8 + a4 * b7 + a5 * b6 +
        a6 * b5 + a7 * b4 + a8 * b3 + a9 * b2 + a10 * b1 + a11 * b0;
  s12 = a1 * b11 + a2 * b10 + a3 * b9 + a4 * b8 + a5 * b7 + a6 * b6 + a7 * b5 +
        a8 * b4 + a9 * b3 + a10 * b2 + a11 * b1;
  s13 = a2 * b11 + a3 * b10 + a4 * b9 + a5 * b8 + a6 * b7 + a7 * b6 + a8 * b5 +
        a9 * b4 + a10 * b3 + a11 * b2;
  s14 = a3 * b11 + a4 * b10 + a5 * b9 + a6 * b8 + a7 * b7 + a8 * b6 + a9 * b5 +
        a10 * b4 + a11 * b3;
  s15 = a4 * b11 + a5 * b10 + a6 * b9 + a7 * b8 + a8 * b7 + a9 * b6 + a10 * b5 +
        a11 * b4;
  s16 = a5 * b11 + a6 * b10 + a7 * b9 + a8 * b8 + a9 * b7 + a10 * b6 + a11 * b5;
  s17 = a6 * b11 + a7 * b10 + a8 * b9 + a9 * b8 + a10 * b7 + a11 * b6;
  s18 = a7 * b11 + a8 * b10 + a9 * b9 + a10 * b8 + a11 * b7;
  s19 = a8 * b11 + a9 * b10 + a10 * b9 + a11 * b8;
  s20 = a9 * b11 + a10 * b10 + a11 * b9;
  s21 = a10 * b11 + a11 * b10;
  s22 = a11 * b11;
  s23 = 0;

  carry0 = (s0 + (1 << 20)) >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry2 = (s2 + (1 << 20)) >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry4 = (s4 + (1 << 20)) >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;
  carry12 = (s12 + (1 << 20)) >> 21;
  s13 += carry12;
  s12 -= carry12 << 21;
  carry14 = (s14 + (1 << 20)) >> 21;
  s15 += carry14;
  s14 -= carry14 << 21;
  carry16 = (s16 + (1 << 20)) >> 21;
  s17 += carry16;
  s16 -= carry16 << 21;
  carry18 = (s18 + (1 << 20)) >> 21;
  s19 += carry18;
  s18 -= carry18 << 21;
  carry20 = (s20 + (1 << 20)) >> 21;
  s21 += carry20;
  s20 -= carry20 << 21;
  carry22 = (s22 + (1 << 20)) >> 21;
  s23 += carry22;
  s22 -= carry22 << 21;

  carry1 = (s1 + (1 << 20)) >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry3 = (s3 + (1 << 20)) >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry5 = (s5 + (1 << 20)) >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;
  carry13 = (s13 + (1 << 20)) >> 21;
  s14 += carry13;
  s13 -= carry13 << 21;
  carry15 = (s15 + (1 << 20)) >> 21;
  s16 += carry15;
  s15 -= carry15 << 21;
  carry17 = (s17 + (1 << 20)) >> 21;
  s18 += carry17;
  s17 -= carry17 << 21;
  carry19 = (s19 + (1 << 20)) >> 21;
  s20 += carry19;
  s19 -= carry19 << 21;
  carry21 = (s21 + (1 << 20)) >> 21;
  s22 += carry21;
  s21 -= carry21 << 21;

  s11 += s23 * 666643;
  s12 += s23 * 470296;
  s13 += s23 * 654183;
  s14 -= s23 * 997805;
  s15 += s23 * 136657;
  s16 -= s23 * 683901;
  s23 = 0;

  s10 += s22 * 666643;
  s11 += s22 * 470296;
  s12 += s22 * 654183;
  s13 -= s22 * 997805;
  s14 += s22 * 136657;
  s15 -= s22 * 683901;
  s22 = 0;

  s9 += s21 * 666643;
  s10 += s21 * 470296;
  s11 += s21 * 654183;
  s12 -= s21 * 997805;
  s13 += s21 * 136657;
  s14 -= s21 * 683901;
  s21 = 0;

  s8 += s20 * 666643;
  s9 += s20 * 470296;
  s10 += s20 * 654183;
  s11 -= s20 * 997805;
  s12 += s20 * 136657;
  s13 -= s20 * 683901;
  s20 = 0;

  s7 += s19 * 666643;
  s8 += s19 * 470296;
  s9 += s19 * 654183;
  s10 -= s19 * 997805;
  s11 += s19 * 136657;
  s12 -= s19 * 683901;
  s19 = 0;

  s6 += s18 * 666643;
  s7 += s18 * 470296;
  s8 += s18 * 654183;
  s9 -= s18 * 997805;
  s10 += s18 * 136657;
  s11 -= s18 * 683901;
  s18 = 0;

  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;
  carry12 = (s12 + (1 << 20)) >> 21;
  s13 += carry12;
  s12 -= carry12 << 21;
  carry14 = (s14 + (1 << 20)) >> 21;
  s15 += carry14;
  s14 -= carry14 << 21;
  carry16 = (s16 + (1 << 20)) >> 21;
  s17 += carry16;
  s16 -= carry16 << 21;

  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;
  carry13 = (s13 + (1 << 20)) >> 21;
  s14 += carry13;
  s13 -= carry13 << 21;
  carry15 = (s15 + (1 << 20)) >> 21;
  s16 += carry15;
  s15 -= carry15 << 21;

  s5 += s17 * 666643;
  s6 += s17 * 470296;
  s7 += s17 * 654183;
  s8 -= s17 * 997805;
  s9 += s17 * 136657;
  s10 -= s17 * 683901;
  s17 = 0;

  s4 += s16 * 666643;
  s5 += s16 * 470296;
  s6 += s16 * 654183;
  s7 -= s16 * 997805;
  s8 += s16 * 136657;
  s9 -= s16 * 683901;
  s16 = 0;

  s3 += s15 * 666643;
  s4 += s15 * 470296;
  s5 += s15 * 654183;
  s6 -= s15 * 997805;
  s7 += s15 * 136657;
  s8 -= s15 * 683901;
  s15 = 0;

  s2 += s14 * 666643;
  s3 += s14 * 470296;
  s4 += s14 * 654183;
  s5 -= s14 * 997805;
  s6 += s14 * 136657;
  s7 -= s14 * 683901;
  s14 = 0;

  s1 += s13 * 666643;
  s2 += s13 * 470296;
  s3 += s13 * 654183;
  s4 -= s13 * 997805;
  s5 += s13 * 136657;
  s6 -= s13 * 683901;
  s13 = 0;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = (s0 + (1 << 20)) >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry2 = (s2 + (1 << 20)) >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry4 = (s4 + (1 << 20)) >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;

  carry1 = (s1 + (1 << 20)) >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry3 = (s3 + (1 << 20)) >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry5 = (s5 + (1 << 20)) >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;
  carry11 = s11 >> 21;
  s12 += carry11;
  s11 -= carry11 << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= carry0 << 21;
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= carry1 << 21;
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= carry2 << 21;
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= carry3 << 21;
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= carry4 << 21;
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= carry5 << 21;
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= carry6 << 21;
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= carry7 << 21;
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= carry8 << 21;
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= carry9 << 21;
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= carry10 << 21;

  s[0] = s0 >> 0;
  s[1] = s0 >> 8;
  s[2] = (s0 >> 16) | (s1 << 5);
  s[3] = s1 >> 3;
  s[4] = s1 >> 11;
  s[5] = (s1 >> 19) | (s2 << 2);
  s[6] = s2 >> 6;
  s[7] = (s2 >> 14) | (s3 << 7);
  s[8] = s3 >> 1;
  s[9] = s3 >> 9;
  s[10] = (s3 >> 17) | (s4 << 4);
  s[11] = s4 >> 4;
  s[12] = s4 >> 12;
  s[13] = (s4 >> 20) | (s5 << 1);
  s[14] = s5 >> 7;
  s[15] = (s5 >> 15) | (s6 << 6);
  s[16] = s6 >> 2;
  s[17] = s6 >> 10;
  s[18] = (s6 >> 18) | (s7 << 3);
  s[19] = s7 >> 5;
  s[20] = s7 >> 13;
  s[21] = s8 >> 0;
  s[22] = s8 >> 8;
  s[23] = (s8 >> 16) | (s9 << 5);
  s[24] = s9 >> 3;
  s[25] = s9 >> 11;
  s[26] = (s9 >> 19) | (s10 << 2);
  s[27] = s10 >> 6;
  s[28] = (s10 >> 14) | (s11 << 7);
  s[29] = s11 >> 1;
  s[30] = s11 >> 9;
  s[31] = s11 >> 17;
}

void ED25519_keypair(uint8_t out_public_key[32], uint8_t out_private_key[64]) {
  uint8_t seed[32];
  RAND_bytes(seed, 32);
  ED25519_keypair_from_seed(out_public_key, out_private_key, seed);
}

int ED25519_sign(uint8_t *out_sig, const uint8_t *message, size_t message_len,
                 const uint8_t private_key[64]) {
  uint8_t az[SHA512_DIGEST_LENGTH];
  SHA512(private_key, 32, az);

  az[0] &= 248;
  az[31] &= 63;
  az[31] |= 64;

  SHA512_CTX hash_ctx;
  SHA512_Init(&hash_ctx);
  SHA512_Update(&hash_ctx, az + 32, 32);
  SHA512_Update(&hash_ctx, message, message_len);
  uint8_t nonce[SHA512_DIGEST_LENGTH];
  SHA512_Final(nonce, &hash_ctx);

  x25519_sc_reduce(nonce);
  ge_p3 R;
  x25519_ge_scalarmult_base(&R, nonce);
  ge_p3_tobytes(out_sig, &R);

  SHA512_Init(&hash_ctx);
  SHA512_Update(&hash_ctx, out_sig, 32);
  SHA512_Update(&hash_ctx, private_key + 32, 32);
  SHA512_Update(&hash_ctx, message, message_len);
  uint8_t hram[SHA512_DIGEST_LENGTH];
  SHA512_Final(hram, &hash_ctx);

  x25519_sc_reduce(hram);
  sc_muladd(out_sig + 32, hram, az, nonce);

  return 1;
}

int ED25519_verify(const uint8_t *message, size_t message_len,
                   const uint8_t signature[64], const uint8_t public_key[32]) {
  ge_p3 A;
  if ((signature[63] & 224) != 0 ||
      x25519_ge_frombytes_vartime(&A, public_key) != 0) {
    return 0;
  }

  fe_loose t;
  fe_neg(&t, &A.X);
  fe_carry(&A.X, &t);
  fe_neg(&t, &A.T);
  fe_carry(&A.T, &t);

  uint8_t pkcopy[32];
  OPENSSL_memcpy(pkcopy, public_key, 32);
  uint8_t rcopy[32];
  OPENSSL_memcpy(rcopy, signature, 32);
  uint8_t scopy[32];
  OPENSSL_memcpy(scopy, signature + 32, 32);

  SHA512_CTX hash_ctx;
  SHA512_Init(&hash_ctx);
  SHA512_Update(&hash_ctx, signature, 32);
  SHA512_Update(&hash_ctx, public_key, 32);
  SHA512_Update(&hash_ctx, message, message_len);
  uint8_t h[SHA512_DIGEST_LENGTH];
  SHA512_Final(h, &hash_ctx);

  x25519_sc_reduce(h);

  ge_p2 R;
  ge_double_scalarmult_vartime(&R, h, &A, scopy);

  uint8_t rcheck[32];
  x25519_ge_tobytes(rcheck, &R);

  return CRYPTO_memcmp(rcheck, rcopy, sizeof(rcheck)) == 0;
}

void ED25519_keypair_from_seed(uint8_t out_public_key[32],
                               uint8_t out_private_key[64],
                               const uint8_t seed[32]) {
  uint8_t az[SHA512_DIGEST_LENGTH];
  SHA512(seed, 32, az);

  az[0] &= 248;
  az[31] &= 63;
  az[31] |= 64;

  ge_p3 A;
  x25519_ge_scalarmult_base(&A, az);
  ge_p3_tobytes(out_public_key, &A);

  OPENSSL_memcpy(out_private_key, seed, 32);
  OPENSSL_memcpy(out_private_key + 32, out_public_key, 32);
}


#if defined(BORINGSSL_X25519_X86_64)

static void x25519_scalar_mult(uint8_t out[32], const uint8_t scalar[32],
                               const uint8_t point[32]) {
  x25519_x86_64(out, scalar, point);
}

#else

// Replace (f,g) with (g,f) if b == 1;
// replace (f,g) with (f,g) if b == 0.
//
// Preconditions: b in {0,1}.
static void fe_cswap(fe *f, fe *g, unsigned int b) {
  b = 0-b;
  unsigned i;
  for (i = 0; i < 10; i++) {
    uint32_t x = f->v[i] ^ g->v[i];
    x &= b;
    f->v[i] ^= x;
    g->v[i] ^= x;
  }
}

// NOTE: based on fiat-crypto fe_mul, edited for in2=121666, 0, 0..
static void fe_mul_121666_impl(uint32_t out[10], const uint32_t in1[10]) {
  assert_fe_loose(in1);
  { const uint32_t x20 = in1[9];
  { const uint32_t x21 = in1[8];
  { const uint32_t x19 = in1[7];
  { const uint32_t x17 = in1[6];
  { const uint32_t x15 = in1[5];
  { const uint32_t x13 = in1[4];
  { const uint32_t x11 = in1[3];
  { const uint32_t x9 = in1[2];
  { const uint32_t x7 = in1[1];
  { const uint32_t x5 = in1[0];
  { const uint32_t x38 = 0;
  { const uint32_t x39 = 0;
  { const uint32_t x37 = 0;
  { const uint32_t x35 = 0;
  { const uint32_t x33 = 0;
  { const uint32_t x31 = 0;
  { const uint32_t x29 = 0;
  { const uint32_t x27 = 0;
  { const uint32_t x25 = 0;
  { const uint32_t x23 = 121666;
  { uint64_t x40 = ((uint64_t)x23 * x5);
  { uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
  { uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
  { uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
  { uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
  { uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
  { uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
  { uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
  { uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
  { uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
  { uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
  { uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
  { uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
  { uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
  { uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
  { uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
  { uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
  { uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
  { uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
  { uint64_t x59 = (x48 + (x58 << 0x4));
  { uint64_t x60 = (x59 + (x58 << 0x1));
  { uint64_t x61 = (x60 + x58);
  { uint64_t x62 = (x47 + (x57 << 0x4));
  { uint64_t x63 = (x62 + (x57 << 0x1));
  { uint64_t x64 = (x63 + x57);
  { uint64_t x65 = (x46 + (x56 << 0x4));
  { uint64_t x66 = (x65 + (x56 << 0x1));
  { uint64_t x67 = (x66 + x56);
  { uint64_t x68 = (x45 + (x55 << 0x4));
  { uint64_t x69 = (x68 + (x55 << 0x1));
  { uint64_t x70 = (x69 + x55);
  { uint64_t x71 = (x44 + (x54 << 0x4));
  { uint64_t x72 = (x71 + (x54 << 0x1));
  { uint64_t x73 = (x72 + x54);
  { uint64_t x74 = (x43 + (x53 << 0x4));
  { uint64_t x75 = (x74 + (x53 << 0x1));
  { uint64_t x76 = (x75 + x53);
  { uint64_t x77 = (x42 + (x52 << 0x4));
  { uint64_t x78 = (x77 + (x52 << 0x1));
  { uint64_t x79 = (x78 + x52);
  { uint64_t x80 = (x41 + (x51 << 0x4));
  { uint64_t x81 = (x80 + (x51 << 0x1));
  { uint64_t x82 = (x81 + x51);
  { uint64_t x83 = (x40 + (x50 << 0x4));
  { uint64_t x84 = (x83 + (x50 << 0x1));
  { uint64_t x85 = (x84 + x50);
  { uint64_t x86 = (x85 >> 0x1a);
  { uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
  { uint64_t x88 = (x86 + x82);
  { uint64_t x89 = (x88 >> 0x19);
  { uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
  { uint64_t x91 = (x89 + x79);
  { uint64_t x92 = (x91 >> 0x1a);
  { uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
  { uint64_t x94 = (x92 + x76);
  { uint64_t x95 = (x94 >> 0x19);
  { uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
  { uint64_t x97 = (x95 + x73);
  { uint64_t x98 = (x97 >> 0x1a);
  { uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
  { uint64_t x100 = (x98 + x70);
  { uint64_t x101 = (x100 >> 0x19);
  { uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
  { uint64_t x103 = (x101 + x67);
  { uint64_t x104 = (x103 >> 0x1a);
  { uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
  { uint64_t x106 = (x104 + x64);
  { uint64_t x107 = (x106 >> 0x19);
  { uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
  { uint64_t x109 = (x107 + x61);
  { uint64_t x110 = (x109 >> 0x1a);
  { uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
  { uint64_t x112 = (x110 + x49);
  { uint64_t x113 = (x112 >> 0x19);
  { uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
  { uint64_t x115 = (x87 + (0x13 * x113));
  { uint32_t x116 = (uint32_t) (x115 >> 0x1a);
  { uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
  { uint32_t x118 = (x116 + x90);
  { uint32_t x119 = (x118 >> 0x19);
  { uint32_t x120 = (x118 & 0x1ffffff);
  out[0] = x117;
  out[1] = x120;
  out[2] = (x119 + x93);
  out[3] = x96;
  out[4] = x99;
  out[5] = x102;
  out[6] = x105;
  out[7] = x108;
  out[8] = x111;
  out[9] = x114;
  }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
  assert_fe(out);
}

static void fe_mul121666(fe *h, const fe_loose *f) {
  assert_fe_loose(f->v);
  fe_mul_121666_impl(h->v, f->v);
  assert_fe(h->v);
}

static void x25519_scalar_mult_generic(uint8_t out[32],
                                       const uint8_t scalar[32],
                                       const uint8_t point[32]) {
  fe x1, x2, z2, x3, z3, tmp0, tmp1;
  fe_loose x2l, z2l, x3l, tmp0l, tmp1l;

  uint8_t e[32];
  OPENSSL_memcpy(e, scalar, 32);
  e[0] &= 248;
  e[31] &= 127;
  e[31] |= 64;
  fe_frombytes(&x1, point);
  fe_1(&x2);
  fe_0(&z2);
  fe_copy(&x3, &x1);
  fe_1(&z3);

  unsigned swap = 0;
  int pos;
  for (pos = 254; pos >= 0; --pos) {
    unsigned b = 1 & (e[pos / 8] >> (pos & 7));
    swap ^= b;
    fe_cswap(&x2, &x3, swap);
    fe_cswap(&z2, &z3, swap);
    swap = b;
    fe_sub(&tmp0l, &x3, &z3);
    fe_sub(&tmp1l, &x2, &z2);
    fe_add(&x2l, &x2, &z2);
    fe_add(&z2l, &x3, &z3);
    fe_mul_tll(&z3, &tmp0l, &x2l);
    fe_mul_tll(&z2, &z2l, &tmp1l);
    fe_sq_tl(&tmp0, &tmp1l);
    fe_sq_tl(&tmp1, &x2l);
    fe_add(&x3l, &z3, &z2);
    fe_sub(&z2l, &z3, &z2);
    fe_mul_ttt(&x2, &tmp1, &tmp0);
    fe_sub(&tmp1l, &tmp1, &tmp0);
    fe_sq_tl(&z2, &z2l);
    fe_mul121666(&z3, &tmp1l);
    fe_sq_tl(&x3, &x3l);
    fe_add(&tmp0l, &tmp0, &z3);
    fe_mul_ttt(&z3, &x1, &z2);
    fe_mul_tll(&z2, &tmp1l, &tmp0l);
  }
  fe_cswap(&x2, &x3, swap);
  fe_cswap(&z2, &z3, swap);

  fe_invert(&z2, &z2);
  fe_mul_ttt(&x2, &x2, &z2);
  fe_tobytes(out, &x2);
}

static void x25519_scalar_mult(uint8_t out[32], const uint8_t scalar[32],
                               const uint8_t point[32]) {
#if defined(BORINGSSL_X25519_NEON)
  if (CRYPTO_is_NEON_capable()) {
    x25519_NEON(out, scalar, point);
    return;
  }
#endif

  x25519_scalar_mult_generic(out, scalar, point);
}

#endif  // BORINGSSL_X25519_X86_64


void X25519_keypair(uint8_t out_public_value[32], uint8_t out_private_key[32]) {
  RAND_bytes(out_private_key, 32);

  // All X25519 implementations should decode scalars correctly (see
  // https://tools.ietf.org/html/rfc7748#section-5). However, if an
  // implementation doesn't then it might interoperate with random keys a
  // fraction of the time because they'll, randomly, happen to be correctly
  // formed.
  //
  // Thus we do the opposite of the masking here to make sure that our private
  // keys are never correctly masked and so, hopefully, any incorrect
  // implementations are deterministically broken.
  //
  // This does not affect security because, although we're throwing away
  // entropy, a valid implementation of scalarmult should throw away the exact
  // same bits anyway.
  out_private_key[0] |= 7;
  out_private_key[31] &= 63;
  out_private_key[31] |= 128;

  X25519_public_from_private(out_public_value, out_private_key);
}

int X25519(uint8_t out_shared_key[32], const uint8_t private_key[32],
           const uint8_t peer_public_value[32]) {
  static const uint8_t kZeros[32] = {0};
  x25519_scalar_mult(out_shared_key, private_key, peer_public_value);
  // The all-zero output results when the input is a point of small order.
  return CRYPTO_memcmp(kZeros, out_shared_key, 32) != 0;
}

#if defined(BORINGSSL_X25519_X86_64)

// When |BORINGSSL_X25519_X86_64| is set, base point multiplication is done with
// the Montgomery ladder because it's faster. Otherwise it's done using the
// Ed25519 tables.

void X25519_public_from_private(uint8_t out_public_value[32],
                                const uint8_t private_key[32]) {
  static const uint8_t kMongomeryBasePoint[32] = {9};
  x25519_scalar_mult(out_public_value, private_key, kMongomeryBasePoint);
}

#else

void X25519_public_from_private(uint8_t out_public_value[32],
                                const uint8_t private_key[32]) {
#if defined(BORINGSSL_X25519_NEON)
  if (CRYPTO_is_NEON_capable()) {
    static const uint8_t kMongomeryBasePoint[32] = {9};
    x25519_NEON(out_public_value, private_key, kMongomeryBasePoint);
    return;
  }
#endif

  uint8_t e[32];
  OPENSSL_memcpy(e, private_key, 32);
  e[0] &= 248;
  e[31] &= 127;
  e[31] |= 64;

  ge_p3 A;
  x25519_ge_scalarmult_base(&A, e);

  // We only need the u-coordinate of the curve25519 point. The map is
  // u=(y+1)/(1-y). Since y=Y/Z, this gives u=(Z+Y)/(Z-Y).
  fe_loose zplusy, zminusy;
  fe zminusy_inv;
  fe_add(&zplusy, &A.Z, &A.Y);
  fe_sub(&zminusy, &A.Z, &A.Y);
  fe_loose_invert(&zminusy_inv, &zminusy);
  fe_mul_tlt(&zminusy_inv, &zplusy, &zminusy_inv);
  fe_tobytes(out_public_value, &zminusy_inv);
}

#endif  // BORINGSSL_X25519_X86_64

