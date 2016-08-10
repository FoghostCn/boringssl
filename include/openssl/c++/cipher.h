/* Copyright (c) 2016, Google Inc.
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

#ifndef OPENSSL_HEADER_CXX_CIPHER_H
#define OPENSSL_HEADER_CXX_CIPHER_H

#include <openssl/cipher.h>
#include <openssl/c++/scoped_helpers.h>

namespace bssl {

using ScopedEVP_CIPHER_CTX =
    ScopedContext<EVP_CIPHER_CTX, int, EVP_CIPHER_CTX_init,
                  EVP_CIPHER_CTX_cleanup>;

}  // namespace bssl

#endif /* OPENSSL_HEADER_CXX_CIPHER_H */
