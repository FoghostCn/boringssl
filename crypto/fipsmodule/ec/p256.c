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

// An implementation of the NIST P-256 elliptic curve point multiplication.
// 256-bit Montgomery form, generated using fiat-crypto, for 64 and 32-bit.
//
// OpenSSL integration was taken from Emilia Kasper's work in ecp_nistp224.c.

#include <openssl/base.h>

#if (defined(OPENSSL_32_BIT) || defined(OPENSSL_64_BIT)) && !defined(OPENSSL_WINDOWS)
// MSVC does not implement uint128_t and does not implement cmovznz in constant time.

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include <string.h>

#include "../delocate.h"
#include "../../internal.h"
#include "internal.h"


// "intrinsics"

#if defined(OPENSSL_64_BIT)

static uint64_t cmovznz_u64(uint64_t t, uint64_t z, uint64_t nz) {
	t = -!!t; // all set if nonzero, 0 if 0
	return (t&nz) | ((~t)&z);
}

static uint64_t mulx_u64(uint64_t a, uint64_t b, uint64_t *high) {
  uint128_t x = (uint128_t)a * b;
  *high = (uint64_t) (x >> 64);
  return (uint64_t) x;
}

static uint64_t addcarryx_u64(uint8_t c, uint64_t a, uint64_t b, uint64_t *low) {
  uint128_t x = (uint128_t)a + b + c;
  *low = (uint64_t) x;
  return (uint64_t) (x>>64);
}

static uint64_t subborrow_u64(uint8_t c, uint64_t a, uint64_t b, uint64_t *low) {
  uint128_t t = ((uint128_t) b + c);
  uint128_t x = a-t;
  *low = (uint64_t) x;
  return (uint8_t) (x>>127);
}

#endif
#if defined(OPENSSL_32_BIT)

static uint32_t cmovznz_u32(uint32_t t, uint32_t z, uint32_t nz) {
	t = -!!t; // all set if nonzero, 0 if 0
	return (t&nz) | ((~t)&z);
}

static uint32_t mulx_u32(uint32_t a, uint32_t b, uint32_t *high) {
  uint64_t x = (uint64_t)a * b;
  *high = (uint32_t) (x >> 32);
  return (uint32_t) x;
}

static uint32_t addcarryx_u32(uint8_t c, uint32_t a, uint32_t b, uint32_t *low) {
  uint64_t x = (uint64_t)a + b + c;
  *low = (uint32_t) x;
  return (uint32_t) (x>>32);
}

static uint32_t subborrow_u32(uint8_t c, uint32_t a, uint32_t b, uint32_t *low) {
  uint64_t t = ((uint64_t) b + c);
  uint64_t x = a-t;
  *low = (uint32_t) x;
  return (uint8_t) (x>>63);
}

#endif

// fiat-crypto generated code

#if defined(OPENSSL_64_BIT)

static void fe_add(uint64_t out[4], const uint64_t in1[4], const uint64_t in2[4]) {
  { const uint64_t x8 = in1[3];
  { const uint64_t x9 = in1[2];
  { const uint64_t x7 = in1[1];
  { const uint64_t x5 = in1[0];
  { const uint64_t x14 = in2[3];
  { const uint64_t x15 = in2[2];
  { const uint64_t x13 = in2[1];
  { const uint64_t x11 = in2[0];
  { uint64_t x17; uint8_t x18 = addcarryx_u64(0x0, x5, x11, &x17);
  { uint64_t x20; uint8_t x21 = addcarryx_u64(x18, x7, x13, &x20);
  { uint64_t x23; uint8_t x24 = addcarryx_u64(x21, x9, x15, &x23);
  { uint64_t x26; uint8_t x27 = addcarryx_u64(x24, x8, x14, &x26);
  { uint64_t x29; uint8_t x30 = subborrow_u64(0x0, x17, 0xffffffffffffffffL, &x29);
  { uint64_t x32; uint8_t x33 = subborrow_u64(x30, x20, 0xffffffff, &x32);
  { uint64_t x35; uint8_t x36 = subborrow_u64(x33, x23, 0x0, &x35);
  { uint64_t x38; uint8_t x39 = subborrow_u64(x36, x26, 0xffffffff00000001L, &x38);
  { uint64_t _1; uint8_t x42 = subborrow_u64(x39, x27, 0x0, &_1);
  { uint64_t x43 = cmovznz_u64(x42, x38, x26);
  { uint64_t x44 = cmovznz_u64(x42, x35, x23);
  { uint64_t x45 = cmovznz_u64(x42, x32, x20);
  { uint64_t x46 = cmovznz_u64(x42, x29, x17);
  out[0] = x46;
  out[1] = x45;
  out[2] = x44;
  out[3] = x43;
  }}}}}}}}}}}}}}}}}}}}}
}

static void fe_opp(uint64_t out[4], const uint64_t in1[4]) {
  const uint64_t x5 = in1[3];
  const uint64_t x6 = in1[2];
  const uint64_t x4 = in1[1];
  const uint64_t x2 = in1[0];
  uint64_t x8; uint8_t x9 = subborrow_u64(0x0, 0x0, x2, &x8);
  uint64_t x11; uint8_t x12 = subborrow_u64(x9, 0x0, x4, &x11);
  uint64_t x14; uint8_t x15 = subborrow_u64(x12, 0x0, x6, &x14);
  uint64_t x17; uint8_t x18 = subborrow_u64(x15, 0x0, x5, &x17);
  uint64_t x19 = (uint64_t)cmovznz_u64(x18, 0x0, 0xffffffffffffffffL);
  uint64_t x20 = (x19 & 0xffffffffffffffffL);
  uint64_t x22; uint8_t x23 = addcarryx_u64(0x0, x8, x20, &x22);
  uint64_t x24 = (x19 & 0xffffffff);
  uint64_t x26; uint8_t x27 = addcarryx_u64(x23, x11, x24, &x26);
  uint64_t x29; uint8_t x30 = addcarryx_u64(x27, x14, 0x0, &x29);
  uint64_t x31 = (x19 & 0xffffffff00000001L);
  uint64_t x33; addcarryx_u64(x30, x17, x31, &x33);
  out[0] = x22;
  out[1] = x26;
  out[2] = x29;
  out[3] = x33;
}

static void fe_mul(uint64_t out[4], const uint64_t in1[4], const uint64_t in2[4]) {
  const uint64_t x8 = in1[3];
  const uint64_t x9 = in1[2];
  const uint64_t x7 = in1[1];
  const uint64_t x5 = in1[0];
  const uint64_t x14 = in2[3];
  const uint64_t x15 = in2[2];
  const uint64_t x13 = in2[1];
  const uint64_t x11 = in2[0];
  uint64_t x18;  uint64_t x17 = mulx_u64(x5, x11, &x18);
  uint64_t x21;  uint64_t x20 = mulx_u64(x5, x13, &x21);
  uint64_t x24;  uint64_t x23 = mulx_u64(x5, x15, &x24);
  uint64_t x27;  uint64_t x26 = mulx_u64(x5, x14, &x27);
  uint64_t x29; uint8_t x30 = addcarryx_u64(0x0, x18, x20, &x29);
  uint64_t x32; uint8_t x33 = addcarryx_u64(x30, x21, x23, &x32);
  uint64_t x35; uint8_t x36 = addcarryx_u64(x33, x24, x26, &x35);
  uint64_t x38; addcarryx_u64(0x0, x36, x27, &x38);
  uint64_t x42;  uint64_t x41 = mulx_u64(x17, 0xffffffffffffffffL, &x42);
  uint64_t x45;  uint64_t x44 = mulx_u64(x17, 0xffffffff, &x45);
  uint64_t x48;  uint64_t x47 = mulx_u64(x17, 0xffffffff00000001L, &x48);
  uint64_t x50; uint8_t x51 = addcarryx_u64(0x0, x42, x44, &x50);
  uint64_t x53; uint8_t x54 = addcarryx_u64(x51, x45, 0x0, &x53);
  uint64_t x56; uint8_t x57 = addcarryx_u64(x54, 0x0, x47, &x56);
  uint64_t x59; addcarryx_u64(0x0, x57, x48, &x59);
  uint64_t _2; uint8_t x63 = addcarryx_u64(0x0, x17, x41, &_2);
  uint64_t x65; uint8_t x66 = addcarryx_u64(x63, x29, x50, &x65);
  uint64_t x68; uint8_t x69 = addcarryx_u64(x66, x32, x53, &x68);
  uint64_t x71; uint8_t x72 = addcarryx_u64(x69, x35, x56, &x71);
  uint64_t x74; uint8_t x75 = addcarryx_u64(x72, x38, x59, &x74);
  uint64_t x78;  uint64_t x77 = mulx_u64(x7, x11, &x78);
  uint64_t x81;  uint64_t x80 = mulx_u64(x7, x13, &x81);
  uint64_t x84;  uint64_t x83 = mulx_u64(x7, x15, &x84);
  uint64_t x87;  uint64_t x86 = mulx_u64(x7, x14, &x87);
  uint64_t x89; uint8_t x90 = addcarryx_u64(0x0, x78, x80, &x89);
  uint64_t x92; uint8_t x93 = addcarryx_u64(x90, x81, x83, &x92);
  uint64_t x95; uint8_t x96 = addcarryx_u64(x93, x84, x86, &x95);
  uint64_t x98; addcarryx_u64(0x0, x96, x87, &x98);
  uint64_t x101; uint8_t x102 = addcarryx_u64(0x0, x65, x77, &x101);
  uint64_t x104; uint8_t x105 = addcarryx_u64(x102, x68, x89, &x104);
  uint64_t x107; uint8_t x108 = addcarryx_u64(x105, x71, x92, &x107);
  uint64_t x110; uint8_t x111 = addcarryx_u64(x108, x74, x95, &x110);
  uint64_t x113; uint8_t x114 = addcarryx_u64(x111, x75, x98, &x113);
  uint64_t x117;  uint64_t x116 = mulx_u64(x101, 0xffffffffffffffffL, &x117);
  uint64_t x120;  uint64_t x119 = mulx_u64(x101, 0xffffffff, &x120);
  uint64_t x123;  uint64_t x122 = mulx_u64(x101, 0xffffffff00000001L, &x123);
  uint64_t x125; uint8_t x126 = addcarryx_u64(0x0, x117, x119, &x125);
  uint64_t x128; uint8_t x129 = addcarryx_u64(x126, x120, 0x0, &x128);
  uint64_t x131; uint8_t x132 = addcarryx_u64(x129, 0x0, x122, &x131);
  uint64_t x134; addcarryx_u64(0x0, x132, x123, &x134);
  uint64_t _3; uint8_t x138 = addcarryx_u64(0x0, x101, x116, &_3);
  uint64_t x140; uint8_t x141 = addcarryx_u64(x138, x104, x125, &x140);
  uint64_t x143; uint8_t x144 = addcarryx_u64(x141, x107, x128, &x143);
  uint64_t x146; uint8_t x147 = addcarryx_u64(x144, x110, x131, &x146);
  uint64_t x149; uint8_t x150 = addcarryx_u64(x147, x113, x134, &x149);
  uint8_t x151 = (x150 + x114);
  uint64_t x154;  uint64_t x153 = mulx_u64(x9, x11, &x154);
  uint64_t x157;  uint64_t x156 = mulx_u64(x9, x13, &x157);
  uint64_t x160;  uint64_t x159 = mulx_u64(x9, x15, &x160);
  uint64_t x163;  uint64_t x162 = mulx_u64(x9, x14, &x163);
  uint64_t x165; uint8_t x166 = addcarryx_u64(0x0, x154, x156, &x165);
  uint64_t x168; uint8_t x169 = addcarryx_u64(x166, x157, x159, &x168);
  uint64_t x171; uint8_t x172 = addcarryx_u64(x169, x160, x162, &x171);
  uint64_t x174; addcarryx_u64(0x0, x172, x163, &x174);
  uint64_t x177; uint8_t x178 = addcarryx_u64(0x0, x140, x153, &x177);
  uint64_t x180; uint8_t x181 = addcarryx_u64(x178, x143, x165, &x180);
  uint64_t x183; uint8_t x184 = addcarryx_u64(x181, x146, x168, &x183);
  uint64_t x186; uint8_t x187 = addcarryx_u64(x184, x149, x171, &x186);
  uint64_t x189; uint8_t x190 = addcarryx_u64(x187, x151, x174, &x189);
  uint64_t x193;  uint64_t x192 = mulx_u64(x177, 0xffffffffffffffffL, &x193);
  uint64_t x196;  uint64_t x195 = mulx_u64(x177, 0xffffffff, &x196);
  uint64_t x199;  uint64_t x198 = mulx_u64(x177, 0xffffffff00000001L, &x199);
  uint64_t x201; uint8_t x202 = addcarryx_u64(0x0, x193, x195, &x201);
  uint64_t x204; uint8_t x205 = addcarryx_u64(x202, x196, 0x0, &x204);
  uint64_t x207; uint8_t x208 = addcarryx_u64(x205, 0x0, x198, &x207);
  uint64_t x210; addcarryx_u64(0x0, x208, x199, &x210);
  uint64_t _4; uint8_t x214 = addcarryx_u64(0x0, x177, x192, &_4);
  uint64_t x216; uint8_t x217 = addcarryx_u64(x214, x180, x201, &x216);
  uint64_t x219; uint8_t x220 = addcarryx_u64(x217, x183, x204, &x219);
  uint64_t x222; uint8_t x223 = addcarryx_u64(x220, x186, x207, &x222);
  uint64_t x225; uint8_t x226 = addcarryx_u64(x223, x189, x210, &x225);
  uint8_t x227 = (x226 + x190);
  uint64_t x230;  uint64_t x229 = mulx_u64(x8, x11, &x230);
  uint64_t x233;  uint64_t x232 = mulx_u64(x8, x13, &x233);
  uint64_t x236;  uint64_t x235 = mulx_u64(x8, x15, &x236);
  uint64_t x239;  uint64_t x238 = mulx_u64(x8, x14, &x239);
  uint64_t x241; uint8_t x242 = addcarryx_u64(0x0, x230, x232, &x241);
  uint64_t x244; uint8_t x245 = addcarryx_u64(x242, x233, x235, &x244);
  uint64_t x247; uint8_t x248 = addcarryx_u64(x245, x236, x238, &x247);
  uint64_t x250; addcarryx_u64(0x0, x248, x239, &x250);
  uint64_t x253; uint8_t x254 = addcarryx_u64(0x0, x216, x229, &x253);
  uint64_t x256; uint8_t x257 = addcarryx_u64(x254, x219, x241, &x256);
  uint64_t x259; uint8_t x260 = addcarryx_u64(x257, x222, x244, &x259);
  uint64_t x262; uint8_t x263 = addcarryx_u64(x260, x225, x247, &x262);
  uint64_t x265; uint8_t x266 = addcarryx_u64(x263, x227, x250, &x265);
  uint64_t x269;  uint64_t x268 = mulx_u64(x253, 0xffffffffffffffffL, &x269);
  uint64_t x272;  uint64_t x271 = mulx_u64(x253, 0xffffffff, &x272);
  uint64_t x275;  uint64_t x274 = mulx_u64(x253, 0xffffffff00000001L, &x275);
  uint64_t x277; uint8_t x278 = addcarryx_u64(0x0, x269, x271, &x277);
  uint64_t x280; uint8_t x281 = addcarryx_u64(x278, x272, 0x0, &x280);
  uint64_t x283; uint8_t x284 = addcarryx_u64(x281, 0x0, x274, &x283);
  uint64_t x286; addcarryx_u64(0x0, x284, x275, &x286);
  uint64_t _5; uint8_t x290 = addcarryx_u64(0x0, x253, x268, &_5);
  uint64_t x292; uint8_t x293 = addcarryx_u64(x290, x256, x277, &x292);
  uint64_t x295; uint8_t x296 = addcarryx_u64(x293, x259, x280, &x295);
  uint64_t x298; uint8_t x299 = addcarryx_u64(x296, x262, x283, &x298);
  uint64_t x301; uint8_t x302 = addcarryx_u64(x299, x265, x286, &x301);
  uint8_t x303 = (x302 + x266);
  uint64_t x305; uint8_t x306 = subborrow_u64(0x0, x292, 0xffffffffffffffffL, &x305);
  uint64_t x308; uint8_t x309 = subborrow_u64(x306, x295, 0xffffffff, &x308);
  uint64_t x311; uint8_t x312 = subborrow_u64(x309, x298, 0x0, &x311);
  uint64_t x314; uint8_t x315 = subborrow_u64(x312, x301, 0xffffffff00000001L, &x314);
  uint64_t _6; uint8_t x318 = subborrow_u64(x315, x303, 0x0, &_6);
  uint64_t x319 = cmovznz_u64(x318, x314, x301);
  uint64_t x320 = cmovznz_u64(x318, x311, x298);
  uint64_t x321 = cmovznz_u64(x318, x308, x295);
  uint64_t x322 = cmovznz_u64(x318, x305, x292);
  out[0] = x322;
  out[1] = x321;
  out[2] = x320;
  out[3] = x319;
}

static void fe_sub(uint64_t out[4], const uint64_t in1[4], const uint64_t in2[4]) {
  const uint64_t x8 = in1[3];
  const uint64_t x9 = in1[2];
  const uint64_t x7 = in1[1];
  const uint64_t x5 = in1[0];
  const uint64_t x14 = in2[3];
  const uint64_t x15 = in2[2];
  const uint64_t x13 = in2[1];
  const uint64_t x11 = in2[0];
  uint64_t x17; uint8_t x18 = subborrow_u64(0x0, x5, x11, &x17);
  uint64_t x20; uint8_t x21 = subborrow_u64(x18, x7, x13, &x20);
  uint64_t x23; uint8_t x24 = subborrow_u64(x21, x9, x15, &x23);
  uint64_t x26; uint8_t x27 = subborrow_u64(x24, x8, x14, &x26);
  uint64_t x28 = (uint64_t)cmovznz_u64(x27, 0x0, 0xffffffffffffffffL);
  uint64_t x29 = (x28 & 0xffffffffffffffffL);
  uint64_t x31; uint8_t x32 = addcarryx_u64(0x0, x17, x29, &x31);
  uint64_t x33 = (x28 & 0xffffffff);
  uint64_t x35; uint8_t x36 = addcarryx_u64(x32, x20, x33, &x35);
  uint64_t x38; uint8_t x39 = addcarryx_u64(x36, x23, 0x0, &x38);
  uint64_t x40 = (x28 & 0xffffffff00000001L);
  uint64_t x42; addcarryx_u64(x39, x26, x40, &x42);
  out[0] = x31;
  out[1] = x35;
  out[2] = x38;
  out[3] = x42;
}

#endif
#if defined(OPENSSL_32_BIT)

static void fe_add(uint32_t out[8], const uint32_t in1[8], const uint32_t in2[8]) {
  const uint32_t x16 = in1[7];
  const uint32_t x17 = in1[6];
  const uint32_t x15 = in1[5];
  const uint32_t x13 = in1[4];
  const uint32_t x11 = in1[3];
  const uint32_t x9 = in1[2];
  const uint32_t x7 = in1[1];
  const uint32_t x5 = in1[0];
  const uint32_t x30 = in2[7];
  const uint32_t x31 = in2[6];
  const uint32_t x29 = in2[5];
  const uint32_t x27 = in2[4];
  const uint32_t x25 = in2[3];
  const uint32_t x23 = in2[2];
  const uint32_t x21 = in2[1];
  const uint32_t x19 = in2[0];
  uint32_t x33; uint8_t x34 = addcarryx_u32(0x0, x5, x19, &x33);
  uint32_t x36; uint8_t x37 = addcarryx_u32(x34, x7, x21, &x36);
  uint32_t x39; uint8_t x40 = addcarryx_u32(x37, x9, x23, &x39);
  uint32_t x42; uint8_t x43 = addcarryx_u32(x40, x11, x25, &x42);
  uint32_t x45; uint8_t x46 = addcarryx_u32(x43, x13, x27, &x45);
  uint32_t x48; uint8_t x49 = addcarryx_u32(x46, x15, x29, &x48);
  uint32_t x51; uint8_t x52 = addcarryx_u32(x49, x17, x31, &x51);
  uint32_t x54; uint8_t x55 = addcarryx_u32(x52, x16, x30, &x54);
  uint32_t x57; uint8_t x58 = subborrow_u32(0x0, x33, 0xffffffff, &x57);
  uint32_t x60; uint8_t x61 = subborrow_u32(x58, x36, 0xffffffff, &x60);
  uint32_t x63; uint8_t x64 = subborrow_u32(x61, x39, 0xffffffff, &x63);
  uint32_t x66; uint8_t x67 = subborrow_u32(x64, x42, 0x0, &x66);
  uint32_t x69; uint8_t x70 = subborrow_u32(x67, x45, 0x0, &x69);
  uint32_t x72; uint8_t x73 = subborrow_u32(x70, x48, 0x0, &x72);
  uint32_t x75; uint8_t x76 = subborrow_u32(x73, x51, 0x1, &x75);
  uint32_t x78; uint8_t x79 = subborrow_u32(x76, x54, 0xffffffff, &x78);
  uint32_t _; uint8_t x82 = subborrow_u32(x79, x55, 0x0, &_);
  uint32_t x83 = cmovznz_u32(x82, x78, x54);
  uint32_t x84 = cmovznz_u32(x82, x75, x51);
  uint32_t x85 = cmovznz_u32(x82, x72, x48);
  uint32_t x86 = cmovznz_u32(x82, x69, x45);
  uint32_t x87 = cmovznz_u32(x82, x66, x42);
  uint32_t x88 = cmovznz_u32(x82, x63, x39);
  uint32_t x89 = cmovznz_u32(x82, x60, x36);
  uint32_t x90 = cmovznz_u32(x82, x57, x33);
  out[0] = x90;
  out[1] = x89;
  out[2] = x88;
  out[3] = x87;
  out[4] = x86;
  out[5] = x85;
  out[6] = x84;
  out[7] = x83;
}

static void fe_mul(uint32_t out[8], const uint32_t in1[8], const uint32_t in2[8]) {
  const uint32_t x16 = in1[7];
  const uint32_t x17 = in1[6];
  const uint32_t x15 = in1[5];
  const uint32_t x13 = in1[4];
  const uint32_t x11 = in1[3];
  const uint32_t x9 = in1[2];
  const uint32_t x7 = in1[1];
  const uint32_t x5 = in1[0];
  const uint32_t x30 = in2[7];
  const uint32_t x31 = in2[6];
  const uint32_t x29 = in2[5];
  const uint32_t x27 = in2[4];
  const uint32_t x25 = in2[3];
  const uint32_t x23 = in2[2];
  const uint32_t x21 = in2[1];
  const uint32_t x19 = in2[0];
  uint32_t x34;  uint32_t x33 = mulx_u32(x5, x19, &x34);
  uint32_t x37;  uint32_t x36 = mulx_u32(x5, x21, &x37);
  uint32_t x40;  uint32_t x39 = mulx_u32(x5, x23, &x40);
  uint32_t x43;  uint32_t x42 = mulx_u32(x5, x25, &x43);
  uint32_t x46;  uint32_t x45 = mulx_u32(x5, x27, &x46);
  uint32_t x49;  uint32_t x48 = mulx_u32(x5, x29, &x49);
  uint32_t x52;  uint32_t x51 = mulx_u32(x5, x31, &x52);
  uint32_t x55;  uint32_t x54 = mulx_u32(x5, x30, &x55);
  uint32_t x57; uint8_t x58 = addcarryx_u32(0x0, x34, x36, &x57);
  uint32_t x60; uint8_t x61 = addcarryx_u32(x58, x37, x39, &x60);
  uint32_t x63; uint8_t x64 = addcarryx_u32(x61, x40, x42, &x63);
  uint32_t x66; uint8_t x67 = addcarryx_u32(x64, x43, x45, &x66);
  uint32_t x69; uint8_t x70 = addcarryx_u32(x67, x46, x48, &x69);
  uint32_t x72; uint8_t x73 = addcarryx_u32(x70, x49, x51, &x72);
  uint32_t x75; uint8_t x76 = addcarryx_u32(x73, x52, x54, &x75);
  uint32_t x78; addcarryx_u32(0x0, x76, x55, &x78);
  uint32_t x82;  uint32_t x81 = mulx_u32(x33, 0xffffffff, &x82);
  uint32_t x85;  uint32_t x84 = mulx_u32(x33, 0xffffffff, &x85);
  uint32_t x88;  uint32_t x87 = mulx_u32(x33, 0xffffffff, &x88);
  uint32_t x91;  uint32_t x90 = mulx_u32(x33, 0xffffffff, &x91);
  uint32_t x93; uint8_t x94 = addcarryx_u32(0x0, x82, x84, &x93);
  uint32_t x96; uint8_t x97 = addcarryx_u32(x94, x85, x87, &x96);
  uint32_t x99; uint8_t x100 = addcarryx_u32(x97, x88, 0x0, &x99);
  uint8_t x101 = (0x0 + 0x0);
  uint32_t _1; uint8_t x104 = addcarryx_u32(0x0, x33, x81, &_1);
  uint32_t x106; uint8_t x107 = addcarryx_u32(x104, x57, x93, &x106);
  uint32_t x109; uint8_t x110 = addcarryx_u32(x107, x60, x96, &x109);
  uint32_t x112; uint8_t x113 = addcarryx_u32(x110, x63, x99, &x112);
  uint32_t x115; uint8_t x116 = addcarryx_u32(x113, x66, x100, &x115);
  uint32_t x118; uint8_t x119 = addcarryx_u32(x116, x69, x101, &x118);
  uint32_t x121; uint8_t x122 = addcarryx_u32(x119, x72, x33, &x121);
  uint32_t x124; uint8_t x125 = addcarryx_u32(x122, x75, x90, &x124);
  uint32_t x127; uint8_t x128 = addcarryx_u32(x125, x78, x91, &x127);
  uint8_t x129 = (x128 + 0x0);
  uint32_t x132;  uint32_t x131 = mulx_u32(x7, x19, &x132);
  uint32_t x135;  uint32_t x134 = mulx_u32(x7, x21, &x135);
  uint32_t x138;  uint32_t x137 = mulx_u32(x7, x23, &x138);
  uint32_t x141;  uint32_t x140 = mulx_u32(x7, x25, &x141);
  uint32_t x144;  uint32_t x143 = mulx_u32(x7, x27, &x144);
  uint32_t x147;  uint32_t x146 = mulx_u32(x7, x29, &x147);
  uint32_t x150;  uint32_t x149 = mulx_u32(x7, x31, &x150);
  uint32_t x153;  uint32_t x152 = mulx_u32(x7, x30, &x153);
  uint32_t x155; uint8_t x156 = addcarryx_u32(0x0, x132, x134, &x155);
  uint32_t x158; uint8_t x159 = addcarryx_u32(x156, x135, x137, &x158);
  uint32_t x161; uint8_t x162 = addcarryx_u32(x159, x138, x140, &x161);
  uint32_t x164; uint8_t x165 = addcarryx_u32(x162, x141, x143, &x164);
  uint32_t x167; uint8_t x168 = addcarryx_u32(x165, x144, x146, &x167);
  uint32_t x170; uint8_t x171 = addcarryx_u32(x168, x147, x149, &x170);
  uint32_t x173; uint8_t x174 = addcarryx_u32(x171, x150, x152, &x173);
  uint32_t x176; addcarryx_u32(0x0, x174, x153, &x176);
  uint32_t x179; uint8_t x180 = addcarryx_u32(0x0, x106, x131, &x179);
  uint32_t x182; uint8_t x183 = addcarryx_u32(x180, x109, x155, &x182);
  uint32_t x185; uint8_t x186 = addcarryx_u32(x183, x112, x158, &x185);
  uint32_t x188; uint8_t x189 = addcarryx_u32(x186, x115, x161, &x188);
  uint32_t x191; uint8_t x192 = addcarryx_u32(x189, x118, x164, &x191);
  uint32_t x194; uint8_t x195 = addcarryx_u32(x192, x121, x167, &x194);
  uint32_t x197; uint8_t x198 = addcarryx_u32(x195, x124, x170, &x197);
  uint32_t x200; uint8_t x201 = addcarryx_u32(x198, x127, x173, &x200);
  uint32_t x203; uint8_t x204 = addcarryx_u32(x201, x129, x176, &x203);
  uint32_t x207;  uint32_t x206 = mulx_u32(x179, 0xffffffff, &x207);
  uint32_t x210;  uint32_t x209 = mulx_u32(x179, 0xffffffff, &x210);
  uint32_t x213;  uint32_t x212 = mulx_u32(x179, 0xffffffff, &x213);
  uint32_t x216;  uint32_t x215 = mulx_u32(x179, 0xffffffff, &x216);
  uint32_t x218; uint8_t x219 = addcarryx_u32(0x0, x207, x209, &x218);
  uint32_t x221; uint8_t x222 = addcarryx_u32(x219, x210, x212, &x221);
  uint32_t x224; uint8_t x225 = addcarryx_u32(x222, x213, 0x0, &x224);
  uint8_t x226 = (0x0 + 0x0);
  uint32_t _2; uint8_t x229 = addcarryx_u32(0x0, x179, x206, &_2);
  uint32_t x231; uint8_t x232 = addcarryx_u32(x229, x182, x218, &x231);
  uint32_t x234; uint8_t x235 = addcarryx_u32(x232, x185, x221, &x234);
  uint32_t x237; uint8_t x238 = addcarryx_u32(x235, x188, x224, &x237);
  uint32_t x240; uint8_t x241 = addcarryx_u32(x238, x191, x225, &x240);
  uint32_t x243; uint8_t x244 = addcarryx_u32(x241, x194, x226, &x243);
  uint32_t x246; uint8_t x247 = addcarryx_u32(x244, x197, x179, &x246);
  uint32_t x249; uint8_t x250 = addcarryx_u32(x247, x200, x215, &x249);
  uint32_t x252; uint8_t x253 = addcarryx_u32(x250, x203, x216, &x252);
  uint8_t x254 = (x253 + x204);
  uint32_t x257;  uint32_t x256 = mulx_u32(x9, x19, &x257);
  uint32_t x260;  uint32_t x259 = mulx_u32(x9, x21, &x260);
  uint32_t x263;  uint32_t x262 = mulx_u32(x9, x23, &x263);
  uint32_t x266;  uint32_t x265 = mulx_u32(x9, x25, &x266);
  uint32_t x269;  uint32_t x268 = mulx_u32(x9, x27, &x269);
  uint32_t x272;  uint32_t x271 = mulx_u32(x9, x29, &x272);
  uint32_t x275;  uint32_t x274 = mulx_u32(x9, x31, &x275);
  uint32_t x278;  uint32_t x277 = mulx_u32(x9, x30, &x278);
  uint32_t x280; uint8_t x281 = addcarryx_u32(0x0, x257, x259, &x280);
  uint32_t x283; uint8_t x284 = addcarryx_u32(x281, x260, x262, &x283);
  uint32_t x286; uint8_t x287 = addcarryx_u32(x284, x263, x265, &x286);
  uint32_t x289; uint8_t x290 = addcarryx_u32(x287, x266, x268, &x289);
  uint32_t x292; uint8_t x293 = addcarryx_u32(x290, x269, x271, &x292);
  uint32_t x295; uint8_t x296 = addcarryx_u32(x293, x272, x274, &x295);
  uint32_t x298; uint8_t x299 = addcarryx_u32(x296, x275, x277, &x298);
  uint32_t x301; addcarryx_u32(0x0, x299, x278, &x301);
  uint32_t x304; uint8_t x305 = addcarryx_u32(0x0, x231, x256, &x304);
  uint32_t x307; uint8_t x308 = addcarryx_u32(x305, x234, x280, &x307);
  uint32_t x310; uint8_t x311 = addcarryx_u32(x308, x237, x283, &x310);
  uint32_t x313; uint8_t x314 = addcarryx_u32(x311, x240, x286, &x313);
  uint32_t x316; uint8_t x317 = addcarryx_u32(x314, x243, x289, &x316);
  uint32_t x319; uint8_t x320 = addcarryx_u32(x317, x246, x292, &x319);
  uint32_t x322; uint8_t x323 = addcarryx_u32(x320, x249, x295, &x322);
  uint32_t x325; uint8_t x326 = addcarryx_u32(x323, x252, x298, &x325);
  uint32_t x328; uint8_t x329 = addcarryx_u32(x326, x254, x301, &x328);
  uint32_t x332;  uint32_t x331 = mulx_u32(x304, 0xffffffff, &x332);
  uint32_t x335;  uint32_t x334 = mulx_u32(x304, 0xffffffff, &x335);
  uint32_t x338;  uint32_t x337 = mulx_u32(x304, 0xffffffff, &x338);
  uint32_t x341;  uint32_t x340 = mulx_u32(x304, 0xffffffff, &x341);
  uint32_t x343; uint8_t x344 = addcarryx_u32(0x0, x332, x334, &x343);
  uint32_t x346; uint8_t x347 = addcarryx_u32(x344, x335, x337, &x346);
  uint32_t x349; uint8_t x350 = addcarryx_u32(x347, x338, 0x0, &x349);
  uint8_t x351 = (0x0 + 0x0);
  uint32_t _3; uint8_t x354 = addcarryx_u32(0x0, x304, x331, &_3);
  uint32_t x356; uint8_t x357 = addcarryx_u32(x354, x307, x343, &x356);
  uint32_t x359; uint8_t x360 = addcarryx_u32(x357, x310, x346, &x359);
  uint32_t x362; uint8_t x363 = addcarryx_u32(x360, x313, x349, &x362);
  uint32_t x365; uint8_t x366 = addcarryx_u32(x363, x316, x350, &x365);
  uint32_t x368; uint8_t x369 = addcarryx_u32(x366, x319, x351, &x368);
  uint32_t x371; uint8_t x372 = addcarryx_u32(x369, x322, x304, &x371);
  uint32_t x374; uint8_t x375 = addcarryx_u32(x372, x325, x340, &x374);
  uint32_t x377; uint8_t x378 = addcarryx_u32(x375, x328, x341, &x377);
  uint8_t x379 = (x378 + x329);
  uint32_t x382;  uint32_t x381 = mulx_u32(x11, x19, &x382);
  uint32_t x385;  uint32_t x384 = mulx_u32(x11, x21, &x385);
  uint32_t x388;  uint32_t x387 = mulx_u32(x11, x23, &x388);
  uint32_t x391;  uint32_t x390 = mulx_u32(x11, x25, &x391);
  uint32_t x394;  uint32_t x393 = mulx_u32(x11, x27, &x394);
  uint32_t x397;  uint32_t x396 = mulx_u32(x11, x29, &x397);
  uint32_t x400;  uint32_t x399 = mulx_u32(x11, x31, &x400);
  uint32_t x403;  uint32_t x402 = mulx_u32(x11, x30, &x403);
  uint32_t x405; uint8_t x406 = addcarryx_u32(0x0, x382, x384, &x405);
  uint32_t x408; uint8_t x409 = addcarryx_u32(x406, x385, x387, &x408);
  uint32_t x411; uint8_t x412 = addcarryx_u32(x409, x388, x390, &x411);
  uint32_t x414; uint8_t x415 = addcarryx_u32(x412, x391, x393, &x414);
  uint32_t x417; uint8_t x418 = addcarryx_u32(x415, x394, x396, &x417);
  uint32_t x420; uint8_t x421 = addcarryx_u32(x418, x397, x399, &x420);
  uint32_t x423; uint8_t x424 = addcarryx_u32(x421, x400, x402, &x423);
  uint32_t x426; addcarryx_u32(0x0, x424, x403, &x426);
  uint32_t x429; uint8_t x430 = addcarryx_u32(0x0, x356, x381, &x429);
  uint32_t x432; uint8_t x433 = addcarryx_u32(x430, x359, x405, &x432);
  uint32_t x435; uint8_t x436 = addcarryx_u32(x433, x362, x408, &x435);
  uint32_t x438; uint8_t x439 = addcarryx_u32(x436, x365, x411, &x438);
  uint32_t x441; uint8_t x442 = addcarryx_u32(x439, x368, x414, &x441);
  uint32_t x444; uint8_t x445 = addcarryx_u32(x442, x371, x417, &x444);
  uint32_t x447; uint8_t x448 = addcarryx_u32(x445, x374, x420, &x447);
  uint32_t x450; uint8_t x451 = addcarryx_u32(x448, x377, x423, &x450);
  uint32_t x453; uint8_t x454 = addcarryx_u32(x451, x379, x426, &x453);
  uint32_t x457;  uint32_t x456 = mulx_u32(x429, 0xffffffff, &x457);
  uint32_t x460;  uint32_t x459 = mulx_u32(x429, 0xffffffff, &x460);
  uint32_t x463;  uint32_t x462 = mulx_u32(x429, 0xffffffff, &x463);
  uint32_t x466;  uint32_t x465 = mulx_u32(x429, 0xffffffff, &x466);
  uint32_t x468; uint8_t x469 = addcarryx_u32(0x0, x457, x459, &x468);
  uint32_t x471; uint8_t x472 = addcarryx_u32(x469, x460, x462, &x471);
  uint32_t x474; uint8_t x475 = addcarryx_u32(x472, x463, 0x0, &x474);
  uint8_t x476 = (0x0 + 0x0);
  uint32_t _4; uint8_t x479 = addcarryx_u32(0x0, x429, x456, &_4);
  uint32_t x481; uint8_t x482 = addcarryx_u32(x479, x432, x468, &x481);
  uint32_t x484; uint8_t x485 = addcarryx_u32(x482, x435, x471, &x484);
  uint32_t x487; uint8_t x488 = addcarryx_u32(x485, x438, x474, &x487);
  uint32_t x490; uint8_t x491 = addcarryx_u32(x488, x441, x475, &x490);
  uint32_t x493; uint8_t x494 = addcarryx_u32(x491, x444, x476, &x493);
  uint32_t x496; uint8_t x497 = addcarryx_u32(x494, x447, x429, &x496);
  uint32_t x499; uint8_t x500 = addcarryx_u32(x497, x450, x465, &x499);
  uint32_t x502; uint8_t x503 = addcarryx_u32(x500, x453, x466, &x502);
  uint8_t x504 = (x503 + x454);
  uint32_t x507;  uint32_t x506 = mulx_u32(x13, x19, &x507);
  uint32_t x510;  uint32_t x509 = mulx_u32(x13, x21, &x510);
  uint32_t x513;  uint32_t x512 = mulx_u32(x13, x23, &x513);
  uint32_t x516;  uint32_t x515 = mulx_u32(x13, x25, &x516);
  uint32_t x519;  uint32_t x518 = mulx_u32(x13, x27, &x519);
  uint32_t x522;  uint32_t x521 = mulx_u32(x13, x29, &x522);
  uint32_t x525;  uint32_t x524 = mulx_u32(x13, x31, &x525);
  uint32_t x528;  uint32_t x527 = mulx_u32(x13, x30, &x528);
  uint32_t x530; uint8_t x531 = addcarryx_u32(0x0, x507, x509, &x530);
  uint32_t x533; uint8_t x534 = addcarryx_u32(x531, x510, x512, &x533);
  uint32_t x536; uint8_t x537 = addcarryx_u32(x534, x513, x515, &x536);
  uint32_t x539; uint8_t x540 = addcarryx_u32(x537, x516, x518, &x539);
  uint32_t x542; uint8_t x543 = addcarryx_u32(x540, x519, x521, &x542);
  uint32_t x545; uint8_t x546 = addcarryx_u32(x543, x522, x524, &x545);
  uint32_t x548; uint8_t x549 = addcarryx_u32(x546, x525, x527, &x548);
  uint32_t x551; addcarryx_u32(0x0, x549, x528, &x551);
  uint32_t x554; uint8_t x555 = addcarryx_u32(0x0, x481, x506, &x554);
  uint32_t x557; uint8_t x558 = addcarryx_u32(x555, x484, x530, &x557);
  uint32_t x560; uint8_t x561 = addcarryx_u32(x558, x487, x533, &x560);
  uint32_t x563; uint8_t x564 = addcarryx_u32(x561, x490, x536, &x563);
  uint32_t x566; uint8_t x567 = addcarryx_u32(x564, x493, x539, &x566);
  uint32_t x569; uint8_t x570 = addcarryx_u32(x567, x496, x542, &x569);
  uint32_t x572; uint8_t x573 = addcarryx_u32(x570, x499, x545, &x572);
  uint32_t x575; uint8_t x576 = addcarryx_u32(x573, x502, x548, &x575);
  uint32_t x578; uint8_t x579 = addcarryx_u32(x576, x504, x551, &x578);
  uint32_t x582;  uint32_t x581 = mulx_u32(x554, 0xffffffff, &x582);
  uint32_t x585;  uint32_t x584 = mulx_u32(x554, 0xffffffff, &x585);
  uint32_t x588;  uint32_t x587 = mulx_u32(x554, 0xffffffff, &x588);
  uint32_t x591;  uint32_t x590 = mulx_u32(x554, 0xffffffff, &x591);
  uint32_t x593; uint8_t x594 = addcarryx_u32(0x0, x582, x584, &x593);
  uint32_t x596; uint8_t x597 = addcarryx_u32(x594, x585, x587, &x596);
  uint32_t x599; uint8_t x600 = addcarryx_u32(x597, x588, 0x0, &x599);
  uint8_t x601 = (0x0 + 0x0);
  uint32_t _5; uint8_t x604 = addcarryx_u32(0x0, x554, x581, &_5);
  uint32_t x606; uint8_t x607 = addcarryx_u32(x604, x557, x593, &x606);
  uint32_t x609; uint8_t x610 = addcarryx_u32(x607, x560, x596, &x609);
  uint32_t x612; uint8_t x613 = addcarryx_u32(x610, x563, x599, &x612);
  uint32_t x615; uint8_t x616 = addcarryx_u32(x613, x566, x600, &x615);
  uint32_t x618; uint8_t x619 = addcarryx_u32(x616, x569, x601, &x618);
  uint32_t x621; uint8_t x622 = addcarryx_u32(x619, x572, x554, &x621);
  uint32_t x624; uint8_t x625 = addcarryx_u32(x622, x575, x590, &x624);
  uint32_t x627; uint8_t x628 = addcarryx_u32(x625, x578, x591, &x627);
  uint8_t x629 = (x628 + x579);
  uint32_t x632;  uint32_t x631 = mulx_u32(x15, x19, &x632);
  uint32_t x635;  uint32_t x634 = mulx_u32(x15, x21, &x635);
  uint32_t x638;  uint32_t x637 = mulx_u32(x15, x23, &x638);
  uint32_t x641;  uint32_t x640 = mulx_u32(x15, x25, &x641);
  uint32_t x644;  uint32_t x643 = mulx_u32(x15, x27, &x644);
  uint32_t x647;  uint32_t x646 = mulx_u32(x15, x29, &x647);
  uint32_t x650;  uint32_t x649 = mulx_u32(x15, x31, &x650);
  uint32_t x653;  uint32_t x652 = mulx_u32(x15, x30, &x653);
  uint32_t x655; uint8_t x656 = addcarryx_u32(0x0, x632, x634, &x655);
  uint32_t x658; uint8_t x659 = addcarryx_u32(x656, x635, x637, &x658);
  uint32_t x661; uint8_t x662 = addcarryx_u32(x659, x638, x640, &x661);
  uint32_t x664; uint8_t x665 = addcarryx_u32(x662, x641, x643, &x664);
  uint32_t x667; uint8_t x668 = addcarryx_u32(x665, x644, x646, &x667);
  uint32_t x670; uint8_t x671 = addcarryx_u32(x668, x647, x649, &x670);
  uint32_t x673; uint8_t x674 = addcarryx_u32(x671, x650, x652, &x673);
  uint32_t x676; addcarryx_u32(0x0, x674, x653, &x676);
  uint32_t x679; uint8_t x680 = addcarryx_u32(0x0, x606, x631, &x679);
  uint32_t x682; uint8_t x683 = addcarryx_u32(x680, x609, x655, &x682);
  uint32_t x685; uint8_t x686 = addcarryx_u32(x683, x612, x658, &x685);
  uint32_t x688; uint8_t x689 = addcarryx_u32(x686, x615, x661, &x688);
  uint32_t x691; uint8_t x692 = addcarryx_u32(x689, x618, x664, &x691);
  uint32_t x694; uint8_t x695 = addcarryx_u32(x692, x621, x667, &x694);
  uint32_t x697; uint8_t x698 = addcarryx_u32(x695, x624, x670, &x697);
  uint32_t x700; uint8_t x701 = addcarryx_u32(x698, x627, x673, &x700);
  uint32_t x703; uint8_t x704 = addcarryx_u32(x701, x629, x676, &x703);
  uint32_t x707;  uint32_t x706 = mulx_u32(x679, 0xffffffff, &x707);
  uint32_t x710;  uint32_t x709 = mulx_u32(x679, 0xffffffff, &x710);
  uint32_t x713;  uint32_t x712 = mulx_u32(x679, 0xffffffff, &x713);
  uint32_t x716;  uint32_t x715 = mulx_u32(x679, 0xffffffff, &x716);
  uint32_t x718; uint8_t x719 = addcarryx_u32(0x0, x707, x709, &x718);
  uint32_t x721; uint8_t x722 = addcarryx_u32(x719, x710, x712, &x721);
  uint32_t x724; uint8_t x725 = addcarryx_u32(x722, x713, 0x0, &x724);
  uint8_t x726 = (0x0 + 0x0);
  uint32_t _6; uint8_t x729 = addcarryx_u32(0x0, x679, x706, &_6);
  uint32_t x731; uint8_t x732 = addcarryx_u32(x729, x682, x718, &x731);
  uint32_t x734; uint8_t x735 = addcarryx_u32(x732, x685, x721, &x734);
  uint32_t x737; uint8_t x738 = addcarryx_u32(x735, x688, x724, &x737);
  uint32_t x740; uint8_t x741 = addcarryx_u32(x738, x691, x725, &x740);
  uint32_t x743; uint8_t x744 = addcarryx_u32(x741, x694, x726, &x743);
  uint32_t x746; uint8_t x747 = addcarryx_u32(x744, x697, x679, &x746);
  uint32_t x749; uint8_t x750 = addcarryx_u32(x747, x700, x715, &x749);
  uint32_t x752; uint8_t x753 = addcarryx_u32(x750, x703, x716, &x752);
  uint8_t x754 = (x753 + x704);
  uint32_t x757;  uint32_t x756 = mulx_u32(x17, x19, &x757);
  uint32_t x760;  uint32_t x759 = mulx_u32(x17, x21, &x760);
  uint32_t x763;  uint32_t x762 = mulx_u32(x17, x23, &x763);
  uint32_t x766;  uint32_t x765 = mulx_u32(x17, x25, &x766);
  uint32_t x769;  uint32_t x768 = mulx_u32(x17, x27, &x769);
  uint32_t x772;  uint32_t x771 = mulx_u32(x17, x29, &x772);
  uint32_t x775;  uint32_t x774 = mulx_u32(x17, x31, &x775);
  uint32_t x778;  uint32_t x777 = mulx_u32(x17, x30, &x778);
  uint32_t x780; uint8_t x781 = addcarryx_u32(0x0, x757, x759, &x780);
  uint32_t x783; uint8_t x784 = addcarryx_u32(x781, x760, x762, &x783);
  uint32_t x786; uint8_t x787 = addcarryx_u32(x784, x763, x765, &x786);
  uint32_t x789; uint8_t x790 = addcarryx_u32(x787, x766, x768, &x789);
  uint32_t x792; uint8_t x793 = addcarryx_u32(x790, x769, x771, &x792);
  uint32_t x795; uint8_t x796 = addcarryx_u32(x793, x772, x774, &x795);
  uint32_t x798; uint8_t x799 = addcarryx_u32(x796, x775, x777, &x798);
  uint32_t x801; addcarryx_u32(0x0, x799, x778, &x801);
  uint32_t x804; uint8_t x805 = addcarryx_u32(0x0, x731, x756, &x804);
  uint32_t x807; uint8_t x808 = addcarryx_u32(x805, x734, x780, &x807);
  uint32_t x810; uint8_t x811 = addcarryx_u32(x808, x737, x783, &x810);
  uint32_t x813; uint8_t x814 = addcarryx_u32(x811, x740, x786, &x813);
  uint32_t x816; uint8_t x817 = addcarryx_u32(x814, x743, x789, &x816);
  uint32_t x819; uint8_t x820 = addcarryx_u32(x817, x746, x792, &x819);
  uint32_t x822; uint8_t x823 = addcarryx_u32(x820, x749, x795, &x822);
  uint32_t x825; uint8_t x826 = addcarryx_u32(x823, x752, x798, &x825);
  uint32_t x828; uint8_t x829 = addcarryx_u32(x826, x754, x801, &x828);
  uint32_t x832;  uint32_t x831 = mulx_u32(x804, 0xffffffff, &x832);
  uint32_t x835;  uint32_t x834 = mulx_u32(x804, 0xffffffff, &x835);
  uint32_t x838;  uint32_t x837 = mulx_u32(x804, 0xffffffff, &x838);
  uint32_t x841;  uint32_t x840 = mulx_u32(x804, 0xffffffff, &x841);
  uint32_t x843; uint8_t x844 = addcarryx_u32(0x0, x832, x834, &x843);
  uint32_t x846; uint8_t x847 = addcarryx_u32(x844, x835, x837, &x846);
  uint32_t x849; uint8_t x850 = addcarryx_u32(x847, x838, 0x0, &x849);
  uint8_t x851 = (0x0 + 0x0);
  uint32_t _7; uint8_t x854 = addcarryx_u32(0x0, x804, x831, &_7);
  uint32_t x856; uint8_t x857 = addcarryx_u32(x854, x807, x843, &x856);
  uint32_t x859; uint8_t x860 = addcarryx_u32(x857, x810, x846, &x859);
  uint32_t x862; uint8_t x863 = addcarryx_u32(x860, x813, x849, &x862);
  uint32_t x865; uint8_t x866 = addcarryx_u32(x863, x816, x850, &x865);
  uint32_t x868; uint8_t x869 = addcarryx_u32(x866, x819, x851, &x868);
  uint32_t x871; uint8_t x872 = addcarryx_u32(x869, x822, x804, &x871);
  uint32_t x874; uint8_t x875 = addcarryx_u32(x872, x825, x840, &x874);
  uint32_t x877; uint8_t x878 = addcarryx_u32(x875, x828, x841, &x877);
  uint8_t x879 = (x878 + x829);
  uint32_t x882;  uint32_t x881 = mulx_u32(x16, x19, &x882);
  uint32_t x885;  uint32_t x884 = mulx_u32(x16, x21, &x885);
  uint32_t x888;  uint32_t x887 = mulx_u32(x16, x23, &x888);
  uint32_t x891;  uint32_t x890 = mulx_u32(x16, x25, &x891);
  uint32_t x894;  uint32_t x893 = mulx_u32(x16, x27, &x894);
  uint32_t x897;  uint32_t x896 = mulx_u32(x16, x29, &x897);
  uint32_t x900;  uint32_t x899 = mulx_u32(x16, x31, &x900);
  uint32_t x903;  uint32_t x902 = mulx_u32(x16, x30, &x903);
  uint32_t x905; uint8_t x906 = addcarryx_u32(0x0, x882, x884, &x905);
  uint32_t x908; uint8_t x909 = addcarryx_u32(x906, x885, x887, &x908);
  uint32_t x911; uint8_t x912 = addcarryx_u32(x909, x888, x890, &x911);
  uint32_t x914; uint8_t x915 = addcarryx_u32(x912, x891, x893, &x914);
  uint32_t x917; uint8_t x918 = addcarryx_u32(x915, x894, x896, &x917);
  uint32_t x920; uint8_t x921 = addcarryx_u32(x918, x897, x899, &x920);
  uint32_t x923; uint8_t x924 = addcarryx_u32(x921, x900, x902, &x923);
  uint32_t x926; addcarryx_u32(0x0, x924, x903, &x926);
  uint32_t x929; uint8_t x930 = addcarryx_u32(0x0, x856, x881, &x929);
  uint32_t x932; uint8_t x933 = addcarryx_u32(x930, x859, x905, &x932);
  uint32_t x935; uint8_t x936 = addcarryx_u32(x933, x862, x908, &x935);
  uint32_t x938; uint8_t x939 = addcarryx_u32(x936, x865, x911, &x938);
  uint32_t x941; uint8_t x942 = addcarryx_u32(x939, x868, x914, &x941);
  uint32_t x944; uint8_t x945 = addcarryx_u32(x942, x871, x917, &x944);
  uint32_t x947; uint8_t x948 = addcarryx_u32(x945, x874, x920, &x947);
  uint32_t x950; uint8_t x951 = addcarryx_u32(x948, x877, x923, &x950);
  uint32_t x953; uint8_t x954 = addcarryx_u32(x951, x879, x926, &x953);
  uint32_t x957;  uint32_t x956 = mulx_u32(x929, 0xffffffff, &x957);
  uint32_t x960;  uint32_t x959 = mulx_u32(x929, 0xffffffff, &x960);
  uint32_t x963;  uint32_t x962 = mulx_u32(x929, 0xffffffff, &x963);
  uint32_t x966;  uint32_t x965 = mulx_u32(x929, 0xffffffff, &x966);
  uint32_t x968; uint8_t x969 = addcarryx_u32(0x0, x957, x959, &x968);
  uint32_t x971; uint8_t x972 = addcarryx_u32(x969, x960, x962, &x971);
  uint32_t x974; uint8_t x975 = addcarryx_u32(x972, x963, 0x0, &x974);
  uint8_t x976 = (0x0 + 0x0);
  uint32_t _8; uint8_t x979 = addcarryx_u32(0x0, x929, x956, &_8);
  uint32_t x981; uint8_t x982 = addcarryx_u32(x979, x932, x968, &x981);
  uint32_t x984; uint8_t x985 = addcarryx_u32(x982, x935, x971, &x984);
  uint32_t x987; uint8_t x988 = addcarryx_u32(x985, x938, x974, &x987);
  uint32_t x990; uint8_t x991 = addcarryx_u32(x988, x941, x975, &x990);
  uint32_t x993; uint8_t x994 = addcarryx_u32(x991, x944, x976, &x993);
  uint32_t x996; uint8_t x997 = addcarryx_u32(x994, x947, x929, &x996);
  uint32_t x999; uint8_t x1000 = addcarryx_u32(x997, x950, x965, &x999);
  uint32_t x1002; uint8_t x1003 = addcarryx_u32(x1000, x953, x966, &x1002);
  uint8_t x1004 = (x1003 + x954);
  uint32_t x1006; uint8_t x1007 = subborrow_u32(0x0, x981, 0xffffffff, &x1006);
  uint32_t x1009; uint8_t x1010 = subborrow_u32(x1007, x984, 0xffffffff, &x1009);
  uint32_t x1012; uint8_t x1013 = subborrow_u32(x1010, x987, 0xffffffff, &x1012);
  uint32_t x1015; uint8_t x1016 = subborrow_u32(x1013, x990, 0x0, &x1015);
  uint32_t x1018; uint8_t x1019 = subborrow_u32(x1016, x993, 0x0, &x1018);
  uint32_t x1021; uint8_t x1022 = subborrow_u32(x1019, x996, 0x0, &x1021);
  uint32_t x1024; uint8_t x1025 = subborrow_u32(x1022, x999, 0x1, &x1024);
  uint32_t x1027; uint8_t x1028 = subborrow_u32(x1025, x1002, 0xffffffff, &x1027);
  uint32_t _9; uint8_t x1031 = subborrow_u32(x1028, x1004, 0x0, &_9);
  uint32_t x1032 = cmovznz_u32(x1031, x1027, x1002);
  uint32_t x1033 = cmovznz_u32(x1031, x1024, x999);
  uint32_t x1034 = cmovznz_u32(x1031, x1021, x996);
  uint32_t x1035 = cmovznz_u32(x1031, x1018, x993);
  uint32_t x1036 = cmovznz_u32(x1031, x1015, x990);
  uint32_t x1037 = cmovznz_u32(x1031, x1012, x987);
  uint32_t x1038 = cmovznz_u32(x1031, x1009, x984);
  uint32_t x1039 = cmovznz_u32(x1031, x1006, x981);
  out[0] = x1039;
  out[1] = x1038;
  out[2] = x1037;
  out[3] = x1036;
  out[4] = x1035;
  out[5] = x1034;
  out[6] = x1033;
  out[7] = x1032;
}

// NOTE: the following functions are generated from fiat-crypto, from the same
// template as their 64-bit counterparts above, but the correctness proof of
// the template was not composed with the correctness proof of the
// specialization pipeline. This is because Coq unexplainedly loops on trying
// to synthesize opp and sub using the normal pipeline :/.

static void fe_sub(uint32_t out[8], const uint32_t in1[8], const uint32_t in2[8]) {
  const uint32_t x14 = in1[7];
  const uint32_t x15 = in1[6];
  const uint32_t x13 = in1[5];
  const uint32_t x11 = in1[4];
  const uint32_t x9 = in1[3];
  const uint32_t x7 = in1[2];
  const uint32_t x5 = in1[1];
  const uint32_t x3 = in1[0];
  const uint32_t x28 = in2[7];
  const uint32_t x29 = in2[6];
  const uint32_t x27 = in2[5];
  const uint32_t x25 = in2[4];
  const uint32_t x23 = in2[3];
  const uint32_t x21 = in2[2];
  const uint32_t x19 = in2[1];
  const uint32_t x17 = in2[0];
  uint32_t x31; uint8_t x32 = subborrow_u32(0x0, x3, x17, &x31);
  uint32_t x34; uint8_t x35 = subborrow_u32(x32, x5, x19, &x34);
  uint32_t x37; uint8_t x38 = subborrow_u32(x35, x7, x21, &x37);
  uint32_t x40; uint8_t x41 = subborrow_u32(x38, x9, x23, &x40);
  uint32_t x43; uint8_t x44 = subborrow_u32(x41, x11, x25, &x43);
  uint32_t x46; uint8_t x47 = subborrow_u32(x44, x13, x27, &x46);
  uint32_t x49; uint8_t x50 = subborrow_u32(x47, x15, x29, &x49);
  uint32_t x52; uint8_t x53 = subborrow_u32(x50, x14, x28, &x52);
  uint32_t x54 = cmovznz_u32(x53, 0x0, 0xffffffff);
  uint32_t x56; uint8_t x57 = addcarryx_u32(0x0, x31, (x54 & 0xffffffff), &x56);
  uint32_t x59; uint8_t x60 = addcarryx_u32(x57, x34, (x54 & 0xffffffff), &x59);
  uint32_t x62; uint8_t x63 = addcarryx_u32(x60, x37, (x54 & 0xffffffff), &x62);
  uint32_t x65; uint8_t x66 = addcarryx_u32(x63, x40, 0x0, &x65);
  uint32_t x68; uint8_t x69 = addcarryx_u32(x66, x43, 0x0, &x68);
  uint32_t x71; uint8_t x72 = addcarryx_u32(x69, x46, 0x0, &x71);
  uint32_t x74; uint8_t x75 = addcarryx_u32(x72, x49, ((uint8_t)x54 & 0x1), &x74);
  uint32_t x77; addcarryx_u32(x75, x52, (x54 & 0xffffffff), &x77);
  out[0] = x56;
  out[1] = x59;
  out[2] = x62;
  out[3] = x65;
  out[4] = x68;
  out[5] = x71;
  out[6] = x74;
  out[7] = x77;
}

static void fe_opp(uint32_t out[8], const uint32_t in1[8]) {
  const uint32_t x12 = in1[7];
  const uint32_t x13 = in1[6];
  const uint32_t x11 = in1[5];
  const uint32_t x9 = in1[4];
  const uint32_t x7 = in1[3];
  const uint32_t x5 = in1[2];
  const uint32_t x3 = in1[1];
  const uint32_t x1 = in1[0];
  uint32_t x15; uint8_t x16 = subborrow_u32(0x0, 0x0, x1, &x15);
  uint32_t x18; uint8_t x19 = subborrow_u32(x16, 0x0, x3, &x18);
  uint32_t x21; uint8_t x22 = subborrow_u32(x19, 0x0, x5, &x21);
  uint32_t x24; uint8_t x25 = subborrow_u32(x22, 0x0, x7, &x24);
  uint32_t x27; uint8_t x28 = subborrow_u32(x25, 0x0, x9, &x27);
  uint32_t x30; uint8_t x31 = subborrow_u32(x28, 0x0, x11, &x30);
  uint32_t x33; uint8_t x34 = subborrow_u32(x31, 0x0, x13, &x33);
  uint32_t x36; uint8_t x37 = subborrow_u32(x34, 0x0, x12, &x36);
  uint32_t x38 = cmovznz_u32(x37, 0x0, 0xffffffff);
  uint32_t x40; uint8_t x41 = addcarryx_u32(0x0, x15, (x38 & 0xffffffff), &x40);
  uint32_t x43; uint8_t x44 = addcarryx_u32(x41, x18, (x38 & 0xffffffff), &x43);
  uint32_t x46; uint8_t x47 = addcarryx_u32(x44, x21, (x38 & 0xffffffff), &x46);
  uint32_t x49; uint8_t x50 = addcarryx_u32(x47, x24, 0x0, &x49);
  uint32_t x52; uint8_t x53 = addcarryx_u32(x50, x27, 0x0, &x52);
  uint32_t x55; uint8_t x56 = addcarryx_u32(x53, x30, 0x0, &x55);
  uint32_t x58; uint8_t x59 = addcarryx_u32(x56, x33, ((uint8_t)x38 & 0x1), &x58);
  uint32_t x61; addcarryx_u32(x59, x36, (x38 & 0xffffffff), &x61);
  out[0] = x40;
  out[1] = x43;
  out[2] = x46;
  out[3] = x49;
  out[4] = x52;
  out[5] = x55;
  out[6] = x58;
  out[7] = x61;
}

#endif

// utility functions, handwritten

#define NBYTES 32

#if defined(OPENSSL_64_BIT)
#define NLIMBS 4
typedef uint64_t limb_t;
typedef uint64_t fe[NLIMBS];
static const fe fe_one = {1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe};
static const limb_t rrmodp[NLIMBS] = {3, 0xfffffffbffffffff, 0xfffffffffffffffe, 0x4fffffffd};
#define cmovznz_limb cmovznz_u64
#endif
#if defined(OPENSSL_32_BIT)
#define NLIMBS 8
typedef uint32_t limb_t;
typedef uint32_t fe[NLIMBS];
static const fe fe_one = {1, 0, 0, 0xffffffff, 0xffffffff, 0xffffffff, 0xfffffffe, 0};
static const limb_t rrmodp[NLIMBS] = {3, 0, 0xffffffff, 0xfffffffb, 0xfffffffe, 0xffffffff, 0xfffffffd, 4};
#define cmovznz_limb cmovznz_u32
#endif

static limb_t fe_nz(const limb_t in1[NLIMBS]) {
  limb_t ret = 0;
  for (int i = 0; i < NLIMBS; i++) { ret |= in1[i]; }
  return ret;
}

static void fe_copy(limb_t out[NLIMBS], const limb_t in1[NLIMBS]) {
  for (int i = 0; i < NLIMBS; i++) { out[i] = in1[i]; }
}

static void fe_cmovznz(limb_t out[NLIMBS], limb_t t, const limb_t z[NLIMBS], const limb_t nz[NLIMBS]) {
  for (int i = 0; i < NLIMBS; i++) { out[i] = cmovznz_limb(t, z[i], nz[i]); }
}

static void fe_sqr(fe out, const fe in) {
  fe_mul(out, in, in);
}

static void fe_tobytes(uint8_t out[NBYTES], const fe in) {
  // ((aR)*1)/R = a
  fe tmp = {0};
  static const limb_t _one[NLIMBS] = {1, 0};
  fe_mul(tmp, _one, in);
  for (int i = 0; i<NBYTES; i++) {
    out[i] = (tmp[i/sizeof(tmp[0])] >> (8*(i%sizeof(tmp[0]))))&0xff;
  }
}

static void fe_frombytes(fe out, const uint8_t in[NBYTES]) {
  // (a*(R*R))/R = (aR)
  for (int i = 0; i<NLIMBS; i++) {
    out[i] = 0;
  }
  for (int i = 0; i<NBYTES; i++) {
    out[i/sizeof(out[0])] |= ((limb_t)in[i]) << (8*(i%sizeof(out[0])));
  }

  fe_mul(out, out, rrmodp);
}

// BN_* compatability wrappers

// To preserve endianness when using BN_bn2bin and BN_bin2bn.
static void flip_endian(uint8_t *out, const uint8_t *in, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    out[i] = in[len - 1 - i];
  }
}

static int BN_to_fe(fe out, const BIGNUM *bn) {
  uint8_t tmp[NBYTES];
  if (!BN_bn2le_padded(tmp, NBYTES, bn)) { return 0; }
  fe_frombytes(out, tmp);
  return 1;
}

static BIGNUM *fe_to_BN(BIGNUM *out, const fe in) {
  uint8_t tmp[NBYTES];
  fe_tobytes(tmp, in);
  return BN_le2bn(tmp, NBYTES, out);
}

// fe_inv calculates |out| = |in|^{-1}
//
// Based on Fermat's Little Theorem:
//   a^p = a (mod p)
//   a^{p-1} = 1 (mod p)
//   a^{p-2} = a^{-1} (mod p)
static void fe_inv(fe out, const fe in) {
  fe ftmp, ftmp2;
  // each e_I will hold |in|^{2^I - 1}
  fe e2, e4, e8, e16, e32, e64;

  fe_sqr(ftmp, in);  // 2^1
  fe_mul(ftmp, in, ftmp);  // 2^2 - 2^0
  fe_copy(e2, ftmp);
  fe_sqr(ftmp, ftmp);  // 2^3 - 2^1
  fe_sqr(ftmp, ftmp);  // 2^4 - 2^2
  fe_mul(ftmp, ftmp, e2);  // 2^4 - 2^0
  fe_copy(e4, ftmp);
  fe_sqr(ftmp, ftmp);  // 2^5 - 2^1
  fe_sqr(ftmp, ftmp);  // 2^6 - 2^2
  fe_sqr(ftmp, ftmp);  // 2^7 - 2^3
  fe_sqr(ftmp, ftmp);  // 2^8 - 2^4
  fe_mul(ftmp, ftmp, e4);  // 2^8 - 2^0
  fe_copy(e8, ftmp);
  for (size_t i = 0; i < 8; i++) {
    fe_sqr(ftmp, ftmp);
  }  // 2^16 - 2^8
  fe_mul(ftmp, ftmp, e8);  // 2^16 - 2^0
  fe_copy(e16, ftmp);
  for (size_t i = 0; i < 16; i++) {
    fe_sqr(ftmp, ftmp);
  }  // 2^32 - 2^16
  fe_mul(ftmp, ftmp, e16);  // 2^32 - 2^0
  fe_copy(e32, ftmp);
  for (size_t i = 0; i < 32; i++) {
    fe_sqr(ftmp, ftmp);
  }  // 2^64 - 2^32
  fe_copy(e64, ftmp);
  fe_mul(ftmp, ftmp, in);  // 2^64 - 2^32 + 2^0
  for (size_t i = 0; i < 192; i++) {
    fe_sqr(ftmp, ftmp);
  }  // 2^256 - 2^224 + 2^192

  fe_mul(ftmp2, e64, e32);  // 2^64 - 2^0
  for (size_t i = 0; i < 16; i++) {
    fe_sqr(ftmp2, ftmp2);
  }  // 2^80 - 2^16
  fe_mul(ftmp2, ftmp2, e16);  // 2^80 - 2^0
  for (size_t i = 0; i < 8; i++) {
    fe_sqr(ftmp2, ftmp2);
  }  // 2^88 - 2^8
  fe_mul(ftmp2, ftmp2, e8);  // 2^88 - 2^0
  for (size_t i = 0; i < 4; i++) {
    fe_sqr(ftmp2, ftmp2);
  }  // 2^92 - 2^4
  fe_mul(ftmp2, ftmp2, e4);  // 2^92 - 2^0
  fe_sqr(ftmp2, ftmp2);  // 2^93 - 2^1
  fe_sqr(ftmp2, ftmp2);  // 2^94 - 2^2
  fe_mul(ftmp2, ftmp2, e2);  // 2^94 - 2^0
  fe_sqr(ftmp2, ftmp2);  // 2^95 - 2^1
  fe_sqr(ftmp2, ftmp2);  // 2^96 - 2^2
  fe_mul(ftmp2, ftmp2, in);  // 2^96 - 3

  fe_mul(out, ftmp2, ftmp);  // 2^256 - 2^224 + 2^192 + 2^96 - 3
}

// Group operations
// ----------------
//
// Building on top of the field operations we have the operations on the
// elliptic curve group itself. Points on the curve are represented in Jacobian
// coordinates.

// point_double calculates 2*(x_in, y_in, z_in)
//
// The method is taken from:
//   http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#doubling-dbl-2001-b
//
// Outputs can equal corresponding inputs, i.e., x_out == x_in is allowed.
// while x_out == y_in is not (maybe this works, but it's not tested).
static void point_double(fe x_out, fe y_out, fe z_out,
                         const fe x_in, const fe y_in, const fe z_in) {
  fe delta, gamma, beta, ftmp, ftmp2, tmptmp, alpha, fourbeta;
  // delta = z^2
  fe_sqr(delta, z_in);
  // gamma = y^2
  fe_sqr(gamma, y_in);
  // beta = x*gamma
  fe_mul(beta, x_in, gamma);

  // alpha = 3*(x-delta)*(x+delta)
  fe_sub(ftmp, x_in, delta);
  fe_add(ftmp2, x_in, delta);
  
  fe_add(tmptmp, ftmp2, ftmp2);
  fe_add(ftmp2, ftmp2, tmptmp);
  fe_mul(alpha, ftmp, ftmp2);

  // x' = alpha^2 - 8*beta
  fe_sqr(x_out, alpha);
  fe_add(fourbeta, beta, beta);
  fe_add(fourbeta, fourbeta, fourbeta);
  fe_add(tmptmp, fourbeta, fourbeta);
  fe_sub(x_out, x_out, tmptmp);

  // z' = (y + z)^2 - gamma - delta
  fe_add(delta, gamma, delta);
  fe_add(ftmp, y_in, z_in);
  fe_sqr(z_out, ftmp);
  fe_sub(z_out, z_out, delta);

  // y' = alpha*(4*beta - x') - 8*gamma^2
  fe_sub(y_out, fourbeta, x_out);
  fe_add(gamma, gamma, gamma);
  fe_sqr(gamma, gamma);
  fe_mul(y_out, alpha, y_out);
  fe_add(gamma, gamma, gamma);
  fe_sub(y_out, y_out, gamma);
}

// point_add calcuates (x1, y1, z1) + (x2, y2, z2)
//
// The method is taken from:
//   http://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#addition-add-2007-bl,
// adapted for mixed addition (z2 = 1, or z2 = 0 for the point at infinity).
//
// This function includes a branch for checking whether the two input points
// are equal, (while not equal to the point at infinity). This case never
// happens during single point multiplication, so there is no timing leak for
// ECDH or ECDSA signing.
static void point_add(fe x3, fe y3, fe z3, const fe x1,
                      const fe y1, const fe z1, const int mixed,
                      const fe x2, const fe y2, const fe z2) {
  fe x_out, y_out, z_out;
  limb_t z1nz = fe_nz(z1);
  limb_t z2nz = fe_nz(z2);

  // ftmp = z1z1 = z1**2
  fe z1z1; fe_sqr(z1z1, z1);

  fe u1, s1, ftmp5;
  if (!mixed) {
    // ftmp2 = z2z2 = z2**2
    fe z2z2; fe_sqr(z2z2, z2);

    // u1 = ftmp3 = x1*z2z2
    fe_mul(u1, x1, z2z2);

    // ftmp5 = (z1 + z2)**2 - (z1z1 + z2z2) = 2z1z2
    fe_add(ftmp5, z1, z2);
    fe_sqr(ftmp5, ftmp5);
    fe_sub(ftmp5, ftmp5, z1z1);
    fe_sub(ftmp5, ftmp5, z2z2);

    // s1 = ftmp2 = y1 * z2**3
    fe_mul(s1, z2, z2z2);
    fe_mul(s1, s1, y1);
  } else {
    // We'll assume z2 = 1 (special case z2 = 0 is handled later).

    // u1 = ftmp3 = x1*z2z2
    fe_copy(u1, x1);
    // ftmp5 = 2z1z2
    fe_add(ftmp5, z1, z1);
    // s1 = ftmp2 = y1 * z2**3
    fe_copy(s1, y1);
  }

  // u2 = x2*z1z1
  fe u2; fe_mul(u2, x2, z1z1);

  // h = ftmp4 = u2 - u1
  fe h; fe_sub(h, u2, u1);

  limb_t xneq = fe_nz(h);

  // z_out = ftmp5 * h
  fe_mul(z_out, h, ftmp5);

  // ftmp = z1 * z1z1
  fe z1z1z1; fe_mul(z1z1z1, z1, z1z1);

  // s2 = tmp = y2 * z1**3
  fe s2; fe_mul(s2, y2, z1z1z1);

  // r = ftmp5 = (s2 - s1)*2
  fe r;
  fe_sub(r, s2, s1);
  fe_add(r, r, r);

  limb_t yneq = fe_nz(r);

  if (!xneq && !yneq && z1nz && z2nz) {
    point_double(x_out, y_out, z_out, x1, y1, z1);
    return;
  }

  // I = ftmp = (2h)**2
  fe i;
  fe_add(i, h, h);
  fe_sqr(i, i);

  // J = ftmp2 = h * I
  fe j; fe_mul(j, h, i);

  // V = ftmp4 = U1 * I
  fe v; fe_mul(v, u1, i);

  // x_out = r**2 - J - 2V
  fe_sqr(x_out, r);
  fe_sub(x_out, x_out, j);
  fe_sub(x_out, x_out, v);
  fe_sub(x_out, x_out, v);

  // y_out = r(V-x_out) - 2 * s1 * J
  fe_sub(y_out, v, x_out);
  fe_mul(y_out, y_out, r);
  fe s1j;
  fe_mul(s1j, s1, j);
  fe_sub(y_out, y_out, s1j);
  fe_sub(y_out, y_out, s1j);

  fe_cmovznz(x_out, z1nz, x2, x_out);
  fe_cmovznz(x3, z2nz, x1, x_out);
  fe_cmovznz(y_out, z1nz, y2, y_out);
  fe_cmovznz(y3, z2nz, y1, y_out);
  fe_cmovznz(z_out, z1nz, z2, z_out);
  fe_cmovznz(z3, z2nz, z1, z_out);
}

// Base point pre computation
// --------------------------
//
// Two different sorts of precomputed tables are used in the following code.
// Each contain various points on the curve, where each point is three field
// elements (x, y, z).
//
// For the base point table, z is usually 1 (0 for the point at infinity).
// This table has 2 * 16 elements, starting with the following:
// index | bits    | point
// ------+---------+------------------------------
//     0 | 0 0 0 0 | 0G
//     1 | 0 0 0 1 | 1G
//     2 | 0 0 1 0 | 2^64G
//     3 | 0 0 1 1 | (2^64 + 1)G
//     4 | 0 1 0 0 | 2^128G
//     5 | 0 1 0 1 | (2^128 + 1)G
//     6 | 0 1 1 0 | (2^128 + 2^64)G
//     7 | 0 1 1 1 | (2^128 + 2^64 + 1)G
//     8 | 1 0 0 0 | 2^192G
//     9 | 1 0 0 1 | (2^192 + 1)G
//    10 | 1 0 1 0 | (2^192 + 2^64)G
//    11 | 1 0 1 1 | (2^192 + 2^64 + 1)G
//    12 | 1 1 0 0 | (2^192 + 2^128)G
//    13 | 1 1 0 1 | (2^192 + 2^128 + 1)G
//    14 | 1 1 1 0 | (2^192 + 2^128 + 2^64)G
//    15 | 1 1 1 1 | (2^192 + 2^128 + 2^64 + 1)G
// followed by a copy of this with each element multiplied by 2^32.
//
// The reason for this is so that we can clock bits into four different
// locations when doing simple scalar multiplies against the base point,
// and then another four locations using the second 16 elements.
//
// Tables for other points have table[i] = iG for i in 0 .. 16.

// g_pre_comp is the table of precomputed base points
#if defined(OPENSSL_64_BIT)
static const fe g_pre_comp[2][16][3] = {
    {{{0x0, 0x0, 0x0, 0x0}, {0x0, 0x0, 0x0, 0x0}, {0x0, 0x0, 0x0, 0x0}},
     {{0x79e730d418a9143c, 0x75ba95fc5fedb601, 0x79fb732b77622510,
       0x18905f76a53755c6},
      {0xddf25357ce95560a, 0x8b4ab8e4ba19e45c, 0xd2e88688dd21f325,
       0x8571ff1825885d85},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x4f922fc516a0d2bb, 0xd5cc16c1a623499, 0x9241cf3a57c62c8b,
       0x2f5e6961fd1b667f},
      {0x5c15c70bf5a01797, 0x3d20b44d60956192, 0x4911b37071fdb52,
       0xf648f9168d6f0f7b},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x9e566847e137bbbc, 0xe434469e8a6a0bec, 0xb1c4276179d73463,
       0x5abe0285133d0015},
      {0x92aa837cc04c7dab, 0x573d9f4c43260c07, 0xc93156278e6cc37,
       0x94bb725b6b6f7383},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x62a8c244bfe20925, 0x91c19ac38fdce867, 0x5a96a5d5dd387063,
       0x61d587d421d324f6},
      {0xe87673a2a37173ea, 0x2384800853778b65, 0x10f8441e05bab43e,
       0xfa11fe124621efbe},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x1c891f2b2cb19ffd, 0x1ba8d5bb1923c23, 0xb6d03d678ac5ca8e,
       0x586eb04c1f13bedc},
      {0xc35c6e527e8ed09, 0x1e81a33c1819ede2, 0x278fd6c056c652fa,
       0x19d5ac0870864f11},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x62577734d2b533d5, 0x673b8af6a1bdddc0, 0x577e7c9aa79ec293,
       0xbb6de651c3b266b1},
      {0xe7e9303ab65259b3, 0xd6a0afd3d03a7480, 0xc5ac83d19b3cfc27,
       0x60b4619a5d18b99b},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xbd6a38e11ae5aa1c, 0xb8b7652b49e73658, 0xb130014ee5f87ed,
       0x9d0f27b2aeebffcd},
      {0xca9246317a730a55, 0x9c955b2fddbbc83a, 0x7c1dfe0ac019a71,
       0x244a566d356ec48d},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x56f8410ef4f8b16a, 0x97241afec47b266a, 0xa406b8e6d9c87c1,
       0x803f3e02cd42ab1b},
      {0x7f0309a804dbec69, 0xa83b85f73bbad05f, 0xc6097273ad8e197f,
       0xc097440e5067adc1},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x846a56f2c379ab34, 0xa8ee068b841df8d1, 0x20314459176c68ef,
       0xf1af32d5915f1f30},
      {0x99c375315d75bd50, 0x837cffbaf72f67bc, 0x613a41848d7723f,
       0x23d0f130e2d41c8b},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xed93e225d5be5a2b, 0x6fe799835934f3c6, 0x4314092622626ffc,
       0x50bbb4d97990216a},
      {0x378191c6e57ec63e, 0x65422c40181dcdb2, 0x41a8099b0236e0f6,
       0x2b10011801fe49c3},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xfc68b5c59b391593, 0xc385f5a2598270fc, 0x7144f3aad19adcbb,
       0xdd55899983fbae0c},
      {0x93b88b8e74b82ff4, 0xd2e03c4071e734c9, 0x9a7a9eaf43c0322a,
       0xe6e4c551149d6041},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x5fe14bfe80ec21fe, 0xf6ce116ac255be82, 0x98bc5a072f4a5d67,
       0xfad27148db7e63af},
      {0x90c0b6ac29ab05b3, 0x37a9a83c4e251ae6, 0xa7dc875c2aade7d,
       0x77387de39f0e1a84},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x1e9ecc49a56c0dd7, 0xa5cffcd846086c74, 0x8f7a1408f505aece,
       0xb37b85c0bef0c47e},
      {0x3596b6e4cc0e6a8f, 0xfd6d4bbf6b388f23, 0xaba453fac39cef4e,
       0x9c135ac8f9f628d5},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xa1c729495c8f8be, 0x2961c4803bf362bf, 0x9e418403df63d4ac,
       0xc109f9cb91ece900},
      {0xc2d095d058945705, 0xb9083d96ddeb85c0, 0x84692b8d7a40449b,
       0x9bc3344f2eee1ee1},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xd5ae35642913074, 0x55491b2748a542b1, 0x469ca665b310732a,
       0x29591d525f1a4cc1},
      {0xe76f5b6bb84f983f, 0xbe7eef419f5f84e1, 0x1200d49680baa189,
       0x6376551f18ef332c},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}}},
    {{{0x0, 0x0, 0x0, 0x0}, {0x0, 0x0, 0x0, 0x0}, {0x0, 0x0, 0x0, 0x0}},
     {{0x202886024147519a, 0xd0981eac26b372f0, 0xa9d4a7caa785ebc8,
       0xd953c50ddbdf58e9},
      {0x9d6361ccfd590f8f, 0x72e9626b44e6c917, 0x7fd9611022eb64cf,
       0x863ebb7e9eb288f3},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x4fe7ee31b0e63d34, 0xf4600572a9e54fab, 0xc0493334d5e7b5a4,
       0x8589fb9206d54831},
      {0xaa70f5cc6583553a, 0x879094ae25649e5, 0xcc90450710044652,
       0xebb0696d02541c4f},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xabbaa0c03b89da99, 0xa6f2d79eb8284022, 0x27847862b81c05e8,
       0x337a4b5905e54d63},
      {0x3c67500d21f7794a, 0x207005b77d6d7f61, 0xa5a378104cfd6e8,
       0xd65e0d5f4c2fbd6},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xd433e50f6d3549cf, 0x6f33696ffacd665e, 0x695bfdacce11fcb4,
       0x810ee252af7c9860},
      {0x65450fe17159bb2c, 0xf7dfbebe758b357b, 0x2b057e74d69fea72,
       0xd485717a92731745},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xce1f69bbe83f7669, 0x9f8ae8272877d6b, 0x9548ae543244278d,
       0x207755dee3c2c19c},
      {0x87bd61d96fef1945, 0x18813cefb12d28c3, 0x9fbcd1d672df64aa,
       0x48dc5ee57154b00d},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xef0f469ef49a3154, 0x3e85a5956e2b2e9a, 0x45aaec1eaa924a9c,
       0xaa12dfc8a09e4719},
      {0x26f272274df69f1d, 0xe0e4c82ca2ff5e73, 0xb9d8ce73b7a9dd44,
       0x6c036e73e48ca901},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xe1e421e1a47153f0, 0xb86c3b79920418c9, 0x93bdce87705d7672,
       0xf25ae793cab79a77},
      {0x1f3194a36d869d0c, 0x9d55c8824986c264, 0x49fb5ea3096e945e,
       0x39b8e65313db0a3e},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xe3417bc035d0b34a, 0x440b386b8327c0a7, 0x8fb7262dac0362d1,
       0x2c41114ce0cdf943},
      {0x2ba5cef1ad95a0b1, 0xc09b37a867d54362, 0x26d6cdd201e486c9,
       0x20477abf42ff9297},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xf121b41bc0a67d2, 0x62d4760a444d248a, 0xe044f1d659b4737,
       0x8fde365250bb4a8},
      {0xaceec3da848bf287, 0xc2a62182d3369d6e, 0x3582dfdc92449482,
       0x2f7e2fd2565d6cd7},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xa0122b5178a876b, 0x51ff96ff085104b4, 0x50b31ab14f29f76,
       0x84abb28b5f87d4e6},
      {0xd5ed439f8270790a, 0x2d6cb59d85e3f46b, 0x75f55c1b6c1e2212,
       0xe5436f6717655640},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xc2965ecc9aeb596d, 0x1ea03e7023c92b4, 0x4704b4b62e013961,
       0xca8fd3f905ea367},
      {0x92523a42551b2b61, 0x1eb7a89c390fcd06, 0xe7f1d2be0392a63e,
       0x96dca2644ddb0c33},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x231c210e15339848, 0xe87a28e870778c8d, 0x9d1de6616956e170,
       0x4ac3c9382bb09c0b},
      {0x19be05516998987d, 0x8b2376c4ae09f4d6, 0x1de0b7651a3f933d,
       0x380d94c7e39705f4},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x3685954b8c31c31d, 0x68533d005bf21a0c, 0xbd7626e75c79ec9,
       0xca17754742c69d54},
      {0xcc6edafff6d2dbb2, 0xfd0d8cbd174a9d18, 0x875e8793aa4578e8,
       0xa976a7139cab2ce6},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0xce37ab11b43ea1db, 0xa7ff1a95259d292, 0x851b02218f84f186,
       0xa7222beadefaad13},
      {0xa2ac78ec2b0a9144, 0x5a024051f2fa59c5, 0x91d1eca56147ce38,
       0xbe94d523bc2ac690},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}},
     {{0x2d8daefd79ec1a0f, 0x3bbcd6fdceb39c97, 0xf5575ffc58f61a95,
       0xdbd986c4adf7b420},
      {0x81aa881415f39eb7, 0x6ee2fcf5b98d976c, 0x5465475dcf2f717d,
       0x8e24d3c46860bbd0},
      {0x1, 0xffffffff00000000, 0xffffffffffffffff, 0xfffffffe}}}};
#endif

#if defined(OPENSSL_32_BIT)
static const fe g_pre_comp[2][16][3] = {
    {{{0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0},
      {0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0},
      {0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0}},
     {{0x18a9143c,0x79e730d4, 0x5fedb601,0x75ba95fc, 0x77622510,0x79fb732b,
       0xa53755c6,0x18905f76},
      {0xce95560a,0xddf25357, 0xba19e45c,0x8b4ab8e4, 0xdd21f325,0xd2e88688,
       0x25885d85,0x8571ff18},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x16a0d2bb,0x4f922fc5, 0x1a623499,0xd5cc16c, 0x57c62c8b,0x9241cf3a,
       0xfd1b667f,0x2f5e6961},
      {0xf5a01797,0x5c15c70b, 0x60956192,0x3d20b44d, 0x71fdb52,0x4911b37,
       0x8d6f0f7b,0xf648f916},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xe137bbbc,0x9e566847, 0x8a6a0bec,0xe434469e, 0x79d73463,0xb1c42761,
       0x133d0015,0x5abe0285},
      {0xc04c7dab,0x92aa837c, 0x43260c07,0x573d9f4c, 0x78e6cc37,0xc931562,
       0x6b6f7383,0x94bb725b},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xbfe20925,0x62a8c244, 0x8fdce867,0x91c19ac3, 0xdd387063,0x5a96a5d5,
       0x21d324f6,0x61d587d4},
      {0xa37173ea,0xe87673a2, 0x53778b65,0x23848008, 0x5bab43e,0x10f8441e,
       0x4621efbe,0xfa11fe12},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x2cb19ffd,0x1c891f2b, 0xb1923c23,0x1ba8d5b, 0x8ac5ca8e,0xb6d03d67,
       0x1f13bedc,0x586eb04c},
      {0x27e8ed09,0xc35c6e5, 0x1819ede2,0x1e81a33c, 0x56c652fa,0x278fd6c0,
       0x70864f11,0x19d5ac08},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xd2b533d5,0x62577734, 0xa1bdddc0,0x673b8af6, 0xa79ec293,0x577e7c9a,
       0xc3b266b1,0xbb6de651},
      {0xb65259b3,0xe7e9303a, 0xd03a7480,0xd6a0afd3, 0x9b3cfc27,0xc5ac83d1,
       0x5d18b99b,0x60b4619a},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x1ae5aa1c,0xbd6a38e1, 0x49e73658,0xb8b7652b, 0xee5f87ed,0xb130014,
       0xaeebffcd,0x9d0f27b2},
      {0x7a730a55,0xca924631, 0xddbbc83a,0x9c955b2f, 0xac019a71,0x7c1dfe0,
       0x356ec48d,0x244a566d},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xf4f8b16a,0x56f8410e, 0xc47b266a,0x97241afe, 0x6d9c87c1,0xa406b8e,
       0xcd42ab1b,0x803f3e02},
      {0x4dbec69,0x7f0309a8, 0x3bbad05f,0xa83b85f7, 0xad8e197f,0xc6097273,
       0x5067adc1,0xc097440e},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xc379ab34,0x846a56f2, 0x841df8d1,0xa8ee068b, 0x176c68ef,0x20314459,
       0x915f1f30,0xf1af32d5},
      {0x5d75bd50,0x99c37531, 0xf72f67bc,0x837cffba, 0x48d7723f,0x613a418,
       0xe2d41c8b,0x23d0f130},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xd5be5a2b,0xed93e225, 0x5934f3c6,0x6fe79983, 0x22626ffc,0x43140926,
       0x7990216a,0x50bbb4d9},
      {0xe57ec63e,0x378191c6, 0x181dcdb2,0x65422c40, 0x236e0f6,0x41a8099b,
       0x1fe49c3,0x2b100118},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x9b391593,0xfc68b5c5, 0x598270fc,0xc385f5a2, 0xd19adcbb,0x7144f3aa,
       0x83fbae0c,0xdd558999},
      {0x74b82ff4,0x93b88b8e, 0x71e734c9,0xd2e03c40, 0x43c0322a,0x9a7a9eaf,
       0x149d6041,0xe6e4c551},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x80ec21fe,0x5fe14bfe, 0xc255be82,0xf6ce116a, 0x2f4a5d67,0x98bc5a07,
       0xdb7e63af,0xfad27148},
      {0x29ab05b3,0x90c0b6ac, 0x4e251ae6,0x37a9a83c, 0xc2aade7d,0xa7dc875,
       0x9f0e1a84,0x77387de3},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xa56c0dd7,0x1e9ecc49, 0x46086c74,0xa5cffcd8, 0xf505aece,0x8f7a1408,
       0xbef0c47e,0xb37b85c0},
      {0xcc0e6a8f,0x3596b6e4, 0x6b388f23,0xfd6d4bbf, 0xc39cef4e,0xaba453fa,
       0xf9f628d5,0x9c135ac8},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x95c8f8be,0xa1c7294, 0x3bf362bf,0x2961c480, 0xdf63d4ac,0x9e418403,
       0x91ece900,0xc109f9cb},
      {0x58945705,0xc2d095d0, 0xddeb85c0,0xb9083d96, 0x7a40449b,0x84692b8d,
       0x2eee1ee1,0x9bc3344f},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x42913074,0xd5ae356, 0x48a542b1,0x55491b27, 0xb310732a,0x469ca665,
       0x5f1a4cc1,0x29591d52},
      {0xb84f983f,0xe76f5b6b, 0x9f5f84e1,0xbe7eef41, 0x80baa189,0x1200d496,
       0x18ef332c,0x6376551f},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}}},
    {{{0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0},
      {0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0},
      {0x0,0x0, 0x0,0x0, 0x0,0x0, 0x0,0x0}},
     {{0x4147519a,0x20288602, 0x26b372f0,0xd0981eac, 0xa785ebc8,0xa9d4a7ca,
       0xdbdf58e9,0xd953c50d},
      {0xfd590f8f,0x9d6361cc, 0x44e6c917,0x72e9626b, 0x22eb64cf,0x7fd96110,
       0x9eb288f3,0x863ebb7e},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xb0e63d34,0x4fe7ee31, 0xa9e54fab,0xf4600572, 0xd5e7b5a4,0xc0493334,
       0x6d54831,0x8589fb92},
      {0x6583553a,0xaa70f5cc, 0xe25649e5,0x879094a, 0x10044652,0xcc904507,
       0x2541c4f,0xebb0696d},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x3b89da99,0xabbaa0c0, 0xb8284022,0xa6f2d79e, 0xb81c05e8,0x27847862,
       0x5e54d63,0x337a4b59},
      {0x21f7794a,0x3c67500d, 0x7d6d7f61,0x207005b7, 0x4cfd6e8,0xa5a3781,
       0xf4c2fbd6,0xd65e0d5},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x6d3549cf,0xd433e50f, 0xfacd665e,0x6f33696f, 0xce11fcb4,0x695bfdac,
       0xaf7c9860,0x810ee252},
      {0x7159bb2c,0x65450fe1, 0x758b357b,0xf7dfbebe, 0xd69fea72,0x2b057e74,
       0x92731745,0xd485717a},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xe83f7669,0xce1f69bb, 0x72877d6b,0x9f8ae82, 0x3244278d,0x9548ae54,
       0xe3c2c19c,0x207755de},
      {0x6fef1945,0x87bd61d9, 0xb12d28c3,0x18813cef, 0x72df64aa,0x9fbcd1d6,
       0x7154b00d,0x48dc5ee5},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xf49a3154,0xef0f469e, 0x6e2b2e9a,0x3e85a595, 0xaa924a9c,0x45aaec1e,
       0xa09e4719,0xaa12dfc8},
      {0x4df69f1d,0x26f27227, 0xa2ff5e73,0xe0e4c82c, 0xb7a9dd44,0xb9d8ce73,
       0xe48ca901,0x6c036e73},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xa47153f0,0xe1e421e1, 0x920418c9,0xb86c3b79, 0x705d7672,0x93bdce87,
       0xcab79a77,0xf25ae793},
      {0x6d869d0c,0x1f3194a3, 0x4986c264,0x9d55c882, 0x96e945e,0x49fb5ea3,
       0x13db0a3e,0x39b8e653},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x35d0b34a,0xe3417bc0, 0x8327c0a7,0x440b386b, 0xac0362d1,0x8fb7262d,
       0xe0cdf943,0x2c41114c},
      {0xad95a0b1,0x2ba5cef1, 0x67d54362,0xc09b37a8, 0x1e486c9,0x26d6cdd2,
       0x42ff9297,0x20477abf},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xbc0a67d2,0xf121b41, 0x444d248a,0x62d4760a, 0x659b4737,0xe044f1d,
       0x250bb4a8,0x8fde365},
      {0x848bf287,0xaceec3da, 0xd3369d6e,0xc2a62182, 0x92449482,0x3582dfdc,
       0x565d6cd7,0x2f7e2fd2},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x178a876b,0xa0122b5, 0x85104b4,0x51ff96ff, 0x14f29f76,0x50b31ab,
       0x5f87d4e6,0x84abb28b},
      {0x8270790a,0xd5ed439f, 0x85e3f46b,0x2d6cb59d, 0x6c1e2212,0x75f55c1b,
       0x17655640,0xe5436f67},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x9aeb596d,0xc2965ecc, 0x23c92b4,0x1ea03e7, 0x2e013961,0x4704b4b6,
       0x905ea367,0xca8fd3f},
      {0x551b2b61,0x92523a42, 0x390fcd06,0x1eb7a89c, 0x392a63e,0xe7f1d2be,
       0x4ddb0c33,0x96dca264},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x15339848,0x231c210e, 0x70778c8d,0xe87a28e8, 0x6956e170,0x9d1de661,
       0x2bb09c0b,0x4ac3c938},
      {0x6998987d,0x19be0551, 0xae09f4d6,0x8b2376c4, 0x1a3f933d,0x1de0b765,
       0xe39705f4,0x380d94c7},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x8c31c31d,0x3685954b, 0x5bf21a0c,0x68533d00, 0x75c79ec9,0xbd7626e,
       0x42c69d54,0xca177547},
      {0xf6d2dbb2,0xcc6edaff, 0x174a9d18,0xfd0d8cbd, 0xaa4578e8,0x875e8793,
       0x9cab2ce6,0xa976a713},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0xb43ea1db,0xce37ab11, 0x5259d292,0xa7ff1a9, 0x8f84f186,0x851b0221,
       0xdefaad13,0xa7222bea},
      {0x2b0a9144,0xa2ac78ec, 0xf2fa59c5,0x5a024051, 0x6147ce38,0x91d1eca5,
       0xbc2ac690,0xbe94d523},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}},
     {{0x79ec1a0f,0x2d8daefd, 0xceb39c97,0x3bbcd6fd, 0x58f61a95,0xf5575ffc,
       0xadf7b420,0xdbd986c4},
      {0x15f39eb7,0x81aa8814, 0xb98d976c,0x6ee2fcf5, 0xcf2f717d,0x5465475d,
       0x6860bbd0,0x8e24d3c4},
      {0x1,0x0, 0x0,0xffffffff, 0xffffffff,0xffffffff, 0xfffffffe,0x0}}}};
#endif

// select_point selects the |idx|th point from a precomputation table and
// copies it to out.
static void select_point(const limb_t idx, size_t size,
                         const fe pre_comp[/*size*/][3],
                         fe out[3]) {
  limb_t* outlimbs = &out[0][0];
  for (size_t j = 0; j < NLIMBS * 3; j++) { outlimbs[j] = 0; }
  for (size_t i = 0; i < size; i++) {
    const limb_t *inlimbs = (const limb_t *)&pre_comp[i][0][0];
    limb_t mask = i ^ idx;
    mask |= mask >> 4;
    mask |= mask >> 2;
    mask |= mask >> 1;
    mask &= 1;
    mask--;
    for (size_t j = 0; j < NLIMBS * 3; j++) {
      outlimbs[j] |= inlimbs[j] & mask;
    }
  }
}

// get_bit returns the |i|th bit in |in|
static char get_bit(const uint8_t *in, int i) {
  if (i < 0 || i >= 256) {
    return 0;
  }
  return (in[i >> 3] >> (i & 7)) & 1;
}

// Interleaved point multiplication using precomputed point multiples: The
// small point multiples 0*P, 1*P, ..., 17*P are in p_pre_comp, the scalar
// in p_scalar, if non-NULL. If g_scalar is non-NULL, we also add this multiple
// of the generator, using certain (large) precomputed multiples in g_pre_comp.
// Output point (X, Y, Z) is stored in x_out, y_out, z_out.
static void batch_mul(fe x_out, fe y_out, fe z_out,
                      const uint8_t *p_scalar, const uint8_t *g_scalar,
                      const fe p_pre_comp[17][3]) {
  fe nq[3] = {{0},{0},{0}}, ftmp, tmp[3];
  uint64_t bits;
  uint8_t sign, digit;

  // Loop over both scalars msb-to-lsb, interleaving additions of multiples
  // of the generator (two in each of the last 32 rounds) and additions of p
  // (every 5th round).

  int skip = 1;  // save two point operations in the first round
  size_t i = p_scalar != NULL ? 255 : 31;
  for (;;) {
    // double
    if (!skip) {
      point_double(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2]);
    }

    // add multiples of the generator
    if (g_scalar != NULL && i <= 31) {
      // first, look 32 bits upwards
      bits = get_bit(g_scalar, i + 224) << 3;
      bits |= get_bit(g_scalar, i + 160) << 2;
      bits |= get_bit(g_scalar, i + 96) << 1;
      bits |= get_bit(g_scalar, i + 32);
      // select the point to add, in constant time
      select_point(bits, 16, g_pre_comp[1], tmp);

      if (!skip) {
        point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1 /* mixed */,
                  tmp[0], tmp[1], tmp[2]);
      } else {
        fe_copy(nq[0], tmp[0]);
        fe_copy(nq[1], tmp[1]);
        fe_copy(nq[2], tmp[2]);
        skip = 0;
      }

      // second, look at the current position
      bits = get_bit(g_scalar, i + 192) << 3;
      bits |= get_bit(g_scalar, i + 128) << 2;
      bits |= get_bit(g_scalar, i + 64) << 1;
      bits |= get_bit(g_scalar, i);
      // select the point to add, in constant time
      select_point(bits, 16, g_pre_comp[0], tmp);
      point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1 /* mixed */, tmp[0],
                tmp[1], tmp[2]);
    }

    // do other additions every 5 doublings
    if (p_scalar != NULL && i % 5 == 0) {
      bits = get_bit(p_scalar, i + 4) << 5;
      bits |= get_bit(p_scalar, i + 3) << 4;
      bits |= get_bit(p_scalar, i + 2) << 3;
      bits |= get_bit(p_scalar, i + 1) << 2;
      bits |= get_bit(p_scalar, i) << 1;
      bits |= get_bit(p_scalar, i - 1);
      ec_GFp_nistp_recode_scalar_bits(&sign, &digit, bits);

      // select the point to add or subtract, in constant time.
      select_point(digit, 17, p_pre_comp, tmp);
      fe_opp(ftmp, tmp[1]);  // (X, -Y, Z) is the negative point.
      fe_cmovznz(tmp[1], sign, tmp[1], ftmp);

      if (!skip) {
        point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 0 /* mixed */,
                  tmp[0], tmp[1], tmp[2]);
      } else {
        fe_copy(nq[0], tmp[0]);
        fe_copy(nq[1], tmp[1]);
        fe_copy(nq[2], tmp[2]);
        skip = 0;
      }
    }

    if (i == 0) {
      break;
    }
    --i;
  }
  fe_copy(x_out, nq[0]);
  fe_copy(y_out, nq[1]);
  fe_copy(z_out, nq[2]);
}

// OPENSSL EC_METHOD FUNCTIONS

// Takes the Jacobian coordinates (X, Y, Z) of a point and returns (X', Y') =
// (X/Z^2, Y/Z^3).
static int ec_GFp_nistp256_point_get_affine_coordinates(const EC_GROUP *group,
                                                        const EC_POINT *point,
                                                        BIGNUM *x_out, BIGNUM *y_out,
                                                        BN_CTX *ctx) {
  fe x, y, z1, z2;

  if (EC_POINT_is_at_infinity(group, point)) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
    return 0;
  }
  if (!BN_to_fe(x, &point->X) ||
      !BN_to_fe(y, &point->Y) ||
      !BN_to_fe(z1, &point->Z)) {
    return 0;
  }

  fe_inv(z2, z1);
  fe_sqr(z1, z2);

  if (x_out != NULL) {
    fe_mul(x, x, z1);
    if (!fe_to_BN(x_out, x)) {
      OPENSSL_PUT_ERROR(EC, ERR_R_BN_LIB);
      return 0;
    }
  }

  if (y_out != NULL) {
    fe_mul(z1, z1, z2);
    fe_mul(y, y, z1);
    if (!fe_to_BN(y_out, y)) {
      OPENSSL_PUT_ERROR(EC, ERR_R_BN_LIB);
      return 0;
    }
  }

  return 1;
}

static int ec_GFp_nistp256_points_mul(const EC_GROUP *group, EC_POINT *r,
                                      const BIGNUM *g_scalar, const EC_POINT *p,
                                      const BIGNUM *p_scalar, BN_CTX *ctx) {
  int ret = 0;
  BN_CTX *new_ctx = NULL;
  BIGNUM *x, *y, *z, *tmp_scalar;
  uint8_t g_secret[NBYTES], p_secret[NBYTES], tmp[NBYTES];
  fe p_pre_comp[17][3];
  fe x_out, y_out, z_out;

  if (ctx == NULL) {
    ctx = new_ctx = BN_CTX_new();
    if (ctx == NULL) {
      return 0;
    }
  }

  BN_CTX_start(ctx);
  if ((x = BN_CTX_get(ctx)) == NULL ||
      (y = BN_CTX_get(ctx)) == NULL ||
      (z = BN_CTX_get(ctx)) == NULL ||
      (tmp_scalar = BN_CTX_get(ctx)) == NULL) {
    goto err;
  }

  if (p != NULL && p_scalar != NULL) {
    // We treat NULL scalars as 0, and NULL points as points at infinity, i.e.,
    // they contribute nothing to the linear combination.
    OPENSSL_memset(&p_secret, 0, sizeof(p_secret));
    OPENSSL_memset(&p_pre_comp, 0, sizeof(p_pre_comp));
    size_t num_bytes;
    // Reduce g_scalar to 0 <= g_scalar < 2^256.
    if (BN_num_bits(p_scalar) > 256 || BN_is_negative(p_scalar)) {
      // This is an unusual input, and we don't guarantee constant-timeness.
      if (!BN_nnmod(tmp_scalar, p_scalar, &group->order, ctx)) {
        OPENSSL_PUT_ERROR(EC, ERR_R_BN_LIB);
        goto err;
      }
      num_bytes = BN_bn2bin(tmp_scalar, tmp);
    } else {
      num_bytes = BN_bn2bin(p_scalar, tmp);
    }
    flip_endian(p_secret, tmp, num_bytes);
    // Precompute multiples.
    if (!BN_to_fe(p_pre_comp[1][0], &p->X) ||
        !BN_to_fe(p_pre_comp[1][1], &p->Y) ||
        !BN_to_fe(p_pre_comp[1][2], &p->Z)) {
      goto err;
    }
    for (size_t j = 2; j <= 16; ++j) {
      if (j & 1) {
        point_add(p_pre_comp[j][0], p_pre_comp[j][1],
                        p_pre_comp[j][2], p_pre_comp[1][0],
                        p_pre_comp[1][1], p_pre_comp[1][2],
                        0,
                        p_pre_comp[j - 1][0], p_pre_comp[j - 1][1],
                        p_pre_comp[j - 1][2]);
      } else {
        point_double(p_pre_comp[j][0], p_pre_comp[j][1],
                           p_pre_comp[j][2], p_pre_comp[j / 2][0],
                           p_pre_comp[j / 2][1], p_pre_comp[j / 2][2]);
      }
    }
  }

  if (g_scalar != NULL) {
    size_t num_bytes;

    OPENSSL_memset(g_secret, 0, sizeof(g_secret));
    // reduce g_scalar to 0 <= g_scalar < 2^256
    if (BN_num_bits(g_scalar) > 256 || BN_is_negative(g_scalar)) {
      // this is an unusual input, and we don't guarantee
      // constant-timeness.
      if (!BN_nnmod(tmp_scalar, g_scalar, &group->order, ctx)) {
        OPENSSL_PUT_ERROR(EC, ERR_R_BN_LIB);
        goto err;
      }
      num_bytes = BN_bn2bin(tmp_scalar, tmp);
    } else {
      num_bytes = BN_bn2bin(g_scalar, tmp);
    }
    flip_endian(g_secret, tmp, num_bytes);
  }
  batch_mul(x_out, y_out, z_out,
            (p != NULL && p_scalar != NULL) ? p_secret : NULL,
            g_scalar != NULL ? g_secret : NULL,
            (const fe (*) [3])p_pre_comp);

  if (!fe_to_BN(x, x_out) ||
      !fe_to_BN(y, y_out) ||
      !fe_to_BN(z, z_out)) {
    OPENSSL_PUT_ERROR(EC, ERR_R_BN_LIB);
    goto err;
  }
  ret = ec_point_set_Jprojective_coordinates_GFp(group, r, x, y, z, ctx);

err:
  BN_CTX_end(ctx);
  BN_CTX_free(new_ctx);
  return ret;
}

DEFINE_METHOD_FUNCTION(EC_METHOD, EC_GFp_nistp256_method) {
  out->group_init = ec_GFp_simple_group_init;
  out->group_finish = ec_GFp_simple_group_finish;
  out->group_set_curve = ec_GFp_simple_group_set_curve;
  out->point_get_affine_coordinates =
      ec_GFp_nistp256_point_get_affine_coordinates;
  out->mul = ec_GFp_nistp256_points_mul;
  out->field_mul = ec_GFp_simple_field_mul;
  out->field_sqr = ec_GFp_simple_field_sqr;
  out->field_encode = NULL;
  out->field_decode = NULL;
};

#endif  // (32_BIT || 64_BIT) && !WINDOWS
