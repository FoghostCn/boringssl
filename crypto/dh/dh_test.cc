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

#include <openssl/dh.h>

#include <stdio.h>
#include <string.h>

#include <vector>

#include <openssl/bn.h>
#include <openssl/c++/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "internal.h"
#include "../test/scoped_types.h"

namespace bssl {

static bool RunBasicTests();
static bool RunRFC5114Tests();
static bool TestBadY();
static bool TestASN1();

static int Main() {
  CRYPTO_library_init();

  if (!RunBasicTests() ||
      !RunRFC5114Tests() ||
      !TestBadY() ||
      !TestASN1()) {
    ERR_print_errors_fp(stderr);
    return 1;
  }

  printf("PASS\n");
  return 0;
}

static int GenerateCallback(int p, int n, BN_GENCB *arg) {
  char c = '*';

  if (p == 0) {
    c = '.';
  } else if (p == 1) {
    c = '+';
  } else if (p == 2) {
    c = '*';
  } else if (p == 3) {
    c = '\n';
  }
  FILE *out = reinterpret_cast<FILE*>(arg->arg);
  fputc(c, out);
  fflush(out);

  return 1;
}

static bool RunBasicTests() {
  BN_GENCB cb;
  BN_GENCB_set(&cb, &GenerateCallback, stdout);
  ScopedDH a(DH_new());
  if (!a || !DH_generate_parameters_ex(a.get(), 64, DH_GENERATOR_5, &cb)) {
    return false;
  }

  int check_result;
  if (!DH_check(a.get(), &check_result)) {
    return false;
  }
  if (check_result & DH_CHECK_P_NOT_PRIME) {
    printf("p value is not prime\n");
  }
  if (check_result & DH_CHECK_P_NOT_SAFE_PRIME) {
    printf("p value is not a safe prime\n");
  }
  if (check_result & DH_CHECK_UNABLE_TO_CHECK_GENERATOR) {
    printf("unable to check the generator value\n");
  }
  if (check_result & DH_CHECK_NOT_SUITABLE_GENERATOR) {
    printf("the g value is not a generator\n");
  }

  printf("\np    = ");
  BN_print_fp(stdout, a->p);
  printf("\ng    = ");
  BN_print_fp(stdout, a->g);
  printf("\n");

  ScopedDH b(DH_new());
  if (!b) {
    return false;
  }

  b->p = BN_dup(a->p);
  b->g = BN_dup(a->g);
  if (b->p == nullptr || b->g == nullptr) {
    return false;
  }

  if (!DH_generate_key(a.get())) {
    return false;
  }
  printf("pri1 = ");
  BN_print_fp(stdout, a->priv_key);
  printf("\npub1 = ");
  BN_print_fp(stdout, a->pub_key);
  printf("\n");

  if (!DH_generate_key(b.get())) {
    return false;
  }
  printf("pri2 = ");
  BN_print_fp(stdout, b->priv_key);
  printf("\npub2 = ");
  BN_print_fp(stdout, b->pub_key);
  printf("\n");

  std::vector<uint8_t> key1(DH_size(a.get()));
  int ret = DH_compute_key(key1.data(), b->pub_key, a.get());
  if (ret < 0) {
    return false;
  }
  key1.resize(ret);

  printf("key1 = ");
  for (size_t i = 0; i < key1.size(); i++) {
    printf("%02x", key1[i]);
  }
  printf("\n");

  std::vector<uint8_t> key2(DH_size(b.get()));
  ret = DH_compute_key(key2.data(), a->pub_key, b.get());
  if (ret < 0) {
    return false;
  }
  key2.resize(ret);

  printf("key2 = ");
  for (size_t i = 0; i < key2.size(); i++) {
    printf("%02x", key2[i]);
  }
  printf("\n");

  if (key1.size() < 4 || key1 != key2) {
    fprintf(stderr, "Error in DH routines\n");
    return false;
  }

  return true;
}

/* Test data from RFC 5114 */

static const uint8_t kDHTest1024_160_xA[] = {
    0xB9, 0xA3, 0xB3, 0xAE, 0x8F, 0xEF, 0xC1, 0xA2, 0x93, 0x04,
    0x96, 0x50, 0x70, 0x86, 0xF8, 0x45, 0x5D, 0x48, 0x94, 0x3E};
static const uint8_t kDHTest1024_160_yA[] = {
    0x2A, 0x85, 0x3B, 0x3D, 0x92, 0x19, 0x75, 0x01, 0xB9, 0x01, 0x5B, 0x2D,
    0xEB, 0x3E, 0xD8, 0x4F, 0x5E, 0x02, 0x1D, 0xCC, 0x3E, 0x52, 0xF1, 0x09,
    0xD3, 0x27, 0x3D, 0x2B, 0x75, 0x21, 0x28, 0x1C, 0xBA, 0xBE, 0x0E, 0x76,
    0xFF, 0x57, 0x27, 0xFA, 0x8A, 0xCC, 0xE2, 0x69, 0x56, 0xBA, 0x9A, 0x1F,
    0xCA, 0x26, 0xF2, 0x02, 0x28, 0xD8, 0x69, 0x3F, 0xEB, 0x10, 0x84, 0x1D,
    0x84, 0xA7, 0x36, 0x00, 0x54, 0xEC, 0xE5, 0xA7, 0xF5, 0xB7, 0xA6, 0x1A,
    0xD3, 0xDF, 0xB3, 0xC6, 0x0D, 0x2E, 0x43, 0x10, 0x6D, 0x87, 0x27, 0xDA,
    0x37, 0xDF, 0x9C, 0xCE, 0x95, 0xB4, 0x78, 0x75, 0x5D, 0x06, 0xBC, 0xEA,
    0x8F, 0x9D, 0x45, 0x96, 0x5F, 0x75, 0xA5, 0xF3, 0xD1, 0xDF, 0x37, 0x01,
    0x16, 0x5F, 0xC9, 0xE5, 0x0C, 0x42, 0x79, 0xCE, 0xB0, 0x7F, 0x98, 0x95,
    0x40, 0xAE, 0x96, 0xD5, 0xD8, 0x8E, 0xD7, 0x76};
static const uint8_t kDHTest1024_160_xB[] = {
    0x93, 0x92, 0xC9, 0xF9, 0xEB, 0x6A, 0x7A, 0x6A, 0x90, 0x22,
    0xF7, 0xD8, 0x3E, 0x72, 0x23, 0xC6, 0x83, 0x5B, 0xBD, 0xDA};
static const uint8_t kDHTest1024_160_yB[] = {
    0x71, 0x7A, 0x6C, 0xB0, 0x53, 0x37, 0x1F, 0xF4, 0xA3, 0xB9, 0x32, 0x94,
    0x1C, 0x1E, 0x56, 0x63, 0xF8, 0x61, 0xA1, 0xD6, 0xAD, 0x34, 0xAE, 0x66,
    0x57, 0x6D, 0xFB, 0x98, 0xF6, 0xC6, 0xCB, 0xF9, 0xDD, 0xD5, 0xA5, 0x6C,
    0x78, 0x33, 0xF6, 0xBC, 0xFD, 0xFF, 0x09, 0x55, 0x82, 0xAD, 0x86, 0x8E,
    0x44, 0x0E, 0x8D, 0x09, 0xFD, 0x76, 0x9E, 0x3C, 0xEC, 0xCD, 0xC3, 0xD3,
    0xB1, 0xE4, 0xCF, 0xA0, 0x57, 0x77, 0x6C, 0xAA, 0xF9, 0x73, 0x9B, 0x6A,
    0x9F, 0xEE, 0x8E, 0x74, 0x11, 0xF8, 0xD6, 0xDA, 0xC0, 0x9D, 0x6A, 0x4E,
    0xDB, 0x46, 0xCC, 0x2B, 0x5D, 0x52, 0x03, 0x09, 0x0E, 0xAE, 0x61, 0x26,
    0x31, 0x1E, 0x53, 0xFD, 0x2C, 0x14, 0xB5, 0x74, 0xE6, 0xA3, 0x10, 0x9A,
    0x3D, 0xA1, 0xBE, 0x41, 0xBD, 0xCE, 0xAA, 0x18, 0x6F, 0x5C, 0xE0, 0x67,
    0x16, 0xA2, 0xB6, 0xA0, 0x7B, 0x3C, 0x33, 0xFE};
static const uint8_t kDHTest1024_160_Z[] = {
    0x5C, 0x80, 0x4F, 0x45, 0x4D, 0x30, 0xD9, 0xC4, 0xDF, 0x85, 0x27, 0x1F,
    0x93, 0x52, 0x8C, 0x91, 0xDF, 0x6B, 0x48, 0xAB, 0x5F, 0x80, 0xB3, 0xB5,
    0x9C, 0xAA, 0xC1, 0xB2, 0x8F, 0x8A, 0xCB, 0xA9, 0xCD, 0x3E, 0x39, 0xF3,
    0xCB, 0x61, 0x45, 0x25, 0xD9, 0x52, 0x1D, 0x2E, 0x64, 0x4C, 0x53, 0xB8,
    0x07, 0xB8, 0x10, 0xF3, 0x40, 0x06, 0x2F, 0x25, 0x7D, 0x7D, 0x6F, 0xBF,
    0xE8, 0xD5, 0xE8, 0xF0, 0x72, 0xE9, 0xB6, 0xE9, 0xAF, 0xDA, 0x94, 0x13,
    0xEA, 0xFB, 0x2E, 0x8B, 0x06, 0x99, 0xB1, 0xFB, 0x5A, 0x0C, 0xAC, 0xED,
    0xDE, 0xAE, 0xAD, 0x7E, 0x9C, 0xFB, 0xB3, 0x6A, 0xE2, 0xB4, 0x20, 0x83,
    0x5B, 0xD8, 0x3A, 0x19, 0xFB, 0x0B, 0x5E, 0x96, 0xBF, 0x8F, 0xA4, 0xD0,
    0x9E, 0x34, 0x55, 0x25, 0x16, 0x7E, 0xCD, 0x91, 0x55, 0x41, 0x6F, 0x46,
    0xF4, 0x08, 0xED, 0x31, 0xB6, 0x3C, 0x6E, 0x6D};
static const uint8_t kDHTest2048_224_xA[] = {
    0x22, 0xE6, 0x26, 0x01, 0xDB, 0xFF, 0xD0, 0x67, 0x08, 0xA6,
    0x80, 0xF7, 0x47, 0xF3, 0x61, 0xF7, 0x6D, 0x8F, 0x4F, 0x72,
    0x1A, 0x05, 0x48, 0xE4, 0x83, 0x29, 0x4B, 0x0C};
static const uint8_t kDHTest2048_224_yA[] = {
    0x1B, 0x3A, 0x63, 0x45, 0x1B, 0xD8, 0x86, 0xE6, 0x99, 0xE6, 0x7B, 0x49,
    0x4E, 0x28, 0x8B, 0xD7, 0xF8, 0xE0, 0xD3, 0x70, 0xBA, 0xDD, 0xA7, 0xA0,
    0xEF, 0xD2, 0xFD, 0xE7, 0xD8, 0xF6, 0x61, 0x45, 0xCC, 0x9F, 0x28, 0x04,
    0x19, 0x97, 0x5E, 0xB8, 0x08, 0x87, 0x7C, 0x8A, 0x4C, 0x0C, 0x8E, 0x0B,
    0xD4, 0x8D, 0x4A, 0x54, 0x01, 0xEB, 0x1E, 0x87, 0x76, 0xBF, 0xEE, 0xE1,
    0x34, 0xC0, 0x38, 0x31, 0xAC, 0x27, 0x3C, 0xD9, 0xD6, 0x35, 0xAB, 0x0C,
    0xE0, 0x06, 0xA4, 0x2A, 0x88, 0x7E, 0x3F, 0x52, 0xFB, 0x87, 0x66, 0xB6,
    0x50, 0xF3, 0x80, 0x78, 0xBC, 0x8E, 0xE8, 0x58, 0x0C, 0xEF, 0xE2, 0x43,
    0x96, 0x8C, 0xFC, 0x4F, 0x8D, 0xC3, 0xDB, 0x08, 0x45, 0x54, 0x17, 0x1D,
    0x41, 0xBF, 0x2E, 0x86, 0x1B, 0x7B, 0xB4, 0xD6, 0x9D, 0xD0, 0xE0, 0x1E,
    0xA3, 0x87, 0xCB, 0xAA, 0x5C, 0xA6, 0x72, 0xAF, 0xCB, 0xE8, 0xBD, 0xB9,
    0xD6, 0x2D, 0x4C, 0xE1, 0x5F, 0x17, 0xDD, 0x36, 0xF9, 0x1E, 0xD1, 0xEE,
    0xDD, 0x65, 0xCA, 0x4A, 0x06, 0x45, 0x5C, 0xB9, 0x4C, 0xD4, 0x0A, 0x52,
    0xEC, 0x36, 0x0E, 0x84, 0xB3, 0xC9, 0x26, 0xE2, 0x2C, 0x43, 0x80, 0xA3,
    0xBF, 0x30, 0x9D, 0x56, 0x84, 0x97, 0x68, 0xB7, 0xF5, 0x2C, 0xFD, 0xF6,
    0x55, 0xFD, 0x05, 0x3A, 0x7E, 0xF7, 0x06, 0x97, 0x9E, 0x7E, 0x58, 0x06,
    0xB1, 0x7D, 0xFA, 0xE5, 0x3A, 0xD2, 0xA5, 0xBC, 0x56, 0x8E, 0xBB, 0x52,
    0x9A, 0x7A, 0x61, 0xD6, 0x8D, 0x25, 0x6F, 0x8F, 0xC9, 0x7C, 0x07, 0x4A,
    0x86, 0x1D, 0x82, 0x7E, 0x2E, 0xBC, 0x8C, 0x61, 0x34, 0x55, 0x31, 0x15,
    0xB7, 0x0E, 0x71, 0x03, 0x92, 0x0A, 0xA1, 0x6D, 0x85, 0xE5, 0x2B, 0xCB,
    0xAB, 0x8D, 0x78, 0x6A, 0x68, 0x17, 0x8F, 0xA8, 0xFF, 0x7C, 0x2F, 0x5C,
    0x71, 0x64, 0x8D, 0x6F};
static const uint8_t kDHTest2048_224_xB[] = {
    0x4F, 0xF3, 0xBC, 0x96, 0xC7, 0xFC, 0x6A, 0x6D, 0x71, 0xD3,
    0xB3, 0x63, 0x80, 0x0A, 0x7C, 0xDF, 0xEF, 0x6F, 0xC4, 0x1B,
    0x44, 0x17, 0xEA, 0x15, 0x35, 0x3B, 0x75, 0x90};
static const uint8_t kDHTest2048_224_yB[] = {
    0x4D, 0xCE, 0xE9, 0x92, 0xA9, 0x76, 0x2A, 0x13, 0xF2, 0xF8, 0x38, 0x44,
    0xAD, 0x3D, 0x77, 0xEE, 0x0E, 0x31, 0xC9, 0x71, 0x8B, 0x3D, 0xB6, 0xC2,
    0x03, 0x5D, 0x39, 0x61, 0x18, 0x2C, 0x3E, 0x0B, 0xA2, 0x47, 0xEC, 0x41,
    0x82, 0xD7, 0x60, 0xCD, 0x48, 0xD9, 0x95, 0x99, 0x97, 0x06, 0x22, 0xA1,
    0x88, 0x1B, 0xBA, 0x2D, 0xC8, 0x22, 0x93, 0x9C, 0x78, 0xC3, 0x91, 0x2C,
    0x66, 0x61, 0xFA, 0x54, 0x38, 0xB2, 0x07, 0x66, 0x22, 0x2B, 0x75, 0xE2,
    0x4C, 0x2E, 0x3A, 0xD0, 0xC7, 0x28, 0x72, 0x36, 0x12, 0x95, 0x25, 0xEE,
    0x15, 0xB5, 0xDD, 0x79, 0x98, 0xAA, 0x04, 0xC4, 0xA9, 0x69, 0x6C, 0xAC,
    0xD7, 0x17, 0x20, 0x83, 0xA9, 0x7A, 0x81, 0x66, 0x4E, 0xAD, 0x2C, 0x47,
    0x9E, 0x44, 0x4E, 0x4C, 0x06, 0x54, 0xCC, 0x19, 0xE2, 0x8D, 0x77, 0x03,
    0xCE, 0xE8, 0xDA, 0xCD, 0x61, 0x26, 0xF5, 0xD6, 0x65, 0xEC, 0x52, 0xC6,
    0x72, 0x55, 0xDB, 0x92, 0x01, 0x4B, 0x03, 0x7E, 0xB6, 0x21, 0xA2, 0xAC,
    0x8E, 0x36, 0x5D, 0xE0, 0x71, 0xFF, 0xC1, 0x40, 0x0A, 0xCF, 0x07, 0x7A,
    0x12, 0x91, 0x3D, 0xD8, 0xDE, 0x89, 0x47, 0x34, 0x37, 0xAB, 0x7B, 0xA3,
    0x46, 0x74, 0x3C, 0x1B, 0x21, 0x5D, 0xD9, 0xC1, 0x21, 0x64, 0xA7, 0xE4,
    0x05, 0x31, 0x18, 0xD1, 0x99, 0xBE, 0xC8, 0xEF, 0x6F, 0xC5, 0x61, 0x17,
    0x0C, 0x84, 0xC8, 0x7D, 0x10, 0xEE, 0x9A, 0x67, 0x4A, 0x1F, 0xA8, 0xFF,
    0xE1, 0x3B, 0xDF, 0xBA, 0x1D, 0x44, 0xDE, 0x48, 0x94, 0x6D, 0x68, 0xDC,
    0x0C, 0xDD, 0x77, 0x76, 0x35, 0xA7, 0xAB, 0x5B, 0xFB, 0x1E, 0x4B, 0xB7,
    0xB8, 0x56, 0xF9, 0x68, 0x27, 0x73, 0x4C, 0x18, 0x41, 0x38, 0xE9, 0x15,
    0xD9, 0xC3, 0x00, 0x2E, 0xBC, 0xE5, 0x31, 0x20, 0x54, 0x6A, 0x7E, 0x20,
    0x02, 0x14, 0x2B, 0x6C};
static const uint8_t kDHTest2048_224_Z[] = {
    0x34, 0xD9, 0xBD, 0xDC, 0x1B, 0x42, 0x17, 0x6C, 0x31, 0x3F, 0xEA, 0x03,
    0x4C, 0x21, 0x03, 0x4D, 0x07, 0x4A, 0x63, 0x13, 0xBB, 0x4E, 0xCD, 0xB3,
    0x70, 0x3F, 0xFF, 0x42, 0x45, 0x67, 0xA4, 0x6B, 0xDF, 0x75, 0x53, 0x0E,
    0xDE, 0x0A, 0x9D, 0xA5, 0x22, 0x9D, 0xE7, 0xD7, 0x67, 0x32, 0x28, 0x6C,
    0xBC, 0x0F, 0x91, 0xDA, 0x4C, 0x3C, 0x85, 0x2F, 0xC0, 0x99, 0xC6, 0x79,
    0x53, 0x1D, 0x94, 0xC7, 0x8A, 0xB0, 0x3D, 0x9D, 0xEC, 0xB0, 0xA4, 0xE4,
    0xCA, 0x8B, 0x2B, 0xB4, 0x59, 0x1C, 0x40, 0x21, 0xCF, 0x8C, 0xE3, 0xA2,
    0x0A, 0x54, 0x1D, 0x33, 0x99, 0x40, 0x17, 0xD0, 0x20, 0x0A, 0xE2, 0xC9,
    0x51, 0x6E, 0x2F, 0xF5, 0x14, 0x57, 0x79, 0x26, 0x9E, 0x86, 0x2B, 0x0F,
    0xB4, 0x74, 0xA2, 0xD5, 0x6D, 0xC3, 0x1E, 0xD5, 0x69, 0xA7, 0x70, 0x0B,
    0x4C, 0x4A, 0xB1, 0x6B, 0x22, 0xA4, 0x55, 0x13, 0x53, 0x1E, 0xF5, 0x23,
    0xD7, 0x12, 0x12, 0x07, 0x7B, 0x5A, 0x16, 0x9B, 0xDE, 0xFF, 0xAD, 0x7A,
    0xD9, 0x60, 0x82, 0x84, 0xC7, 0x79, 0x5B, 0x6D, 0x5A, 0x51, 0x83, 0xB8,
    0x70, 0x66, 0xDE, 0x17, 0xD8, 0xD6, 0x71, 0xC9, 0xEB, 0xD8, 0xEC, 0x89,
    0x54, 0x4D, 0x45, 0xEC, 0x06, 0x15, 0x93, 0xD4, 0x42, 0xC6, 0x2A, 0xB9,
    0xCE, 0x3B, 0x1C, 0xB9, 0x94, 0x3A, 0x1D, 0x23, 0xA5, 0xEA, 0x3B, 0xCF,
    0x21, 0xA0, 0x14, 0x71, 0xE6, 0x7E, 0x00, 0x3E, 0x7F, 0x8A, 0x69, 0xC7,
    0x28, 0xBE, 0x49, 0x0B, 0x2F, 0xC8, 0x8C, 0xFE, 0xB9, 0x2D, 0xB6, 0xA2,
    0x15, 0xE5, 0xD0, 0x3C, 0x17, 0xC4, 0x64, 0xC9, 0xAC, 0x1A, 0x46, 0xE2,
    0x03, 0xE1, 0x3F, 0x95, 0x29, 0x95, 0xFB, 0x03, 0xC6, 0x9D, 0x3C, 0xC4,
    0x7F, 0xCB, 0x51, 0x0B, 0x69, 0x98, 0xFF, 0xD3, 0xAA, 0x6D, 0xE7, 0x3C,
    0xF9, 0xF6, 0x38, 0x69};
static const uint8_t kDHTest2048_256_xA[] = {
    0x08, 0x81, 0x38, 0x2C, 0xDB, 0x87, 0x66, 0x0C, 0x6D, 0xC1, 0x3E,
    0x61, 0x49, 0x38, 0xD5, 0xB9, 0xC8, 0xB2, 0xF2, 0x48, 0x58, 0x1C,
    0xC5, 0xE3, 0x1B, 0x35, 0x45, 0x43, 0x97, 0xFC, 0xE5, 0x0E};
static const uint8_t kDHTest2048_256_yA[] = {
    0x2E, 0x93, 0x80, 0xC8, 0x32, 0x3A, 0xF9, 0x75, 0x45, 0xBC, 0x49, 0x41,
    0xDE, 0xB0, 0xEC, 0x37, 0x42, 0xC6, 0x2F, 0xE0, 0xEC, 0xE8, 0x24, 0xA6,
    0xAB, 0xDB, 0xE6, 0x6C, 0x59, 0xBE, 0xE0, 0x24, 0x29, 0x11, 0xBF, 0xB9,
    0x67, 0x23, 0x5C, 0xEB, 0xA3, 0x5A, 0xE1, 0x3E, 0x4E, 0xC7, 0x52, 0xBE,
    0x63, 0x0B, 0x92, 0xDC, 0x4B, 0xDE, 0x28, 0x47, 0xA9, 0xC6, 0x2C, 0xB8,
    0x15, 0x27, 0x45, 0x42, 0x1F, 0xB7, 0xEB, 0x60, 0xA6, 0x3C, 0x0F, 0xE9,
    0x15, 0x9F, 0xCC, 0xE7, 0x26, 0xCE, 0x7C, 0xD8, 0x52, 0x3D, 0x74, 0x50,
    0x66, 0x7E, 0xF8, 0x40, 0xE4, 0x91, 0x91, 0x21, 0xEB, 0x5F, 0x01, 0xC8,
    0xC9, 0xB0, 0xD3, 0xD6, 0x48, 0xA9, 0x3B, 0xFB, 0x75, 0x68, 0x9E, 0x82,
    0x44, 0xAC, 0x13, 0x4A, 0xF5, 0x44, 0x71, 0x1C, 0xE7, 0x9A, 0x02, 0xDC,
    0xC3, 0x42, 0x26, 0x68, 0x47, 0x80, 0xDD, 0xDC, 0xB4, 0x98, 0x59, 0x41,
    0x06, 0xC3, 0x7F, 0x5B, 0xC7, 0x98, 0x56, 0x48, 0x7A, 0xF5, 0xAB, 0x02,
    0x2A, 0x2E, 0x5E, 0x42, 0xF0, 0x98, 0x97, 0xC1, 0xA8, 0x5A, 0x11, 0xEA,
    0x02, 0x12, 0xAF, 0x04, 0xD9, 0xB4, 0xCE, 0xBC, 0x93, 0x7C, 0x3C, 0x1A,
    0x3E, 0x15, 0xA8, 0xA0, 0x34, 0x2E, 0x33, 0x76, 0x15, 0xC8, 0x4E, 0x7F,
    0xE3, 0xB8, 0xB9, 0xB8, 0x7F, 0xB1, 0xE7, 0x3A, 0x15, 0xAF, 0x12, 0xA3,
    0x0D, 0x74, 0x6E, 0x06, 0xDF, 0xC3, 0x4F, 0x29, 0x0D, 0x79, 0x7C, 0xE5,
    0x1A, 0xA1, 0x3A, 0xA7, 0x85, 0xBF, 0x66, 0x58, 0xAF, 0xF5, 0xE4, 0xB0,
    0x93, 0x00, 0x3C, 0xBE, 0xAF, 0x66, 0x5B, 0x3C, 0x2E, 0x11, 0x3A, 0x3A,
    0x4E, 0x90, 0x52, 0x69, 0x34, 0x1D, 0xC0, 0x71, 0x14, 0x26, 0x68, 0x5F,
    0x4E, 0xF3, 0x7E, 0x86, 0x8A, 0x81, 0x26, 0xFF, 0x3F, 0x22, 0x79, 0xB5,
    0x7C, 0xA6, 0x7E, 0x29};
static const uint8_t kDHTest2048_256_xB[] = {
    0x7D, 0x62, 0xA7, 0xE3, 0xEF, 0x36, 0xDE, 0x61, 0x7B, 0x13, 0xD1,
    0xAF, 0xB8, 0x2C, 0x78, 0x0D, 0x83, 0xA2, 0x3B, 0xD4, 0xEE, 0x67,
    0x05, 0x64, 0x51, 0x21, 0xF3, 0x71, 0xF5, 0x46, 0xA5, 0x3D};
static const uint8_t kDHTest2048_256_yB[] = {
    0x57, 0x5F, 0x03, 0x51, 0xBD, 0x2B, 0x1B, 0x81, 0x74, 0x48, 0xBD, 0xF8,
    0x7A, 0x6C, 0x36, 0x2C, 0x1E, 0x28, 0x9D, 0x39, 0x03, 0xA3, 0x0B, 0x98,
    0x32, 0xC5, 0x74, 0x1F, 0xA2, 0x50, 0x36, 0x3E, 0x7A, 0xCB, 0xC7, 0xF7,
    0x7F, 0x3D, 0xAC, 0xBC, 0x1F, 0x13, 0x1A, 0xDD, 0x8E, 0x03, 0x36, 0x7E,
    0xFF, 0x8F, 0xBB, 0xB3, 0xE1, 0xC5, 0x78, 0x44, 0x24, 0x80, 0x9B, 0x25,
    0xAF, 0xE4, 0xD2, 0x26, 0x2A, 0x1A, 0x6F, 0xD2, 0xFA, 0xB6, 0x41, 0x05,
    0xCA, 0x30, 0xA6, 0x74, 0xE0, 0x7F, 0x78, 0x09, 0x85, 0x20, 0x88, 0x63,
    0x2F, 0xC0, 0x49, 0x23, 0x37, 0x91, 0xAD, 0x4E, 0xDD, 0x08, 0x3A, 0x97,
    0x8B, 0x88, 0x3E, 0xE6, 0x18, 0xBC, 0x5E, 0x0D, 0xD0, 0x47, 0x41, 0x5F,
    0x2D, 0x95, 0xE6, 0x83, 0xCF, 0x14, 0x82, 0x6B, 0x5F, 0xBE, 0x10, 0xD3,
    0xCE, 0x41, 0xC6, 0xC1, 0x20, 0xC7, 0x8A, 0xB2, 0x00, 0x08, 0xC6, 0x98,
    0xBF, 0x7F, 0x0B, 0xCA, 0xB9, 0xD7, 0xF4, 0x07, 0xBE, 0xD0, 0xF4, 0x3A,
    0xFB, 0x29, 0x70, 0xF5, 0x7F, 0x8D, 0x12, 0x04, 0x39, 0x63, 0xE6, 0x6D,
    0xDD, 0x32, 0x0D, 0x59, 0x9A, 0xD9, 0x93, 0x6C, 0x8F, 0x44, 0x13, 0x7C,
    0x08, 0xB1, 0x80, 0xEC, 0x5E, 0x98, 0x5C, 0xEB, 0xE1, 0x86, 0xF3, 0xD5,
    0x49, 0x67, 0x7E, 0x80, 0x60, 0x73, 0x31, 0xEE, 0x17, 0xAF, 0x33, 0x80,
    0xA7, 0x25, 0xB0, 0x78, 0x23, 0x17, 0xD7, 0xDD, 0x43, 0xF5, 0x9D, 0x7A,
    0xF9, 0x56, 0x8A, 0x9B, 0xB6, 0x3A, 0x84, 0xD3, 0x65, 0xF9, 0x22, 0x44,
    0xED, 0x12, 0x09, 0x88, 0x21, 0x93, 0x02, 0xF4, 0x29, 0x24, 0xC7, 0xCA,
    0x90, 0xB8, 0x9D, 0x24, 0xF7, 0x1B, 0x0A, 0xB6, 0x97, 0x82, 0x3D, 0x7D,
    0xEB, 0x1A, 0xFF, 0x5B, 0x0E, 0x8E, 0x4A, 0x45, 0xD4, 0x9F, 0x7F, 0x53,
    0x75, 0x7E, 0x19, 0x13};
static const uint8_t kDHTest2048_256_Z[] = {
    0x86, 0xC7, 0x0B, 0xF8, 0xD0, 0xBB, 0x81, 0xBB, 0x01, 0x07, 0x8A, 0x17,
    0x21, 0x9C, 0xB7, 0xD2, 0x72, 0x03, 0xDB, 0x2A, 0x19, 0xC8, 0x77, 0xF1,
    0xD1, 0xF1, 0x9F, 0xD7, 0xD7, 0x7E, 0xF2, 0x25, 0x46, 0xA6, 0x8F, 0x00,
    0x5A, 0xD5, 0x2D, 0xC8, 0x45, 0x53, 0xB7, 0x8F, 0xC6, 0x03, 0x30, 0xBE,
    0x51, 0xEA, 0x7C, 0x06, 0x72, 0xCA, 0xC1, 0x51, 0x5E, 0x4B, 0x35, 0xC0,
    0x47, 0xB9, 0xA5, 0x51, 0xB8, 0x8F, 0x39, 0xDC, 0x26, 0xDA, 0x14, 0xA0,
    0x9E, 0xF7, 0x47, 0x74, 0xD4, 0x7C, 0x76, 0x2D, 0xD1, 0x77, 0xF9, 0xED,
    0x5B, 0xC2, 0xF1, 0x1E, 0x52, 0xC8, 0x79, 0xBD, 0x95, 0x09, 0x85, 0x04,
    0xCD, 0x9E, 0xEC, 0xD8, 0xA8, 0xF9, 0xB3, 0xEF, 0xBD, 0x1F, 0x00, 0x8A,
    0xC5, 0x85, 0x30, 0x97, 0xD9, 0xD1, 0x83, 0x7F, 0x2B, 0x18, 0xF7, 0x7C,
    0xD7, 0xBE, 0x01, 0xAF, 0x80, 0xA7, 0xC7, 0xB5, 0xEA, 0x3C, 0xA5, 0x4C,
    0xC0, 0x2D, 0x0C, 0x11, 0x6F, 0xEE, 0x3F, 0x95, 0xBB, 0x87, 0x39, 0x93,
    0x85, 0x87, 0x5D, 0x7E, 0x86, 0x74, 0x7E, 0x67, 0x6E, 0x72, 0x89, 0x38,
    0xAC, 0xBF, 0xF7, 0x09, 0x8E, 0x05, 0xBE, 0x4D, 0xCF, 0xB2, 0x40, 0x52,
    0xB8, 0x3A, 0xEF, 0xFB, 0x14, 0x78, 0x3F, 0x02, 0x9A, 0xDB, 0xDE, 0x7F,
    0x53, 0xFA, 0xE9, 0x20, 0x84, 0x22, 0x40, 0x90, 0xE0, 0x07, 0xCE, 0xE9,
    0x4D, 0x4B, 0xF2, 0xBA, 0xCE, 0x9F, 0xFD, 0x4B, 0x57, 0xD2, 0xAF, 0x7C,
    0x72, 0x4D, 0x0C, 0xAA, 0x19, 0xBF, 0x05, 0x01, 0xF6, 0xF1, 0x7B, 0x4A,
    0xA1, 0x0F, 0x42, 0x5E, 0x3E, 0xA7, 0x60, 0x80, 0xB4, 0xB9, 0xD6, 0xB3,
    0xCE, 0xFE, 0xA1, 0x15, 0xB2, 0xCE, 0xB8, 0x78, 0x9B, 0xB8, 0xA3, 0xB0,
    0xEA, 0x87, 0xFE, 0xBE, 0x63, 0xB6, 0xC8, 0xF8, 0x46, 0xEC, 0x6D, 0xB0,
    0xC2, 0x6C, 0x5D, 0x7C};

struct RFC5114TestData {
  DH *(*get_param)(const ENGINE *engine);
  const uint8_t *xA;
  size_t xA_len;
  const uint8_t *yA;
  size_t yA_len;
  const uint8_t *xB;
  size_t xB_len;
  const uint8_t *yB;
  size_t yB_len;
  const uint8_t *Z;
  size_t Z_len;
};

#define MAKE_RFC5114_TEST_DATA(pre)                                           \
  {                                                                           \
    DH_get_##pre, kDHTest##pre##_xA, sizeof(kDHTest##pre##_xA),               \
        kDHTest##pre##_yA, sizeof(kDHTest##pre##_yA), kDHTest##pre##_xB,      \
        sizeof(kDHTest##pre##_xB), kDHTest##pre##_yB,                         \
        sizeof(kDHTest##pre##_yB), kDHTest##pre##_Z, sizeof(kDHTest##pre##_Z) \
  }

static const RFC5114TestData kRFCTestData[] = {
    MAKE_RFC5114_TEST_DATA(1024_160),
    MAKE_RFC5114_TEST_DATA(2048_224),
    MAKE_RFC5114_TEST_DATA(2048_256),
 };

static bool RunRFC5114Tests() {
  for (unsigned i = 0; i < sizeof(kRFCTestData) / sizeof(RFC5114TestData); i++) {
    const RFC5114TestData *td = kRFCTestData + i;
    /* Set up DH structures setting key components */
    ScopedDH dhA(td->get_param(nullptr));
    ScopedDH dhB(td->get_param(nullptr));
    if (!dhA || !dhB) {
      fprintf(stderr, "Initialisation error RFC5114 set %u\n", i + 1);
      return false;
    }

    dhA->priv_key = BN_bin2bn(td->xA, td->xA_len, nullptr);
    dhA->pub_key = BN_bin2bn(td->yA, td->yA_len, nullptr);

    dhB->priv_key = BN_bin2bn(td->xB, td->xB_len, nullptr);
    dhB->pub_key = BN_bin2bn(td->yB, td->yB_len, nullptr);

    if (!dhA->priv_key || !dhA->pub_key || !dhB->priv_key || !dhB->pub_key) {
      fprintf(stderr, "BN_bin2bn error RFC5114 set %u\n", i + 1);
      return false;
    }

    if ((td->Z_len != (size_t)DH_size(dhA.get())) ||
        (td->Z_len != (size_t)DH_size(dhB.get()))) {
      return false;
    }

    std::vector<uint8_t> Z1(DH_size(dhA.get()));
    std::vector<uint8_t> Z2(DH_size(dhB.get()));
    /* Work out shared secrets using both sides and compare
     * with expected values. */
    int ret1 = DH_compute_key(Z1.data(), dhB->pub_key, dhA.get());
    int ret2 = DH_compute_key(Z2.data(), dhA->pub_key, dhB.get());
    if (ret1 < 0 || ret2 < 0) {
      fprintf(stderr, "DH_compute_key error RFC5114 set %u\n", i + 1);
      return false;
    }

    if (static_cast<size_t>(ret1) != td->Z_len ||
        memcmp(Z1.data(), td->Z, td->Z_len) != 0 ||
        static_cast<size_t>(ret2) != td->Z_len ||
        memcmp(Z2.data(), td->Z, td->Z_len) != 0) {
      fprintf(stderr, "Test failed RFC5114 set %u\n", i + 1);
      return false;
    }

    printf("RFC5114 parameter test %u OK\n", i + 1);
  }

  return 1;
}

// kRFC5114_2048_224BadY is a bad y-coordinate for RFC 5114's 2048-bit MODP
// Group with 224-bit Prime Order Subgroup (section 2.2).
static const uint8_t kRFC5114_2048_224BadY[] = {
    0x45, 0x32, 0x5f, 0x51, 0x07, 0xe5, 0xdf, 0x1c, 0xd6, 0x02, 0x82, 0xb3,
    0x32, 0x8f, 0xa4, 0x0f, 0x87, 0xb8, 0x41, 0xfe, 0xb9, 0x35, 0xde, 0xad,
    0xc6, 0x26, 0x85, 0xb4, 0xff, 0x94, 0x8c, 0x12, 0x4c, 0xbf, 0x5b, 0x20,
    0xc4, 0x46, 0xa3, 0x26, 0xeb, 0xa4, 0x25, 0xb7, 0x68, 0x8e, 0xcc, 0x67,
    0xba, 0xea, 0x58, 0xd0, 0xf2, 0xe9, 0xd2, 0x24, 0x72, 0x60, 0xda, 0x88,
    0x18, 0x9c, 0xe0, 0x31, 0x6a, 0xad, 0x50, 0x6d, 0x94, 0x35, 0x8b, 0x83,
    0x4a, 0x6e, 0xfa, 0x48, 0x73, 0x0f, 0x83, 0x87, 0xff, 0x6b, 0x66, 0x1f,
    0xa8, 0x82, 0xc6, 0x01, 0xe5, 0x80, 0xb5, 0xb0, 0x52, 0xd0, 0xe9, 0xd8,
    0x72, 0xf9, 0x7d, 0x5b, 0x8b, 0xa5, 0x4c, 0xa5, 0x25, 0x95, 0x74, 0xe2,
    0x7a, 0x61, 0x4e, 0xa7, 0x8f, 0x12, 0xe2, 0xd2, 0x9d, 0x8c, 0x02, 0x70,
    0x34, 0x44, 0x32, 0xc7, 0xb2, 0xf3, 0xb9, 0xfe, 0x17, 0x2b, 0xd6, 0x1f,
    0x8b, 0x7e, 0x4a, 0xfa, 0xa3, 0xb5, 0x3e, 0x7a, 0x81, 0x9a, 0x33, 0x66,
    0x62, 0xa4, 0x50, 0x18, 0x3e, 0xa2, 0x5f, 0x00, 0x07, 0xd8, 0x9b, 0x22,
    0xe4, 0xec, 0x84, 0xd5, 0xeb, 0x5a, 0xf3, 0x2a, 0x31, 0x23, 0xd8, 0x44,
    0x22, 0x2a, 0x8b, 0x37, 0x44, 0xcc, 0xc6, 0x87, 0x4b, 0xbe, 0x50, 0x9d,
    0x4a, 0xc4, 0x8e, 0x45, 0xcf, 0x72, 0x4d, 0xc0, 0x89, 0xb3, 0x72, 0xed,
    0x33, 0x2c, 0xbc, 0x7f, 0x16, 0x39, 0x3b, 0xeb, 0xd2, 0xdd, 0xa8, 0x01,
    0x73, 0x84, 0x62, 0xb9, 0x29, 0xd2, 0xc9, 0x51, 0x32, 0x9e, 0x7a, 0x6a,
    0xcf, 0xc1, 0x0a, 0xdb, 0x0e, 0xe0, 0x62, 0x77, 0x6f, 0x59, 0x62, 0x72,
    0x5a, 0x69, 0xa6, 0x5b, 0x70, 0xca, 0x65, 0xc4, 0x95, 0x6f, 0x9a, 0xc2,
    0xdf, 0x72, 0x6d, 0xb1, 0x1e, 0x54, 0x7b, 0x51, 0xb4, 0xef, 0x7f, 0x89,
    0x93, 0x74, 0x89, 0x59,
};

static bool TestBadY() {
  ScopedDH dh(DH_get_2048_224(nullptr));
  ScopedBIGNUM pub_key(
      BN_bin2bn(kRFC5114_2048_224BadY, sizeof(kRFC5114_2048_224BadY), nullptr));
  if (!dh || !pub_key || !DH_generate_key(dh.get())) {
    return false;
  }

  int flags;
  if (!DH_check_pub_key(dh.get(), pub_key.get(), &flags)) {
    return false;
  }
  if (!(flags & DH_CHECK_PUBKEY_INVALID)) {
    fprintf(stderr, "DH_check_pub_key did not reject the key.\n");
    return false;
  }

  std::vector<uint8_t> result(DH_size(dh.get()));
  if (DH_compute_key(result.data(), pub_key.get(), dh.get()) >= 0) {
    fprintf(stderr, "DH_compute_key unexpectedly succeeded.\n");
    return false;
  }
  ERR_clear_error();

  return true;
}

static bool BIGNUMEqualsHex(const BIGNUM *bn, const char *hex) {
  BIGNUM *hex_bn = NULL;
  if (!BN_hex2bn(&hex_bn, hex)) {
    return false;
  }
  ScopedBIGNUM free_hex_bn(hex_bn);
  return BN_cmp(bn, hex_bn) == 0;
}

static bool TestASN1() {
  // kParams are a set of Diffie-Hellman parameters generated with
  // openssl dhparam 256
  static const uint8_t kParams[] = {
      0x30, 0x26, 0x02, 0x21, 0x00, 0xd7, 0x20, 0x34, 0xa3, 0x27,
      0x4f, 0xdf, 0xbf, 0x04, 0xfd, 0x24, 0x68, 0x25, 0xb6, 0x56,
      0xd8, 0xab, 0x2a, 0x41, 0x2d, 0x74, 0x0a, 0x52, 0x08, 0x7c,
      0x40, 0x71, 0x4e, 0xd2, 0x57, 0x93, 0x13, 0x02, 0x01, 0x02,
  };

  CBS cbs;
  CBS_init(&cbs, kParams, sizeof(kParams));
  ScopedDH dh(DH_parse_parameters(&cbs));
  if (!dh || CBS_len(&cbs) != 0 ||
      !BIGNUMEqualsHex(
          dh->p,
          "d72034a3274fdfbf04fd246825b656d8ab2a412d740a52087c40714ed2579313") ||
      !BIGNUMEqualsHex(dh->g, "2") || dh->priv_length != 0) {
    return false;
  }

  ScopedCBB cbb;
  uint8_t *der;
  size_t der_len;
  if (!CBB_init(cbb.get(), 0) ||
      !DH_marshal_parameters(cbb.get(), dh.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  ScopedOpenSSLBytes free_der(der);
  if (der_len != sizeof(kParams) || memcmp(der, kParams, der_len) != 0) {
    return false;
  }

  // kParamsDSA are a set of Diffie-Hellman parameters generated with
  // openssl dhparam 256 -dsaparam
  static const uint8_t kParamsDSA[] = {
      0x30, 0x81, 0x89, 0x02, 0x41, 0x00, 0x93, 0xf3, 0xc1, 0x18, 0x01, 0xe6,
      0x62, 0xb6, 0xd1, 0x46, 0x9a, 0x2c, 0x72, 0xea, 0x31, 0xd9, 0x18, 0x10,
      0x30, 0x28, 0x63, 0xe2, 0x34, 0x7d, 0x80, 0xca, 0xee, 0x82, 0x2b, 0x19,
      0x3c, 0x19, 0xbb, 0x42, 0x83, 0x02, 0x70, 0xdd, 0xdb, 0x8c, 0x03, 0xab,
      0xe9, 0x9c, 0xc4, 0x00, 0x4d, 0x70, 0x5f, 0x52, 0x03, 0x31, 0x2c, 0xa4,
      0x67, 0x34, 0x51, 0x95, 0x2a, 0xac, 0x11, 0xe2, 0x6a, 0x55, 0x02, 0x40,
      0x44, 0xc8, 0x10, 0x53, 0x44, 0x32, 0x31, 0x63, 0xd8, 0xd1, 0x8c, 0x75,
      0xc8, 0x98, 0x53, 0x3b, 0x5b, 0x4a, 0x2a, 0x0a, 0x09, 0xe7, 0xd0, 0x3c,
      0x53, 0x72, 0xa8, 0x6b, 0x70, 0x41, 0x9c, 0x26, 0x71, 0x44, 0xfc, 0x7f,
      0x08, 0x75, 0xe1, 0x02, 0xab, 0x74, 0x41, 0xe8, 0x2a, 0x3d, 0x3c, 0x26,
      0x33, 0x09, 0xe4, 0x8b, 0xb4, 0x41, 0xec, 0xa6, 0xa8, 0xba, 0x1a, 0x07,
      0x8a, 0x77, 0xf5, 0x5f, 0x02, 0x02, 0x00, 0xa0,
  };

  CBS_init(&cbs, kParamsDSA, sizeof(kParamsDSA));
  dh.reset(DH_parse_parameters(&cbs));
  if (!dh || CBS_len(&cbs) != 0 ||
      !BIGNUMEqualsHex(dh->p,
                       "93f3c11801e662b6d1469a2c72ea31d91810302863e2347d80caee8"
                       "22b193c19bb42830270dddb8c03abe99cc4004d705f5203312ca467"
                       "3451952aac11e26a55") ||
      !BIGNUMEqualsHex(dh->g,
                       "44c8105344323163d8d18c75c898533b5b4a2a0a09e7d03c5372a86"
                       "b70419c267144fc7f0875e102ab7441e82a3d3c263309e48bb441ec"
                       "a6a8ba1a078a77f55f") ||
      dh->priv_length != 160) {
    return false;
  }

  if (!CBB_init(cbb.get(), 0) ||
      !DH_marshal_parameters(cbb.get(), dh.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  ScopedOpenSSLBytes free_der2(der);
  if (der_len != sizeof(kParamsDSA) || memcmp(der, kParamsDSA, der_len) != 0) {
    return false;
  }

  return true;
}

}  // namespace bssl

int main() {
  return bssl::Main();
}
