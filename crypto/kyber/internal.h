/* Copyright (c) 2023, Google Inc.
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

#ifndef OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


struct BORINGSSL_keccak_st {
  union {
    uint64_t u64[25];
    uint8_t u8[200];
  } state;
  size_t rate_bytes;
  size_t offset;
};

enum boringssl_keccak_config_t {
  boringssl_sha3_256,
  boringssl_sha3_512,
  boringssl_shake128,
  boringssl_shake256,
};

// BORINGSSL_keccak hashes |in_len| bytes from |in| and writes |out_len| bytes
// of output to |out|. If the |config| specifies a fixed-output function, like
// SHA3-256, then |out_len| must be the correct length for that function.
OPENSSL_EXPORT void BORINGSSL_keccak(uint8_t *out, size_t out_len,
                                     const uint8_t *in, size_t in_len,
                                     enum boringssl_keccak_config_t config);

// BORINGSSL_keccak_init absorbs |in_len| bytes from |in| and sets up |ctx| for
// squeezing. The |config| must specify a SHAKE variant, otherwise callers
// should use |BORINGSSL_keccak|.
OPENSSL_EXPORT void BORINGSSL_keccak_init(
    struct BORINGSSL_keccak_st *ctx, const uint8_t *in, size_t in_len,
    enum boringssl_keccak_config_t config);

// BORINGSSL_keccak_squeeze writes |out_len| bytes to |out| from |ctx|.
OPENSSL_EXPORT void BORINGSSL_keccak_squeeze(
    struct BORINGSSL_keccak_st *ctx, uint8_t *out, size_t out_len);


#if defined(__cplusplus)
}
#endif

#endif  // OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H
