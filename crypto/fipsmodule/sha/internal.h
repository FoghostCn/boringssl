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

#ifndef OPENSSL_HEADER_SHA_INTERNAL_H
#define OPENSSL_HEADER_SHA_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


#if !defined(OPENSSL_NO_ASM)

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64) || defined(OPENSSL_ARM) || \
    defined(OPENSSL_AARCH64) || defined(OPENSSL_PPC64LE)
#define SHA1_ASM
void sha1_block_data_order(uint32_t *state, const uint8_t *in,
                           size_t num_blocks);
#endif

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64) || defined(OPENSSL_ARM) || \
    defined(OPENSSL_AARCH64)
#define SHA256_ASM
#define SHA512_ASM
void sha256_block_data_order(uint32_t *state, const uint8_t *in,
                             size_t num_blocks);
void sha512_block_data_order(uint64_t *state, const uint8_t *in,
                             size_t num_blocks);
#endif

#elif defined(OPENSSL_PPC64LE)

// POWER has an intrinsics-based implementation of SHA-1 and thus the functions
// normally defined in assembly are available even with |OPENSSL_NO_ASM| in
// this case.
#define SHA1_ASM

#endif  // NO_ASM


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // OPENSSL_HEADER_SHA_INTERNAL_H
