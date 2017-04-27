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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_CAVP_TEST_UTIL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_CAVP_TEST_UTIL_H

#include <stdlib.h>
#include <string>
#include <vector>

#include <openssl/aead.h>
#include <openssl/cipher.h>


std::string EncodeHex(const uint8_t *in, size_t in_len);

const EVP_CIPHER *GetCipher(const std::string &name);

bool CipherOperation(const EVP_CIPHER *cipher, bool encrypt,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &iv,
                     const std::vector<uint8_t> &in, std::vector<uint8_t> *out);

bool AEADEncrypt(const EVP_AEAD *aead, const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &pt,
                 const std::vector<uint8_t> &aad, std::vector<uint8_t> *iv,
                 std::vector<uint8_t> *tag, size_t tag_len,
                 std::vector<uint8_t> *ct);

bool AEADDecrypt(const EVP_AEAD *aead, const std::vector<uint8_t> &key,
                 const std::vector<uint8_t> &ct,
                 const std::vector<uint8_t> &tag, std::vector<uint8_t> &iv,
                 std::vector<uint8_t> *aad, size_t aad_len,
                 std::vector<uint8_t> *pt, size_t pt_len);


#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_CAVP_TEST_UTIL_H
