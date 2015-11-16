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

#ifndef OPENSSL_HEADER_CURVE25519_H
#define OPENSSL_HEADER_CURVE25519_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Ed25519.
 *
 * Ed25519 is a signature scheme using a twisted-Edwards curve that is
 * birationally equivalent to curve25519. */

/* ED25519_keypair sets |out_public_key| and |out_private_key| to a freshly
 * generated, public–private key pair. */
OPENSSL_EXPORT void ED25519_keypair(uint8_t out_public_key[32],
                                    uint8_t out_private_key[64]);

/* ED25519_sign sets |out_sig| to be a signature of |message_len| bytes from
 * |message| using |private_key|. It returns one on success or zero on
 * error. */
OPENSSL_EXPORT int ED25519_sign(uint8_t out_sig[64], const uint8_t *message,
                                size_t message_len,
                                const uint8_t private_key[64]);

/* ED25519_verify returns one iff |signature| is a valid signature, by
 * |public_key| of |message_len| bytes from |message|. */
OPENSSL_EXPORT int ED25519_verify(const uint8_t *message, size_t message_len,
                                  const uint8_t signature[64],
                                  const uint8_t public_key[32]);

void CURVE25519_scalar_base_mult(uint8_t out[32], const uint8_t scalar[32]);

void CURVE25519_scalar_mult(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);

#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_CMAC_H */
