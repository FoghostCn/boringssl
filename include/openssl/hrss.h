/* Copyright (c) 2018, Google Inc.
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

#ifndef OPENSSL_HEADER_HRSS_H
#define OPENSSL_HEADER_HRSS_H

#include <openssl/base.h>

#if (defined(OPENSSL_AARCH64) || defined(OPENSSL_ARM) || \
     defined(OPENSSL_X86) || defined(OPENSSL_X86_64)) && \
    !defined(_MSC_VER)
#include <stdalign.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// HRSS
//
// HRSS is a structured-lattice-based post-quantum key encapsulation mechanism.
// The best exposition is https://eprint.iacr.org/2017/1005 although this
// implementation uses a different KEM construction based on
// https://eprint.iacr.org/2017/1005.pdf.

struct HRSS_private_key {
  union {
    uint64_t alignment;
    uint8_t opaque[1794];
  } u;
};

struct HRSS_public_key {
  union {
    uint64_t alignment;
    uint8_t opaque[1416];
  } u;
};

// HRSS_SAMPLE_BYTES is the number of bytes of entropy needed to generate a
// short vector. There are 701 coefficients, but the final one is always set to
// zero when sampling. Otherwise, one byte of input is enough to generate two
// coefficients.
#define HRSS_SAMPLE_BYTES ((701 - 1) / 2)
// HRSS_GENERATE_KEY_BYTES is the number of bytes of entropy needed to generate
// an HRSS key pair.
#define HRSS_GENERATE_KEY_BYTES (HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES + 32)
// HRSS_ENCAP_BYTES is the number of bytes of entropy needed to encapsulate a
// session key.
#define HRSS_ENCAP_BYTES (HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES)
// HRSS_PUBLIC_KEY_BYTES is the number of bytes in a public key.
#define HRSS_PUBLIC_KEY_BYTES 1138
// HRSS_CIPHERTEXT_BYTES is the number of bytes in a ciphertext.
#define HRSS_CIPHERTEXT_BYTES (1138 + 32)
// HRSS_KEY_BYTES is the number of bytes in a shared key.
#define HRSS_KEY_BYTES 32
// HRSS_POLY3_BYTES is the number of bytes needed to serialise a mod 3
// polynomial.
#define HRSS_POLY3_BYTES 140
#define HRSS_PRIVATE_KEY_BYTES \
  (HRSS_POLY3_BYTES * 2 + HRSS_PUBLIC_KEY_BYTES + 2 + 32)

// HRSS_generate_key is a deterministic function that ouptuts a public and
// private key based on the given entropy.
OPENSSL_EXPORT void HRSS_generate_key(
    struct HRSS_public_key *out_pub, struct HRSS_private_key *out_priv,
    const uint8_t input[HRSS_GENERATE_KEY_BYTES]);

// HRSS_encap is a deterministic function the generates and encrypts a random
// session key from the given entropy, writing those values to |out_shared_key|
// and |out_ciphertext|, respectively.
OPENSSL_EXPORT void HRSS_encap(uint8_t out_ciphertext[HRSS_CIPHERTEXT_BYTES],
                               uint8_t out_shared_key[HRSS_KEY_BYTES],
                               const struct HRSS_public_key *in_pub,
                               const uint8_t in[HRSS_ENCAP_BYTES]);

// HRSS_decap decrypts a session key from |ciphertext_len| bytes of
// |ciphertext|. If the ciphertext is valid, the decrypted key is written to
// |out_shared_key|. Otherwise the HMAC of |ciphertext| under a secret key (kept
// in |in_priv|) is written. If the ciphertext is the wrong length then it will
// leak which was done via side-channels. Otherwise it should perform either
// action in constant-time.
OPENSSL_EXPORT void HRSS_decap(uint8_t out_shared_key[HRSS_KEY_BYTES],
                               const struct HRSS_public_key *in_pub,
                               const struct HRSS_private_key *in_priv,
                               const uint8_t *ciphertext,
                               size_t ciphertext_len);

OPENSSL_EXPORT void HRSS_marshal_public_key(
    uint8_t out[HRSS_PUBLIC_KEY_BYTES], const struct HRSS_public_key *in_pub);

OPENSSL_EXPORT int HRSS_parse_public_key(
    struct HRSS_public_key *out, const uint8_t in[HRSS_PUBLIC_KEY_BYTES]);

OPENSSL_EXPORT void HRSS_marshal_private_key(
    uint8_t out[HRSS_PRIVATE_KEY_BYTES],
    const struct HRSS_private_key *in_priv);

OPENSSL_EXPORT int HRSS_parse_private_key(
    struct HRSS_private_key *out_priv,
    const uint8_t in[HRSS_PRIVATE_KEY_BYTES]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_HRSS_H
